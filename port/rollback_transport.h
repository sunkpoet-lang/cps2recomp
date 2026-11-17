// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy and the fork's contributors.
// Extracted from port/online.cpp; legacy two-player NGO transport, no backend types.
#pragma once
#include "udp_transport.h"
#include "epoch_recovery.h"
#include <gekkonet.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
namespace recomp::net {
struct PacketSocket {
    inline static constexpr char kTag[3] = {'N', 'G', 'O'};
    inline static constexpr char kPeer[] = "peer";
    static uint64_t NowMs() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    recomp::net::Udp transport;
    sockaddr_in peer{};
    bool havePeer = false;
    bool host = false;
    bool recovery = false;
    uint64_t room=0,epoch=1,lastReceived=0,stalePackets=0;
    std::vector<std::pair<char,std::vector<uint8_t>>> controls;
    std::deque<std::vector<uint8_t>> forGekko;   // GekkoNet's packets from the peer, until it asks
    std::vector<GekkoNetResult *> handedOver;    // (what receive_data last returned: the array is ours)
    std::vector<std::pair<sockaddr_in, std::vector<uint8_t>>> hellos;
    std::vector<std::pair<char, std::vector<uint8_t>>> forChat; // (this fork: the chat's C and A, from the peer)
    // (this fork: spectators) the host's: each spectator's address and the name GekkoNet knows it by ("spec1" ...);
    // their hellos as they come; the spectator's: the host's refusal
    std::vector<std::pair<sockaddr_in, std::string>> watchers;
    std::vector<std::pair<sockaddr_in, std::vector<uint8_t>>> watchHellos;
    std::deque<std::string> forGekkoFrom; // (who each of forGekko's packets came from)
    std::string refused;
    std::map<std::string, uint64_t> watcherHeard; // (the host's: when each spectator was last heard)
    std::vector<uint8_t> status;                   // (the host's last Q, as the others got it)
    uint64_t statusMs = 0;
    uint64_t sentToPeer = 0, sentToWatchers = 0, packetsToWatchers = 0; // (bytes sent, for the measurements)
    // (this fork: spectators' chat) the relay's V and U from the host; at the host, each spectator's C, A and U
    std::vector<std::pair<char, std::vector<uint8_t>>> forRelay;
    std::vector<std::tuple<std::string, char, std::vector<uint8_t>>> fromWatchers;
    // (--chat-loss: the chat's packets dropped at random as sent, as a bad network would)
    int chatLoss = 0;
    uint32_t lossSeed = 12345;
    bool ChatLost() {
        lossSeed = lossSeed * 1103515245u + 12345u;
        return static_cast<int>((lossSeed >> 16) % 100) < chatLoss;
    }

    const std::string *WatcherName(const sockaddr_in &from) const {
        for (const auto &[at, name] : watchers) {
            if (Same(at, from)) {
                return &name;
            }
        }
        return nullptr;
    }
    const sockaddr_in *WatcherAt(const char *name, size_t size) const {
        for (const auto &[at, one] : watchers) {
            if (one.size() == size && !memcmp(one.data(), name, size)) {
                return &at;
            }
        }
        return nullptr;
    }

    bool Open(uint16_t port, std::string& error) {
        return transport.Open(port, error);
    }

    bool Resolve(const std::string& host, uint16_t port, std::string& error) {
        if (!transport.Resolve(host, port, peer, error)) return false;
        havePeer = true;
        return true;
    }

    void Send(char kind, const void *data, size_t size, const sockaddr_in &to) {
        std::vector<uint8_t> wrapped;
        if(recovery && (kind=='G' || kind=='C' || kind=='A')) {
            if(!room)return;
            Recovery::Put(wrapped,room);Recovery::Put(wrapped,epoch);
            if(size){const auto* first=static_cast<const uint8_t*>(data);wrapped.insert(wrapped.end(),first,first+size);}
            data=wrapped.data();size=wrapped.size();
        }
        std::vector<uint8_t> packet(4 + size);
        memcpy(packet.data(), kTag, 3);
        packet[3] = static_cast<uint8_t>(kind);
        if (size) {
            memcpy(packet.data() + 4, data, size);
        }
        transport.Send(packet, to);
        if (havePeer && Same(to, peer)) {
            sentToPeer += packet.size();
        } else {
            sentToWatchers += packet.size();
            packetsToWatchers++;
        }
    }

    static bool Same(const sockaddr_in &a, const sockaddr_in &b) {
        return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
    }

    // (this fork, online-abuse-protection) A refusal ('X', why) to someone this session will not take: at most one a
    // second to each address, so the session cannot be used to send a flood at a third party's address (spoofed
    // hellos); at most 512 addresses remembered
    std::map<uint64_t, uint64_t> refusedAt;
    void Refuse(const std::string &why, const sockaddr_in &to) {
        const uint64_t key = (static_cast<uint64_t>(to.sin_addr.s_addr) << 16) | to.sin_port, now = NowMs();
        const auto at = refusedAt.find(key);
        if (at != refusedAt.end() && now - at->second < 1000) {
            return;
        }
        if (refusedAt.size() >= 512) {
            refusedAt.clear();
        }
        refusedAt[key] = now;
        Send('X', why.data(), why.size(), to);
    }

    // All payloads in an opted-in session are tied to the pinned peer/room/epoch.
    void Pump() {
        uint8_t buffer[65536];
        while(true) {
            sockaddr_in from{};
            const auto received = transport.Receive(buffer, from);
            if (received.kind == recomp::net::ReceiveKind::PeerUnreachable) continue;
            if (received.kind != recomp::net::ReceiveKind::Packet) break;
            const int got = static_cast<int>(received.size);
            if(got<4 || memcmp(buffer,kTag,3)!=0)continue;
            const char kind=static_cast<char>(buffer[3]);
            if(kind=='H'){if(hellos.size()<64)hellos.emplace_back(from,std::vector<uint8_t>(buffer+4,buffer+got));continue;}
            if(kind=='W'){if(host && watchHellos.size()<64)watchHellos.emplace_back(from,std::vector<uint8_t>(buffer+4,buffer+got));continue;}
            if(kind=='X'){if(!host && havePeer && Same(from,peer))refused.assign(reinterpret_cast<const char*>(buffer+4),static_cast<size_t>(got-4));continue;}
            if(kind=='Q'){if(!host && havePeer && Same(from,peer) && got>=24){status.assign(buffer+4,buffer+got);statusMs=NowMs();}continue;}
            // (a spectator's GekkoNet packets, to the host, with the room and epoch when the match has them: nothing
            // else of theirs is taken)
            if(host && (kind=='G' || kind=='C' || kind=='A' || kind=='U')) {
                if(const std::string* name=WatcherName(from)) {
                    const uint8_t* bytes=buffer+4;size_t count=static_cast<size_t>(got-4);
                    if(recovery && kind!='U') {
                        if(count<16 || !room || Recovery::Get(bytes)!=room)continue;
                        if(Recovery::Get(bytes+8)!=epoch){++stalePackets;continue;}
                        bytes+=16;count-=16;
                    }
                    watcherHeard[*name]=NowMs();
                    if(kind=='G'){forGekko.emplace_back(bytes,bytes+count);forGekkoFrom.push_back(*name);}
                    else if(fromWatchers.size()<256)fromWatchers.emplace_back(*name,kind,std::vector<uint8_t>(bytes,bytes+count));
                    continue;
                }
            }
            if(!havePeer || !Same(from,peer))continue;
            if(recovery && std::strchr("NRKSTPL",kind) && got==20) {
                if(controls.size()<64)controls.emplace_back(kind,std::vector<uint8_t>(buffer+4,buffer+got));
                continue;
            }
            if(kind=='V' || kind=='U'){if(forRelay.size()<256)forRelay.emplace_back(kind,std::vector<uint8_t>(buffer+4,buffer+got));continue;}
            if(kind!='G' && kind!='C' && kind!='A')continue;
            const uint8_t* payload=buffer+4;size_t size=static_cast<size_t>(got-4);
            if(recovery) {
                if(size<16 || !room || Recovery::Get(payload)!=room)continue;
                if(Recovery::Get(payload+8)!=epoch){++stalePackets;continue;}
                payload+=16;size-=16;
            }
            lastReceived=NowMs();
            if(kind=='G'){forGekko.emplace_back(payload,payload+size);forGekkoFrom.push_back(kPeer);}
            else forChat.emplace_back(kind,std::vector<uint8_t>(payload,payload+size));
        }
    }

};

// Gekko's adapter callbacks carry no user context: preserve one active socket.
struct PacketAdapter {
    inline static PacketSocket* active = nullptr;
    static void SendData(GekkoNetAddress *addr, const char *data, int length) {
        if (!active) {
            return;
        }
        // (this fork: spectators) the address GekkoNet gives is a name: the peer's, or a spectator's
        if (addr && addr->data && active->host) {
            if (const sockaddr_in *at = active->WatcherAt(static_cast<const char *>(addr->data), addr->size)) {
                active->Send('G', data, static_cast<size_t>(length), *at);
                return;
            }
        }
        if (active->havePeer) {
            active->Send('G', data, static_cast<size_t>(length), active->peer);
        }
    }

    static GekkoNetResult **ReceiveData(int *length) {
        active->handedOver.clear();
        while (!active->forGekko.empty()) {
            std::vector<uint8_t> &packet = active->forGekko.front();
            auto *result = static_cast<GekkoNetResult *>(malloc(sizeof(GekkoNetResult)));
            const std::string from = active->forGekkoFrom.empty() ? std::string(PacketSocket::kPeer) : active->forGekkoFrom.front();
            if (!active->forGekkoFrom.empty()) {
                active->forGekkoFrom.pop_front();
            }
            result->addr.size = static_cast<unsigned int>(from.size());
            result->addr.data = malloc(result->addr.size);
            memcpy(result->addr.data, from.data(), result->addr.size);
            result->data_len = static_cast<unsigned int>(packet.size());
            result->data = malloc(packet.size() ? packet.size() : 1);
            if (!packet.empty()) {
                memcpy(result->data, packet.data(), packet.size());
            }
            active->handedOver.push_back(result);
            active->forGekko.pop_front();
        }
        *length = static_cast<int>(active->handedOver.size());
        return active->handedOver.data();
    }

    static void FreeData(void *data) {
        free(data);
    }

    inline static GekkoNetAdapter callbacks{SendData, ReceiveData, FreeData};
};
} // namespace recomp::net
