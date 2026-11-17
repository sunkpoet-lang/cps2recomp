// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy and the fork's contributors. Part of cps2recomp (this fork), from neogeo-recomp's
// port/neogeo_online.cpp.
#include "online.h"
#include "rollback_session.h"
#include "udp_transport.h"

#include "game_info.h"
#include "../fight/battle.h"

#include "../machine/machine.h"
#include "../runtime/game.h"
#include "../runtime/state.h"
#include "port.h"
#include "../recomp-common/chat.h"

#include <gekkonet.h>
#include <zlib.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <charconv>
#include <random>
#include <deque>
#include <functional>
#include <map>
#include <tuple>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#ifndef CPS2RECOMP_VERSION
#define CPS2RECOMP_VERSION "0"
#endif

namespace port {

using cps2recomp::Archive;
using cps2recomp::Machine;
using cps2recomp::TheGame;

namespace {
    // Every packet starts with these three bytes and a kind: H, hello (before GekkoNet's own); G, GekkoNet's;
    // (this fork) W, a spectator's hello; X, the host's refusal, with its reason; Q, the host's status, to the other
    // player and each spectator four times a second: the room and epoch (big-endian), the phase, how many watch, the
    // two streaks, then the players' names, each ended by a 0 byte
    constexpr char kTag[3] = {'N', 'G', 'O'};
    constexpr int kMostSpectators = 6; // (the tunnel's eight guests: the opponent, six spectators, one kept free)

    // (this fork: the chat's clock, in milliseconds)
    uint64_t NowMs() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    constexpr int kPredictionWindow = 8;
    constexpr unsigned kTimeoutMs = 5000; // (under the tunnel's 8 s without traffic)
    // The address GekkoNet is given for the other player: the socket knows where that is
    constexpr char kPeer[] = "peer";
    constexpr uint32_t kStateMark = 0x4F4E4C31; // "ONL1"

    // The state the session keeps: the save state's (the machine and the game's sources' own)
    void SaveTo(std::vector<uint8_t> &data, Machine &machine) {
        data.clear();
        Archive ar(data, true);
        ar.Mark(kStateMark);
        machine.Serialize(ar);
        if (TheGame().state) {
            TheGame().state(ar);
        }
    }
    bool LoadFrom(std::vector<uint8_t> &data, Machine &machine) {
        Archive ar(data, false);
        ar.Mark(kStateMark);
        machine.Serialize(ar);
        if (TheGame().state) {
            TheGame().state(ar);
        }
        return ar.Ok();
    }

    // The features that change the game, as both players must have them (the Display page's, but for the widescreen,
    // which changes the fights, and what online play turns off are each player's own): "name=value" for each
    std::vector<std::string> GameFeatures(bool matchFlow) {
        std::vector<std::string> list;
        for (const cps2recomp::Feature &feature : TheGame().features) {
            const bool own = !strcmp(feature.category, "Display") && strncmp(feature.name, "widescreen", 10) != 0;
            // ((this fork) Mute spectators: what each player sees, never part of the match)
            if (own || cps2recomp::OnlineOff(feature.name)) {
                continue;
            }
            list.push_back(std::string(feature.name) + "=" + std::to_string(matchFlow && !strcmp(feature.name,"online_hide_names") ? 0 : cps2recomp::FeatureSetting(feature.name)));
        }
        return list;
    }

    // The features two hellos have set differently, by their titles
    std::string Differing(const std::string &theirs, const std::string &ours) {
        auto split = [](const std::string &text) {
            std::vector<std::string> list;
            for (size_t at = 0; at < text.size();) {
                const size_t comma = text.find(',', at);
                list.push_back(text.substr(at, comma == std::string::npos ? std::string::npos : comma - at));
                at = comma == std::string::npos ? text.size() : comma + 1;
            }
            return list;
        };
        const std::vector<std::string> a = split(theirs), b = split(ours);
        std::string names;
        for (const std::string &one : b) {
            if (std::find(a.begin(), a.end(), one) != a.end()) {
                continue;
            }
            const std::string name = one.substr(0, one.find('='));
            for (const cps2recomp::Feature &feature : TheGame().features) {
                if (name == feature.name) {
                    names += std::string(names.empty() ? "" : ", ") + feature.title;
                }
            }
        }
        return names.empty() ? "the features" : names;
    }

    uint32_t Crc(uint32_t crc, const std::vector<uint8_t> &data) {
        // (zlib takes at most 4 GB a call; a set is far less)
        return static_cast<uint32_t>(crc32(crc, data.data(), static_cast<uInt>(data.size())));
    }
} // namespace

// --- (this fork) the spectators' chat relay ---
//
// The players' own chat (C and A) is unchanged. With spectators, the host relays every line to whoever did not say
// it, on a channel of its own to each of the others: V, a line with who said it (its number, little-endian like the
// chat's; the role, 0 the host, 1 the other player, 2 a spectator; the name's length and the name; the text's length,
// two bytes, and the text), and U, how many have come (the chat's A). Lines are resent every quarter second until
// acknowledged, and delivered once and in order (docs/chat-protocol.md, Spectators).
class ChatRelay {
public:
    using Send = std::function<void(char kind, const std::vector<uint8_t> &payload)>;
    struct Line {
        int role = 0;
        std::string name, text;
    };
    explicit ChatRelay(Send send) : m_send(std::move(send)) {}

    void Push(const Line &line, uint64_t now) {
        std::vector<uint8_t> payload(4);
        const uint32_t number = m_next++;
        for (int i = 0; i < 4; i++) {
            payload[i] = static_cast<uint8_t>(number >> (8 * i));
        }
        const std::string name = line.name.substr(0, 255);
        payload.push_back(static_cast<uint8_t>(line.role));
        payload.push_back(static_cast<uint8_t>(name.size()));
        payload.insert(payload.end(), name.begin(), name.end());
        const std::string text = line.text.substr(0, 4096);
        payload.push_back(static_cast<uint8_t>(text.size()));
        payload.push_back(static_cast<uint8_t>(text.size() >> 8));
        payload.insert(payload.end(), text.begin(), text.end());
        m_out.emplace_back(number, std::move(payload));
        m_send('V', m_out.back().second);
        m_sentAt = now;
    }
    void Receive(char kind, const uint8_t *data, size_t size, uint64_t now) {
        (void)now;
        if (kind == 'U') {
            uint32_t count = 0;
            if (!recomp::chat::DecodeAck(data, size, count)) {
                return;
            }
            while (!m_out.empty() && m_out.front().first < count) {
                m_out.pop_front();
            }
            return;
        }
        if (kind != 'V' || size < 8) {
            return;
        }
        const uint32_t number = data[0] | data[1] << 8 | data[2] << 16 | static_cast<uint32_t>(data[3]) << 24;
        const int role = data[4];
        const size_t nameSize = data[5];
        if (6 + nameSize + 2 > size) {
            return;
        }
        const size_t textSize = data[6 + nameSize] | data[7 + nameSize] << 8;
        if (8 + nameSize + textSize > size) {
            return;
        }
        if (number == m_heardUpTo) {
            Line line;
            line.role = role;
            recomp::chat::Clean(std::string(reinterpret_cast<const char *>(data + 6), nameSize), recomp::chat::kNameMost, line.name);
            recomp::chat::Clean(std::string(reinterpret_cast<const char *>(data + 8 + nameSize), textSize), recomp::chat::kLineMost,
                                line.text);
            if (!line.text.empty()) {
                m_heard.push_back(std::move(line));
            }
            m_heardUpTo++;
        }
        m_send('U', recomp::chat::EncodeAck(m_heardUpTo));
    }
    void Pump(uint64_t now) {
        if (!m_out.empty() && now - m_sentAt >= recomp::chat::kResendMs) {
            for (const auto &[number, payload] : m_out) {
                m_send('V', payload);
            }
            m_sentAt = now;
        }
    }
    std::vector<Line> Take() {
        std::vector<Line> lines;
        lines.swap(m_heard);
        return lines;
    }

private:
    Send m_send;
    std::deque<std::pair<uint32_t, std::vector<uint8_t>>> m_out;
    uint32_t m_next = 0, m_heardUpTo = 0;
    uint64_t m_sentAt = 0;
    std::vector<Line> m_heard;
};

// --- the socket, and GekkoNet's adapter on it ---

namespace {
    auto*& g_socket = recomp::net::PacketAdapter::active;
    auto& g_adapter = recomp::net::PacketAdapter::callbacks;
} // namespace

namespace { uint64_t FreshRecoveryNonce(); bool RecoveryHelloNonce(std::vector<uint8_t>&,uint64_t&); }

// --- the session ---

Online::~Online() {
    Stop();
}

void Online::Stop() {
    if(m_socket && ChatHost())for(int n=0;n<3;++n)SendStatus(4); // (this fork: the spectators told, and the other player)
    if(m_options.matchFlow && m_heard && m_socket)for(int n=0;n<3;++n)SendControl('L',Epoch());
    if(m_socket && m_socket->stalePackets)fprintf(stderr,"online: ignored stale epoch packets: %llu\n",static_cast<unsigned long long>(m_socket->stalePackets));
    for (const Spectator &one : m_spectators) {
        if (m_session && one.handle >= 0) {
            GekkoNetworkStats net{};
            gekko_network_stats(m_session.Get(), one.handle, &net);
            fprintf(stderr, "online: spectator %s: ping %u\n", one.name.c_str(), static_cast<unsigned>(net.last_ping));
        }
    }
    if (m_socket && m_options.mode == OnlineOptions::Mode::Host) {
        fprintf(stderr, "online: sent %.1f KB to the opponent, %.1f KB in %llu packets to spectators over %llu ticks\n",
                m_socket->sentToPeer / 1024.0, m_socket->sentToWatchers / 1024.0,
                static_cast<unsigned long long>(m_socket->packetsToWatchers), static_cast<unsigned long long>(m_ticks));
    }
    if(m_checkpointLog){std::fclose(m_checkpointLog);m_checkpointLog=nullptr;}
    if(m_flowLog){std::fclose(m_flowLog);m_flowLog=nullptr;}
    ReplayEnd();
    if (m_session) {
        m_session.Reset();
    }
    if (m_socket) {
        if (g_socket == m_socket) {
            g_socket = nullptr;
        }
        delete m_socket;
        m_socket = nullptr;
    }
    if (m_seats) {
        if (const auto *star = Star())
        fprintf(stderr,"online: seat routing: local %u; relayed %llu; sent %llu,%llu,%llu,%llu; received %llu,%llu,%llu,%llu; logical %llu,%llu,%llu,%llu\n",
            star->Primary(),static_cast<unsigned long long>(star->relayed),
            static_cast<unsigned long long>(star->sentTo[0]),static_cast<unsigned long long>(star->sentTo[1]),static_cast<unsigned long long>(star->sentTo[2]),static_cast<unsigned long long>(star->sentTo[3]),
            static_cast<unsigned long long>(star->receivedFrom[0]),static_cast<unsigned long long>(star->receivedFrom[1]),static_cast<unsigned long long>(star->receivedFrom[2]),static_cast<unsigned long long>(star->receivedFrom[3]),
            static_cast<unsigned long long>(star->logicalFrom[0]),static_cast<unsigned long long>(star->logicalFrom[1]),static_cast<unsigned long long>(star->logicalFrom[2]),static_cast<unsigned long long>(star->logicalFrom[3]));
        m_seats.reset(); // (its replay is kept as its session goes: the backend's ending)
    }
    if (m_port) {
        if (m_port->gekkoMatch == this) {
            m_port->gekkoMatch = nullptr;
        }
        TheGame().online = false;
        m_port->gekkoOnline = false;
        m_port->gekkoArcadeWaiting = false;
        if (m_port->gekkoChat == this) {
            m_port->gekkoChat = nullptr;
        }
    }
}

bool Online::Start(Port &port, const OnlineOptions &options, std::string &error) {
    if(options.epochs && !options.seats){error="Epoch framing requires --gekko-seats";return false;}
    if(options.lateJoins && (!options.epochs || options.mode!=OnlineOptions::Mode::Host)){error="Late joins need the host of an epoch-framed seat room";return false;}
    if (options.localPlayers<1 || options.localPlayers>2 || (!options.seats && options.localPlayers!=1)) { error="One or two locals require an experimental seat room"; return false; }
    if (options.seats && !port.opt.headless) { error = "Experimental seat rooms currently require --headless"; return false; }
    if (options.seats && ((options.mode != OnlineOptions::Mode::Host && options.mode != OnlineOptions::Mode::Join) || options.matchFlow || options.arcadeWait || options.spectate)) {
        error = "Experimental seat rooms do not yet support arcade flow or spectators"; return false;
    }
    if (port.Match()) {
        error = "An online session is already active";
        return false;
    }
    if(!options.checkpointLog.empty()) {
#ifdef _WIN32
        const std::filesystem::path path(std::u8string(reinterpret_cast<const char8_t*>(options.checkpointLog.c_str())));
        m_checkpointLog=_wfopen(path.c_str(),L"w");
#else
        m_checkpointLog=std::fopen(options.checkpointLog.c_str(),"w");
#endif
        if(!m_checkpointLog){error="The rollback checkpoint log could not be opened";return false;}
    }
    if(!options.flowLog.empty()) {
#ifdef _WIN32
        const std::filesystem::path path(std::u8string(reinterpret_cast<const char8_t*>(options.flowLog.c_str())));
        m_flowLog=_wfopen(path.c_str(),L"w");
#else
        m_flowLog=std::fopen(options.flowLog.c_str(),"w");
#endif
        if(!m_flowLog){error="The match presentation log could not be opened";return false;}
    }
    m_port = &port;
    m_options = options;
    m_players = 2;
    if (options.mode == OnlineOptions::Mode::Stress || options.seats) {
#ifdef GAME_PLAYERS
        const int configured = std::atoi(GAME_PLAYERS);
        if (configured < 2 || configured > 4) { error = "The game must declare two to four players"; return false; }
        m_players = static_cast<unsigned>(configured);
#endif
    }
    if(options.seats && options.mode==OnlineOptions::Mode::Host && options.localPlayers>=m_players) { error="Use offline play when all seats are at this PC"; return false; }
    if(options.startPlayers && (!options.epochs || options.startPlayers<2 || options.startPlayers>m_players || (options.mode==OnlineOptions::Mode::Host && options.startPlayers<=options.localPlayers))){error="The initial player threshold requires epochs, game capacity and a remote PC";return false;}
    m_actors.fill(-1);
    m_recovery.Reset(options.mode==OnlineOptions::Mode::Host,NowMs());m_prepared=m_everPlayed=false;
    if(options.matchFlow) {
        try{m_localNonce=FreshRecoveryNonce();}catch(...){error="A recovery session nonce could not be created";return false;}
    }
    m_options.delay = std::clamp(m_options.delay, 0, kPredictionWindow);
    port.gekkoOnline = true;
    port.gekkoArcadeWaiting = options.arcadeWait && options.mode == OnlineOptions::Mode::Host;
    if(options.matchFlow) { m_flow.Reset(); PublishFlow(); }
    TheGame().online = true;
    port.SoundtrackFollow(); // (the custom soundtrack off: the board's music)
    // (the fingerprints compared leave out the picture, as frames run again are not drawn, and the sound side, as
    // Tommy's netplay does: Port::StateHash)
    port.opt.hashNoPicture = true;
    port.opt.hashNoSound = true;
    Machine &machine = *port.machine;

    // The state GekkoNet keeps: the save state, with room to spare (its size hardly changes)
    SaveTo(m_state, machine);
    if(options.matchFlow || port.gekkoArcadeWaiting)m_arcadeState=m_state;
    m_stateSize = static_cast<uint32_t>(m_state.size() + m_state.size() / 4 + 4096);

    GekkoConfig config{};
    config.num_players = static_cast<unsigned char>(m_players);
    config.max_spectators = 0;
    config.max_input_prediction_window = kPredictionWindow;
    config.input_size = sizeof(uint16_t);
    config.state_size = m_stateSize;
    config.desync_detection = true;
    config.check_distance = 10;

    if (options.mode == OnlineOptions::Mode::Stress) {
        if (!m_session.Start(GekkoStressSession, config)) {
            error = "The rollback stress session could not start";
            return false;
        }
        for (unsigned seat = 0; seat < m_players; ++seat) {
            m_actors[seat] = m_session.AddActor(GekkoLocalPlayer, nullptr);
            if (m_actors[seat] < 0 || !m_session.Delay(m_actors[seat], static_cast<unsigned char>(m_options.delay))) {
                error = "A local rollback player could not start"; return false;
            }
        }
        m_local = m_actors[0]; m_remote = m_actors[1];
        ReplayBegin();
        fprintf(stderr,"online: stress input lanes: %u\n",m_players);
        m_status = Status::Playing;
        return true;
    }

    // The hello: the game, its set's checksum (the ROMs as loaded), the version and the features that change the game
    uint32_t crc = 0;
    for (const cps2recomp::RomSet *set : {&port.roms, &port.extraRoms}) {
        for (const std::vector<uint8_t> *data : {&set->program, &set->opcodes, &set->graphics, &set->soundProgram, &set->samples}) {
            crc = Crc(crc, *data);
        }
    }
    char hello[64] = {};
    snprintf(hello, sizeof(hello), "%s|%08x|%s|", GAME_ID, crc, CPS2RECOMP_VERSION);
    std::string text = hello;
    for (const std::string &one : GameFeatures(options.matchFlow)) {
        text += one + ",";
    }
    if(options.matchFlow)text+="|forkade-recovery-1";
    m_hello.assign(text.begin(), text.end());

    if (options.seats) return StartSeats(port, options, error);
    m_socket = new Socket;
    // (this fork) The in-game chat, on the session's socket
    Socket *socket = m_socket;
    socket->chatLoss = options.chatLoss;
    m_chat = std::make_unique<recomp::chat::Channel>([socket](char kind, const std::vector<uint8_t> &payload) {
        if (socket->havePeer && !socket->ChatLost()) {
            socket->Send(kind, payload.data(), payload.size(), socket->peer);
        }
    });
    // (this fork: spectators' chat) the host's relay to the other player, or this side's from the host
    m_relay = std::make_shared<ChatRelay>([socket](char kind, const std::vector<uint8_t> &payload) {
        if (socket->havePeer && !socket->ChatLost()) {
            socket->Send(kind, payload.data(), payload.size(), socket->peer);
        }
    });
    port.gekkoChat = this;
    g_socket = m_socket;
    const bool host = options.mode == OnlineOptions::Mode::Host;
    m_socket->host = host;m_socket->recovery=options.matchFlow;m_socket->room=host?m_localNonce:0;
    if (!m_socket->Open(host ? options.port : 0, error)) {
        return false;
    }
    if (!host && !m_socket->Resolve(options.host, options.port, error)) {
        return false;
    }
    if (Spectating()) {
        // (this fork) A spectator: its session is made once the host has answered
        m_status = Status::Waiting;
        m_message = "Calling " + options.host + ":" + std::to_string(options.port) + " to watch";
        port.gekkoMatch = this;
        fprintf(stderr, "online: %s\n", m_message.c_str());
        return true;
    }
    if(!CreateGameSession()){error="The rollback session could not start";return false;}
    m_status = Status::Waiting;
    m_message = host && options.arcadeWait ? "WAITING FOR A CHALLENGER" : host ? "Waiting for the other player (port " + std::to_string(options.port) + ")"
                     : "Calling " + options.host + ":" + std::to_string(options.port);
    port.gekkoMatch = this;
    fprintf(stderr, "online: %s\n", m_message.c_str());
    return true;
}

namespace {
uint64_t FreshRecoveryNonce() {
    std::random_device random;
    const auto value=(static_cast<uint64_t>(random())<<32)^static_cast<uint64_t>(random());
    return value?value:1;
}
bool RecoveryHelloNonce(std::vector<uint8_t>& hello,uint64_t& nonce) {
    const std::string text(hello.begin(),hello.end());
    const auto colon=text.rfind(':');
    if(colon==std::string::npos || text.size()-colon-1!=16)return false;
    const std::string base=text.substr(0,colon);
    if(!base.ends_with("|forkade-recovery-1"))return false;
    const auto parsed=std::from_chars(text.data()+colon+1,text.data()+text.size(),nonce,16);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || !nonce)return false;
    hello.assign(base.begin(),base.end());return true;
}
}

bool Online::StartSeats(Port &port, const OnlineOptions &options, std::string &error) {
    Machine &machine = *port.machine;
    recomp::match::Options o;
    o.host = options.mode == OnlineOptions::Mode::Host; o.port = static_cast<uint16_t>(options.port); o.address = options.host;
    o.capacity = m_players; o.locals = static_cast<unsigned>(options.localPlayers); o.startPlayers = static_cast<unsigned>(options.startPlayers);
    o.lateJoins = options.lateJoins; o.epochs = options.epochs;
    o.name = options.name.empty() ? "Player" : recomp::chat::Printable(options.name, recomp::chat::kNameMost);
    o.profile = m_hello; o.build = port.BuildIdentity();
    o.delay = static_cast<unsigned>(m_options.delay); o.prediction = kPredictionWindow; o.timeoutMs = kTimeoutMs;
    o.stateBytes = m_stateSize; o.scope = recomp::net::BindScope::Any; o.checkpointLog = m_checkpointLog;

    recomp::match::Backend b;
    b.save = [&machine](std::vector<uint8_t> &state) { SaveTo(state, machine); return !state.empty(); };
    b.load = [&machine](std::span<const uint8_t> bytes) { std::vector<uint8_t> data(bytes.begin(), bytes.end()); return LoadFrom(data, machine); };
    b.frame = [this, &machine](std::span<const uint16_t> lanes, bool again) {
        // (a frame run again is neither seen nor heard: its sound is dropped)
        const size_t heard = machine.audio.size();
        if (!m_port->RunOnlineFrame(lanes, again)) return false;
        if (again) machine.audio.resize(heard);
        return true;
    };
    b.hash = [this] { return m_port->StateHash(); };
    b.begun = [this] {
        // (the session's replay: its recording the seat match started)
        m_replay.reset(); m_replayRan.clear(); m_replayChecks.clear();
        m_replayBase = m_port->opt.replayPlay.empty() ? m_seats->RecordingBase() : 0;
        m_heard = true;
        if (!m_flowStarted) { m_flow.Reset(); PublishFlow(); m_flowStarted = true; } else m_flow.Rebase();
    };
    b.saved = [this](int frame) {
        m_flow.Save(frame);
        if (m_flowLog) {
            const auto &s = m_flow.state;
            std::fprintf(m_flowLog, "%d %d %d %d %d %d %d %d %d\n", frame, int(s.joined[0]), int(s.joined[1]), s.streak[0], s.streak[1], s.rounds[0], s.rounds[1], int(s.counted), s.quiet);
        }
        return true;
    };
    b.loaded = [this](int frame) { if (!m_flow.Load(frame)) return false; PublishFlow(); return true; };
    b.before = [this](int frame, std::span<const uint16_t> lanes) {
        Replay::Pads players{}; std::copy(lanes.begin(), lanes.end(), players.begin());
        ReplayAdvance(frame, players, true);
    };
    b.after = [this](int frame, std::span<const uint16_t> lanes, bool again) {
        Replay::Pads players{}; std::copy(lanes.begin(), lanes.end(), players.begin());
        ReplayAdvance(frame, players, false);
        const bool before = m_port->cameIn;
        const auto result = TheGame().onlineMatchResult;
        m_flow.Advance(players.data(), cps2recomp::kStart, result ? result() : cps2recomp::battle::RoundResultNow(), result ? 1 : cps2recomp::battle::RoundsToWin(), m_players);
        PublishFlow();
        if (!again && !before && m_port->cameIn) fprintf(stderr, "online: match flow: local Start received\n");
    };
    b.ending = [this](uint64_t frames) {
        m_replayCap = frames > SIZE_MAX ? SIZE_MAX : static_cast<size_t>(frames);
        ReplayEnd();
        m_replayCap = SIZE_MAX;
    };
    b.applied = [&machine] { machine.audio.clear(); };

    m_seats = std::make_unique<recomp::match::SeatMatch>();
    m_flowStarted = false;
    if (!m_seats->Start(o, std::move(b), error)) { m_seats.reset(); return false; }
    m_status = Status::Waiting;
    m_message = "Waiting for " + std::to_string(m_players) + " players (experimental seats)";
    port.gekkoMatch = this;
    fprintf(stderr, "online: seat transport waiting: capacity %u\n", m_players);
    return true;
}

int Online::TickSeats(std::span<const uint16_t> local) {
    const int shown = m_seats->Tick(local);
    using Seat = recomp::match::SeatMatch::Status;
    switch (m_seats->GetStatus()) {
    case Seat::Waiting: m_status = Status::Waiting; break;
    case Seat::Connecting: m_status = Status::Connecting; break;
    case Seat::Playing: m_status = Status::Playing; break;
    case Seat::Ended:
        // (the seat match said why already)
        m_status = Status::Ended; m_failed = m_seats->Failed(); m_message = m_seats->Message();
        return shown;
    }
    if (!m_seats->Message().empty() || m_status == Status::Playing) m_message = m_seats->Message();
    return shown;
}

bool Online::CreateGameSession() {
    GekkoConfig config{};
    // (this fork: spectators, the host's only; added once the match runs, from its confirmed state)
    const int spectators=ChatHost() ? std::clamp(m_options.spectators,0,kMostSpectators) : 0;
    config.num_players=2;config.max_spectators=static_cast<unsigned char>(spectators);config.max_input_prediction_window=kPredictionWindow;
    config.input_size=sizeof(uint16_t);config.state_size=m_stateSize;
    config.desync_detection=true;config.check_distance=10;
    if (!m_session.Start(GekkoGameSession, config, &g_adapter, kTimeoutMs)) return false;
    GekkoNetAddress peer{const_cast<char*>(kPeer),sizeof(kPeer)-1};
    if(ChatHost()) {m_local=m_session.AddActor(GekkoLocalPlayer,nullptr);m_remote=m_session.AddActor(GekkoRemotePlayer,&peer);}
    else {m_remote=m_session.AddActor(GekkoRemotePlayer,&peer);m_local=m_session.AddActor(GekkoLocalPlayer,nullptr);}
    m_session.Delay(m_local,static_cast<unsigned char>(m_options.delay));
    if(m_local<0 || m_remote<0)return false;
    ReplayBegin();
    return true;
}

// (this fork, native-replays)
void Online::ReplayBegin() {
    m_replay.reset();
    m_replayRan.clear();
    m_replayChecks.clear();
    m_replayBase = 0;
    if (!m_session || !m_port || (m_port->opt.replayFolder.empty() && !m_seats) || !m_port->opt.replayPlay.empty() || Spectating()) {
        return;
    }
    // (no state of GekkoNet's own, nor its packing: only the inputs it confirms, read back at the end)
    if (!gekko_start_recording(m_session.Get(), false, true)) {
        fprintf(stderr, "online: replay: GekkoNet does not record this session\n");
        return;
    }
    gekko_peek_recording(m_session.Get(), &m_replayBase);
}

void Online::ReplayAdvance(int frame, const Replay::Pads& players, bool before) {
    if (!m_replayBase || frame < 0) {
        return;
    }
    const size_t at = static_cast<size_t>(frame);
    if (before) {
        // The state the session began from: the board before its first frame ran
        if (!m_replay && at == 0) {
            m_replay = m_port->NewReplay("online");
            m_replay->players = static_cast<int>(m_players);
        }
        if (m_replayRan.size() <= at) {
            m_replayRan.resize(at + 1);
        }
        m_replayRan[at] = players;
        return;
    }
    if ((at + 1) % 60 == 0) {
        m_replayChecks[at + 1] = m_port->StateHash(true);
    }
}

void Online::ReplayEnd() {
    if (!ReplaySession() || !m_replayBase) {
        m_replay.reset();
        return;
    }
    unsigned int length = 0;
    const unsigned char *data = gekko_stop_recording(ReplaySession(), &length);
    const unsigned int base = m_replayBase;
    m_replayBase = 0;
    std::unique_ptr<Replay> replay = std::move(m_replay);
    if (!replay || !data) {
        return;
    }
    // GekkoNet's recording, as it serializes it unpacked and with nothing else kept: ..., the inputs (their length,
    // uint32, then each frame's players' inputs), then four empty fields (16 bytes: no state, no checksum interval,
    // no checksums, no user data). Its length less the length with no frames is the inputs' bytes.
    const unsigned int kBlock = m_players * sizeof(uint16_t);
    const unsigned int bytes = length >= base ? length - base : 0;
    if (bytes % kBlock != 0 || length < bytes + 20) {
        fprintf(stderr, "online: replay: GekkoNet's recording is not as expected (%u bytes, %u at the start)\n", length, base);
        return;
    }
    const unsigned char *inputs = data + length - 16 - bytes;
    const uint32_t said = inputs[-4] | inputs[-3] << 8 | inputs[-2] << 16 | static_cast<uint32_t>(inputs[-1]) << 24;
    bool tail = true;
    for (unsigned int i = length - 16; i < length; i++) {
        tail = tail && data[i] == 0;
    }
    if (said != bytes || !tail) {
        fprintf(stderr, "online: replay: GekkoNet's recording is not as expected (inputs %u, said %u)\n", bytes, said);
        return;
    }
    // The confirmed frames, as far as each one's last run had its confirmed inputs (a rollback still owed at the
    // end: the frames from there are left out)
    const size_t confirmed = bytes / kBlock;
    size_t frames = 0;
    Replay::Pads last = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    for (; frames < confirmed && frames < m_replayRan.size() && frames < m_replayCap; frames++) {
        const unsigned char *one = inputs + frames * kBlock;
        Replay::Pads pads{};
        for (unsigned seat = 0; seat < m_players; ++seat) pads[seat] = static_cast<uint16_t>(one[2*seat] | one[2*seat+1] << 8);
        if (pads != m_replayRan[frames]) break;
        if (pads != last) {
            replay->inputs[frames] = pads;
            last = pads;
        }
    }
    if (frames < 60) {
        return; // (under a second: nothing worth keeping)
    }
    for (const auto &[at, hash] : m_replayChecks) {
        if (at <= frames) {
            replay->checks.emplace_back(at, hash);
        }
    }
    replay->info.frames = frames;
    const bool host = m_options.mode != OnlineOptions::Mode::Join;
    replay->info.role = m_options.mode == OnlineOptions::Mode::Stress ? "stress" : host ? "host" : "guest";
    const std::string mine = m_options.name.empty() ? "Player" : m_options.name;
    const std::string theirs = m_theirName.empty() ? "Player" : m_theirName;
    replay->info.players = m_options.mode == OnlineOptions::Mode::Stress ? std::vector<std::string>{mine, mine}
                           : host ? std::vector<std::string>{mine, theirs} : std::vector<std::string>{theirs, mine};
    if (m_options.mode == OnlineOptions::Mode::Stress) replay->info.players.assign(m_players,mine);
    if(Star()) replay->info.players.assign(Star()->Names().begin(),Star()->Names().begin()+m_players);
    if(Star())for(auto&name:replay->info.players)if(name.empty())name="Empty seat";
    fprintf(stderr, "online: replay: %zu of %zu confirmed frames (%zu run)\n", frames, confirmed, m_replayRan.size());
    m_port->SaveReplay(*replay);
}

// (this fork) A spectator's session: the host as its one remote actor, the confirmed inputs it sends
bool Online::CreateSpectateSession() {
    GekkoConfig config{};
    config.num_players=2;config.max_spectators=0;config.max_input_prediction_window=kPredictionWindow;
    config.spectator_delay=static_cast<unsigned int>(std::clamp(m_options.spectatorDelay,1,300));
    config.input_size=sizeof(uint16_t);config.state_size=m_stateSize;
    config.desync_detection=false;
    if (!m_session.Start(GekkoSpectateSession, config, &g_adapter, kTimeoutMs)) return false;
    GekkoNetAddress peer{const_cast<char*>(kPeer),sizeof(kPeer)-1};
    m_remote=m_session.AddActor(GekkoRemotePlayer,&peer);
    return m_remote>=0;
}

// (the spectators the host took, whether the match runs for them yet or not)
int Online::Watching() const {
    return static_cast<int>(m_spectators.size());
}

// (this fork) The host's status, to the other player and the spectators (see the packet kinds above)
void Online::SendStatus(int phase) {
    if(!m_socket || !ChatHost() || !m_port)return;
    std::vector<uint8_t> q;
    Recovery::Put(q,m_options.matchFlow?m_localNonce:0);Recovery::Put(q,m_socket->epoch);
    q.push_back(static_cast<uint8_t>(phase));q.push_back(static_cast<uint8_t>(Watching()));
    for(int side=0;side<2;++side)q.push_back(static_cast<uint8_t>(std::clamp(m_port->streak[side],0,255)));
    for(const std::string& name:{ChatName(0),m_heard?ChatName(1):std::string()}){q.insert(q.end(),name.begin(),name.end());q.push_back(0);}
    if(m_heard && m_socket->havePeer)m_socket->Send('Q',q.data(),q.size(),m_socket->peer);
    for(const auto& [at,name]:m_socket->watchers)m_socket->Send('Q',q.data(),q.size(),at);
}

// (this fork) The host's last status, on the other player's side (how many watch) and a spectator's (what to show,
// and a new session when the match starts again from a new epoch)
void Online::ApplyStatus() {
    if(!m_socket || m_socket->host || m_socket->status.size()<24)return;
    const std::vector<uint8_t> q=std::move(m_socket->status);m_socket->status.clear();
    const uint64_t room=Recovery::Get(q.data()),epoch=Recovery::Get(q.data()+8);
    if(m_options.matchFlow && m_roomNonce && room!=m_roomNonce)return;
    m_seenWatching=q[17];
    if(!Spectating())return;
    size_t at=20;
    for(std::string& name:m_statusNames) {
        size_t end=at;
        while(end<q.size() && q[end])end++;
        name=recomp::chat::Printable(std::string(q.begin()+static_cast<std::ptrdiff_t>(at),q.begin()+static_cast<std::ptrdiff_t>(end)),recomp::chat::kNameMost);
        at=std::min(q.size(),end+1);
    }
    m_port->streak[0]=q[18];m_port->streak[1]=q[19];
    if(m_hostPhase!=q[16])fprintf(stderr,"online: watching: the host's phase %d (epoch %llu)\n",q[16],static_cast<unsigned long long>(epoch));
    m_hostPhase=q[16];
    auto drop=[&]{if(m_session)m_session.Reset();m_socket->forGekko.clear();m_socket->forGekkoFrom.clear();};
    if(m_hostPhase==4){drop();End("The host stopped hosting",false);return;}
    if(m_hostPhase==3) {
        if(m_status!=Status::Recovering)fprintf(stderr,"online: watching: the players are reconnecting\n");
        drop();m_status=Status::Recovering;m_message="Reconnecting";return;
    }
    if(m_options.matchFlow && epoch!=m_socket->epoch) {
        fprintf(stderr,"online: watching epoch %llu\n",static_cast<unsigned long long>(epoch));
        drop();m_socket->epoch=epoch;
    }
    if(m_heard && !m_session) {
        if(!CreateSpectateSession()){End("The spectator session could not start",true);return;}
        m_status=Status::Connecting;
    }
    if(m_status!=Status::Playing)
        m_message=m_hostPhase!=0?"Waiting for the match"
                  :m_statusNames[0].empty()?"WAITING FOR A CHALLENGER":m_statusNames[0]+" IS WAITING FOR A CHALLENGER";
}

// (this fork) The host adds the spectators who said hello once the match runs: from its confirmed state, which
// GekkoNet sends them, never waiting for them
void Online::AddSpectators() {
    if(!m_session || m_status!=Status::Playing || !ChatHost())return;
    for(Spectator& one:m_spectators) {
        if(one.handle>=0)continue;
        GekkoNetAddress at{const_cast<char*>(one.name.data()),static_cast<unsigned int>(one.name.size())};
        one.handle=m_session.AddActor(GekkoSpectator,&at);
        fprintf(stderr,"online: spectator %s (%s) %s at frame %llu\n",one.name.c_str(),one.nick.c_str(),one.handle>=0?"added":"not added",
                static_cast<unsigned long long>(m_port->frame));
    }
}

void Online::SpectatorLeft(int handle) {
    for(size_t i=0;i<m_spectators.size();++i) {
        if(m_spectators[i].handle!=handle)continue;
        fprintf(stderr,"online: spectator %s left\n",m_spectators[i].name.c_str());
        m_socket->watcherHeard.erase(m_spectators[i].name);
        auto& list=m_socket->watchers;
        list.erase(std::remove_if(list.begin(),list.end(),[&](const auto& w){return w.second==m_spectators[i].name;}),list.end());
        m_spectators.erase(m_spectators.begin()+static_cast<std::ptrdiff_t>(i));
        return;
    }
}

// (this fork) Spectators' hellos, at the host: checked as a player's, then kept until the match runs
void Online::WatchHellos() {
    // (spectators not heard for 10 s while the match does not run for them: gone)
    const uint64_t now=NowMs();
    for(size_t i=0;i<m_spectators.size();) {
        const Spectator& one=m_spectators[i];
        const auto heard=m_socket->watcherHeard.find(one.name);
        if(one.handle<0 && (heard==m_socket->watcherHeard.end() || now-heard->second>10000)) {
            fprintf(stderr,"online: spectator %s gone\n",one.name.c_str());
            auto& list=m_socket->watchers;
            list.erase(std::remove_if(list.begin(),list.end(),[&](const auto& w){return w.second==one.name;}),list.end());
            m_socket->watcherHeard.erase(one.name);
            m_spectators.erase(m_spectators.begin()+static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
    auto hellos=std::move(m_socket->watchHellos);m_socket->watchHellos.clear();
    for(auto& [from,whole]:hellos) {
        std::vector<uint8_t> said;std::string name;
        recomp::chat::SplitHello(whole,said,name);
        auto refuse=[&](const std::string& why){m_socket->Refuse(why,from);};
        if(!m_socket->WatcherName(from)) {
            const int most=std::clamp(m_options.spectators,0,kMostSpectators);
            uint64_t theirNonce=0;
            if(m_options.matchFlow && !RecoveryHelloNonce(said,theirNonce)){refuse(Mismatch(said,false));continue;}
            if(most==0){refuse("This match does not allow spectators");continue;}
            if(static_cast<int>(m_spectators.size())>=most){refuse("The match is full ("+std::to_string(most)+" watching)");continue;}
            const std::string why=Mismatch(said,true);
            if(!why.empty()){refuse(why);continue;}
            int number=1;
            for(bool taken=true;taken;) {
                taken=false;
                for(const Spectator& one:m_spectators)taken|=one.name=="spec"+std::to_string(number);
                if(taken)number++;
            }
            Spectator one;one.name="spec"+std::to_string(number);one.nick=name;
            // (its chat: its lines to the host, and the others' relayed to it)
            Socket* socket=m_socket;const std::string to=one.name;
            auto send=[socket,to](char kind,const std::vector<uint8_t>& payload) {
                if(socket->ChatLost())return;
                if(const sockaddr_in* at=socket->WatcherAt(to.data(),to.size()))socket->Send(kind,payload.data(),payload.size(),*at);
            };
            one.own=std::make_shared<recomp::chat::Channel>(send);
            one.relay=std::make_shared<ChatRelay>(send);
            m_spectators.push_back(one);m_socket->watchers.emplace_back(from,one.name);
            fprintf(stderr,"online: spectator %s (%s) said hello\n",one.name.c_str(),name.c_str());
        }
        if(const std::string* known=m_socket->WatcherName(from))m_socket->watcherHeard[*known]=now;
        // (each hello answered, as the players' are: the first answer may be lost)
        const std::vector<uint8_t> hello=HelloOut();
        m_socket->Send('H',hello.data(),hello.size(),from);
    }
    AddSpectators();
}

void Online::SendControl(char kind,uint64_t epoch) {
    if(!m_socket || !m_socket->havePeer || !m_roomNonce || !kind)return;
    std::vector<uint8_t> bytes;Recovery::Put(bytes,m_roomNonce);Recovery::Put(bytes,epoch);
    m_socket->Send(kind,bytes.data(),bytes.size(),m_socket->peer);
}

void Online::PrepareRecovery() {
    if(m_prepared)return;
    // Outside all SDK event iterations: keep socket, peer and reliable chat channel.
    ReplayEnd();
    if(m_session)m_session.Reset();
    m_socket->forGekko.clear();m_socket->forGekkoFrom.clear();m_socket->forChat.clear();
    for(Spectator& one:m_spectators)one.handle=-1; // (this fork: re-added when the new session runs)
    m_prepared=true;
}

void Online::RequestRecovery() {
    if(!m_options.matchFlow || !m_everPlayed){End("The other player left",false);return;}
    if(m_recovery.Waiting())return;
    m_status=Status::Recovering;
    const auto action=m_recovery.Begin(NowMs());
    if(action.expired){End("The reconnection sequence is exhausted",true);return;}
    SendControl(action.reply,action.epoch);
    fprintf(stderr,"online: recovery requested, fixed 60-second deadline\n");
}

bool Online::ResetRecoveryEpoch() {
    ReplayEnd();
    if(m_session)m_session.Reset();
    if(!LoadFrom(m_arcadeState,*m_port->machine)){End("The initial recovery state could not be restored",true);return false;}
    m_socket->forGekko.clear();m_socket->forGekkoFrom.clear();m_socket->forChat.clear();
    m_socket->epoch=m_recovery.Epoch();
    for(Spectator& one:m_spectators)one.handle=-1; // (this fork: re-added when the new session runs)
    m_port->frame=0;m_port->coinFrames=m_port->start2Frames=0;m_port->machine->audio.clear();
    m_port->gekkoArcadeWaiting=false;m_port->paused=false;m_port->steps=0;m_port->menu.Close();
    m_flow.Reset();PublishFlow();
    if(!CreateGameSession()){End("The recovered rollback session could not start",true);return false;}
    if(m_checkpointLog)std::fprintf(m_checkpointLog,"# epoch %llu\n",static_cast<unsigned long long>(Epoch()));
    if(m_flowLog)std::fprintf(m_flowLog,"# epoch %llu\n",static_cast<unsigned long long>(Epoch()));
    fprintf(stderr,"online: restored epoch %llu at frame 0\n",static_cast<unsigned long long>(Epoch()));
    m_prepared=true;return true;
}

void Online::ApplyRecovery(const Recovery::Action& action) {
    if(action.expired){End("Reconnection timed out after 60 seconds",true);return;}
    if(action.left){End("The other player left",false);return;}
    if(m_recovery.Waiting()) {
        m_status=Status::Recovering;PrepareRecovery();
    }
    if(action.reset && !ResetRecoveryEpoch())return;
    if(action.ready){m_status=Status::Connecting;m_prepared=false;}
    SendControl(action.reply,action.epoch);
}

void Online::End(const std::string &message, bool failed) {
    m_status = Status::Ended;
    m_failed = failed;
    m_message = message;
    fprintf(stderr, "online: %s\n", message.c_str());
}

// Why a hello is not this match's ("" when it is): another game, ROM set, version or settings
std::string Online::Mismatch(const std::vector<uint8_t> &said, bool recoveryCompatible) const {
    if (said == m_hello && recoveryCompatible) {
        return "";
    }
    const std::string theirs(said.begin(), said.end()), ours(m_hello.begin(), m_hello.end());
    const auto field = [](const std::string &text, int n) {
        size_t from = 0;
        for (int i = 0; i < n; i++) {
            from = text.find('|', from);
            from = from == std::string::npos ? text.size() : from + 1;
        }
        // (this fork: only what can be shown, and not too long: a hello may be anyone's bytes)
        return recomp::chat::Printable(text.substr(from, text.find('|', from) - from), 48);
    };
    std::string why = "The other player has another game (" + field(theirs, 0) + ")";
    if (field(theirs, 0) == field(ours, 0)) {
        why = field(theirs, 1) != field(ours, 1) ? "The other player's ROM set is not the same as yours"
              : field(theirs, 2) != field(ours, 2)
                  ? "The other player has another version (" + field(theirs, 2) + ", yours " + field(ours, 2) + ")"
                  : "Your settings for the game differ: " + Differing(field(theirs, 3), field(ours, 3));
    }
    if(!recoveryCompatible && field(theirs,0)==field(ours,0) && field(theirs,1)==field(ours,1) &&
       field(theirs,2)==field(ours,2) && field(theirs,3)==field(ours,3))
        why="The other player uses an incompatible ForkAde recovery protocol";
    return why;
}

// (this fork: with this player's name after a 0 byte, and the match flow's nonce before it)
std::vector<uint8_t> Online::HelloOut() const {
    auto base=m_hello;
    if(m_options.matchFlow) {
        char nonce[18];std::snprintf(nonce,sizeof nonce,":%016llx",static_cast<unsigned long long>(m_localNonce));
        base.insert(base.end(),nonce,nonce+17);
    }
    return recomp::chat::HelloWithName(base, m_options.name);
}

void Online::SendHello() {
    if (m_socket->havePeer) {
        const std::vector<uint8_t> hello = HelloOut();
        m_socket->Send(Spectating() ? 'W' : 'H', hello.data(), hello.size(), m_socket->peer);
    }
}

// The hellos that came: the host takes the first one's sender as its peer; each side checks it is the same game
void Online::Pump() {
    m_socket->Pump();
    for (auto &[from, whole] : m_socket->hellos) {
        // (this fork: the name after a 0 byte is not compared)
        std::vector<uint8_t> said;
        std::string name;
        recomp::chat::SplitHello(whole, said, name);
        if (!m_socket->havePeer && m_socket->host) {
            // (this fork, online-abuse-protection: the first hello is taken as the peer only if it is this game's;
            // anything else is refused and the host goes on waiting, so a stranger's garbage cannot end its match)
            // (on a copy: RecoveryHelloNonce takes the nonce off what it is given, which the checks below need whole)
            std::vector<uint8_t> checked = said;
            uint64_t theirNonce = 0;
            const bool compatible = !m_options.matchFlow || RecoveryHelloNonce(checked, theirNonce);
            if (const std::string why = Mismatch(checked, compatible); !why.empty()) {
                m_socket->Refuse(why, from);
                continue;
            }
            m_socket->peer = from;
            m_socket->havePeer = true;
        }
        if (!Socket::Same(from, m_socket->peer)) {
            // (someone else: not the player this session is with; (this fork) told so, as a spectator would be)
            if (m_socket->host) {
                m_socket->Refuse("The match is full: two players are already playing", from);
            }
            continue;
        }
        if (m_socket->host) {
            SendHello(); // (each hello answered: the first answer may be lost)
        }
        if (m_heard) {
            continue;
        }
        uint64_t peerNonce=0;
        const bool recoveryCompatible=!m_options.matchFlow || RecoveryHelloNonce(said,peerNonce);
        if (const std::string why = Mismatch(said, recoveryCompatible); !why.empty()) {
            End(why, true);
            return;
        }
        if(m_options.matchFlow) {
            m_peerNonce=peerNonce;m_roomNonce=ChatHost()?m_localNonce:m_peerNonce;
            m_socket->room=m_roomNonce;m_socket->lastReceived=NowMs();
        }
        if(m_port->gekkoArcadeWaiting) {
            const auto solo=m_port->frame;
            if(!LoadFrom(m_arcadeState,*m_port->machine)){End("The initial arcade state could not be restored",true);return;}
            m_port->frame=0;m_port->coinFrames=m_port->start2Frames=0;m_port->machine->audio.clear();m_port->cameIn=m_port->otherIn=false;
            m_port->gekkoArcadeWaiting=false;m_port->menu.Close();m_port->paused=false;m_port->steps=0;
            fprintf(stderr,"online: arcade reset after %llu solo frames\n",static_cast<unsigned long long>(solo));
        }
        if(m_options.matchFlow)m_challengerUntil=NowMs()+1200;
        m_heard = true;
        m_theirName = name;
        m_status = Status::Connecting;
        m_message = "Connecting";
        if (Spectating()) {
            // (this fork) The host took this spectator: its session, which waits for the match's state
            if (!CreateSpectateSession()) {
                End("The spectator session could not start", true);
                return;
            }
            m_message = "Waiting for the match";
        }
    }
    m_socket->hellos.clear();
    if (m_socket->host) {
        WatchHellos();
    } else if (!m_socket->refused.empty()) {
        End(recomp::chat::Printable(m_socket->refused, 120), true);
        return;
    } else {
        ApplyStatus();
        if (m_status == Status::Ended) {
            return;
        }
    }
    auto controls=std::move(m_socket->controls);m_socket->controls.clear();
    for(const auto& [kind,payload]:controls) {
        if(!m_options.matchFlow || !m_heard || payload.size()!=16 || Recovery::Get(payload.data())!=m_roomNonce)continue;
        const auto action=m_recovery.Receive(kind,Recovery::Get(payload.data()+8),NowMs());
        if(action.accepted)m_socket->lastReceived=NowMs();
        ApplyRecovery(action);
        if(m_status==Status::Ended)break;
    }
    // (this fork) The chat's packets, once the other side is known
    for (const auto &[kind, payload] : m_socket->forChat) {
        if (m_heard && m_chat) {
            m_chat->Receive(kind, payload.data(), payload.size(), NowMs());
        }
    }
    m_socket->forChat.clear();
    // (this fork: spectators' chat) the relay's packets from the host; at the host, each spectator's
    for (const auto &[kind, payload] : m_socket->forRelay) {
        if (m_heard && m_relay) {
            m_relay->Receive(kind, payload.data(), payload.size(), NowMs());
        }
    }
    m_socket->forRelay.clear();
    // (match-spectators 7.1) a spectator's chat is ignored: not received, acknowledged, shown, relayed or logged
    m_socket->fromWatchers.clear();
}

void Online::PublishFlow() {
    const auto& state = m_flow.state;
    if (const auto *star = Star()) {
        // (seats: this PC's first seat, and anyone else's)
        const unsigned own = star->Primary() < 4 ? star->Primary() : 0;
        m_port->cameIn = state.joined[own];
        m_port->otherIn = false;
        for (unsigned seat = 0; seat < m_players && seat < 4; ++seat) if (seat != own && state.joined[seat]) m_port->otherIn = true;
    } else {
        m_port->cameIn = state.joined[ChatLocal()];
        m_port->otherIn = state.joined[1 - ChatLocal()];
    }
    for (int side = 0; side < 2; ++side) {
        m_port->streak[side] = state.streak[side];
        m_port->roundsWon[side] = state.rounds[side];
    }
    m_port->roundCounted = state.counted;
    m_port->roundQuiet = state.quiet;
}

void Online::UpdateFlowMessage() {
    if(!m_options.matchFlow || m_status==Status::Ended || Spectating())return;
    if(Recovering())m_message=ChatHost()?"Waiting for the same player (up to 60 seconds)":"Rejoining the same match (up to 60 seconds)";
    else if(m_heard && NowMs()<m_challengerUntil)m_message=Epoch()>1?"Connection restored - starting again":"Here comes a new challenger";
    else if(m_status==Status::Connecting && m_message=="Here comes a new challenger")m_message="Connecting";
    else if(m_status==Status::Playing)m_message=m_port->cameIn?"":"PRESS START TO JOIN";
    if(m_message!=m_lastFlowMessage) {
        m_lastFlowMessage=m_message;
        fprintf(stderr,"online: match flow: %s\n",m_message.empty()?"Playing":m_message.c_str());
    }
}

int Online::Tick(std::span<const uint16_t> local, bool advanceWaiting) {
    const uint64_t inputEpoch=Epoch();
    UpdateFlowMessage();
    m_ticks++;
    m_shown = 0;
    // (this fork) The window's clock, as Port::Tick moves it offline and in Tommy's netplay: the chat's lines, its
    // hint and the notices last for so many ticks of it, and a GekkoNet match never goes through Port::Tick
    if (m_options.mode != OnlineOptions::Mode::Stress) {
        m_port->tick++;
    }
    // (this fork) After the tick, whatever it did, the chat's (Port::ChatTick): lines heard, the log, the end
    struct ChatAfter {
        Port *port;
        ~ChatAfter() {
            if (port) {
                port->ChatTick();
            }
        }
    } chatAfter{m_options.mode == OnlineOptions::Mode::Stress ? nullptr : m_port};
    if (m_status == Status::Ended) {
        return 0;
    }
    if (m_seats) {
        if(local.size()<m_options.localPlayers){End("A local seat input is missing",true);return 0;}
        std::array<uint16_t,2> inputs{};std::copy_n(local.begin(),m_options.localPlayers,inputs.begin());
        if(m_port->coinFrames>0){--m_port->coinFrames;inputs[0]|=cps2recomp::kCoin;}
        return TickSeats(std::span<const uint16_t>(inputs.data(),m_options.localPlayers));
    }
    if (local.size() < m_players) { End("The local input buffer has too few player lanes",true); return 0; }
    uint16_t mine = local[0];
    // (the menu's coin: through this player's controls, as everything that changes the game must be)
    if (m_port->coinFrames > 0 && !ArcadeWaiting()) {
        m_port->coinFrames--;
        mine |= cps2recomp::kCoin;
    }
    if (m_options.mode == OnlineOptions::Mode::Stress) {
        for (unsigned seat = 0; seat < m_players; ++seat) {
            uint16_t input = seat == 0 ? mine : local[seat];
            gekko_add_local_input(m_session.Get(), m_actors[seat], &input);
        }
        Run();
        return m_shown;
    }
    Pump();
    if (m_status == Status::Ended) {
        return 0;
    }
    if (ChatHost() && m_ticks % 15 == 0) {
        // (this fork: spectators) the host's status, to the other player and the spectators
        SendStatus(m_status == Status::Ended ? 4 : Recovering() ? 3 : !m_heard ? 0 : m_status == Status::Playing ? 2 : 1);
    }
    UpdateFlowMessage();
    if (Spectating()) {
        // (this fork) A spectator: no input of its own; the confirmed frames as they come, faster when behind
        if (!m_heard) {
            if (m_ticks % 15 == 1) {
                SendHello();
            }
            return 0;
        }
        if (m_ticks % 60 == 0) {
            SendHello(); // (the host keeps a spectator it hears, while the match does not run for it too)
        }
        if (m_socket->statusMs && NowMs() - m_socket->statusMs > kTimeoutMs) {
            fprintf(stderr, "online: watching: no status from the host for %llu ms (tick %llu)\n",
                    static_cast<unsigned long long>(NowMs() - m_socket->statusMs), static_cast<unsigned long long>(m_ticks));
            End("The connection to the match was lost", false);
            return 0;
        }
        PumpChat();
        if (!m_session) {
            return 0;
        }
        gekko_network_poll(m_session.Get());
        Run();
        for (int more = 0; more < 2 && m_status == Status::Playing &&
                           gekko_spectator_buffered_frames(m_session.Get()) > std::clamp(m_options.spectatorDelay, 1, 300) + 8; ++more) {
            Run();
        }
        return m_shown;
    }
    if (!m_heard) {
        if(ArcadeWaiting() && advanceWaiting) {
            m_port->pad[0]=mine;
            m_port->ArcadeFrame();
            return 1;
        }
        if (!m_socket->host && m_ticks % 15 == 1) {
            SendHello(); // (four times a second until the host answers)
        }
        return 0;
    }
    if(m_options.matchFlow) {
        if(m_everPlayed && !m_recovery.Waiting() && NowMs()-m_socket->lastReceived>=kTimeoutMs)RequestRecovery();
        if(m_status==Status::Recovering)PrepareRecovery();
        ApplyRecovery(m_recovery.Tick(NowMs()));
        if(m_status==Status::Ended || m_recovery.Waiting() || Epoch()!=inputEpoch)return 0;
    }
    gekko_network_poll(m_session.Get());
    PumpChat(); // (this fork: the chat's lines not acknowledged yet, again)
    gekko_add_local_input(m_session.Get(), m_local, &mine);
    Run();
    if(m_status==Status::Recovering)PrepareRecovery();
    UpdateFlowMessage();
    return m_shown;
}

void Online::Run() {
    if (!m_session) return;
    Events();
    if (m_status == Status::Ended || m_status == Status::Recovering) return;
    Machine &machine = *m_port->machine;
    recomp::rollback::Callbacks callbacks;
    callbacks.active = [&] { return m_status != Status::Ended && m_status != Status::Recovering; };
    callbacks.save = [&](int frame, std::vector<uint8_t>& state) {
        if(m_options.matchFlow && !Spectating()) {
            m_flow.Save(frame);
            if(m_flowLog) {
                const auto& s=m_flow.state;
                std::fprintf(m_flowLog,"%d %d %d %d %d %d %d %d %d\n",frame,
                    int(s.joined[0]),int(s.joined[1]),s.streak[0],s.streak[1],s.rounds[0],s.rounds[1],int(s.counted),s.quiet);
            }
        }
        SaveTo(state, machine);
        return true;
    };
    callbacks.checksum = [&](int frame) {
        // Preserve the game's semantic fingerprint, not padding or bookkeeping in saved bytes.
        const uint32_t hash = m_port->StateHash();
        if (m_checkpointLog) std::fprintf(m_checkpointLog, "%lld %08x\n", static_cast<long long>(frame), hash);
        return hash;
    };
    callbacks.load = [&](int frame, std::span<const uint8_t> bytes) {
        if(m_options.matchFlow && !Spectating()) {
            if(!m_flow.Load(frame)) { End("The match presentation checkpoint is unavailable",true); return false; }
            PublishFlow();
        }
        std::vector<uint8_t> data(bytes.begin(), bytes.end());
        if (!LoadFrom(data, machine)) {
            End("A state the session kept did not load", true);
        }
        if (Spectating()) {
            // (this fork) The match's state as the host confirmed it: GekkoNet's frame N is the board after N + 1
            // frames, which is what the players' frame count says then
            m_port->frame = static_cast<uint64_t>(std::max(0, frame + 1));
            fprintf(stderr, "online: watching from frame %d (%zu bytes)\n", frame, bytes.size());
        }
        return callbacks.active();
    };
    callbacks.advance = [&](recomp::rollback::Frame frame, std::span<const uint8_t> inputs) {
        Replay::Pads players{};
        if (!recomp::rollback::DecodeInputs(inputs,m_players,players)) return false;
        // A frame run again (or ahead) is neither seen nor heard: its sound is dropped
        const bool again = frame.rollback || frame.runahead;
        const size_t heard = machine.audio.size();
        if (!frame.runahead) {
            ReplayAdvance(frame.number, players, true);
        }
        if (!m_port->RunOnlineFrame(std::span<const uint16_t>(players.data(),m_players), again)) {
            End("The board refused the player input lanes",true); return false;
        }
        if (!frame.runahead) {
            ReplayAdvance(frame.number, players, false);
        }
        if(m_options.matchFlow && !Spectating() && !frame.runahead) {
            const bool before=m_port->cameIn;
            const auto result = TheGame().onlineMatchResult;
            m_flow.Advance(players.data(),cps2recomp::kStart,result ? result() : cps2recomp::battle::RoundResultNow(),result ? 1 : cps2recomp::battle::RoundsToWin(),m_players);
            PublishFlow();
            if(!again && !before && m_port->cameIn)fprintf(stderr,"online: match flow: local Start received\n");
        }
        if (again) {
            machine.audio.resize(heard);
        }
        if (!again) {
            m_shown++;
        }
        return callbacks.active();
    };
    callbacks.error = [&](recomp::rollback::Error error) {
        End(error == recomp::rollback::Error::StateCapacity
                ? "The board's state outgrew what the session keeps"
                : "The rollback session supplied invalid buffers", true);
    };
    recomp::rollback::Step(m_session.Get(), {m_players, sizeof(uint16_t), m_stateSize}, m_state, callbacks);
    Events();
}

void Online::Events() {
    int count = 0;
    GekkoSessionEvent **events = gekko_session_events(m_session.Get(), &count);
    for (int i = 0; i < count && m_status != Status::Ended && m_status!=Status::Recovering; i++) {
        const GekkoSessionEvent *event = events[i];
        switch (event->type) {
        case GekkoPlayerSyncing:
            m_message = "Connecting " + std::to_string(event->data.syncing.current) + "/" + std::to_string(event->data.syncing.max);
            break;
        case GekkoSessionStarted:
            if (m_status != Status::Playing) {
                fprintf(stderr, "online: playing\n");
            }
            m_status = Status::Playing;
            if(m_options.matchFlow && !Spectating()) {
                const bool resumed=Recovering();m_recovery.Complete();m_prepared=false;m_everPlayed=true;
                if(resumed){m_challengerUntil=NowMs()+1200;fprintf(stderr,"online: recovery playing epoch %llu\n",static_cast<unsigned long long>(Epoch()));}
            }
            m_message.clear();
            AddSpectators();
            break;
        case GekkoPlayerDisconnected:
            // (this fork) A spectator leaving is not the other player leaving
            if (ChatHost() && event->data.disconnected.handle != m_remote) {
                SpectatorLeft(event->data.disconnected.handle);
                break;
            }
            if (Spectating()) {
                // (the players reconnecting: the host's status says so, and a new session comes with the new epoch)
                if (m_hostPhase != 3) {
                    End("The match ended or the connection was lost", false);
                }
                break;
            }
            RequestRecovery();
            break;
        case GekkoSpectatorPaused:
            fprintf(stderr, "online: watching paused, waiting for the match\n");
            break;
        case GekkoSpectatorUnpaused:
            fprintf(stderr, "online: watching again\n");
            break;
        case GekkoDesyncDetected: {
            const auto &d = event->data.desynced;
            fprintf(stderr, "online: desync at frame %d (here %08x, the other side %08x)\n", d.frame, d.local_checksum, d.remote_checksum);
            End("Desync at frame " + std::to_string(d.frame), true);
            break;
        }
        default: break;
        }
    }
}

std::string Online::Stats() const {
    if (!m_session || m_status != Status::Playing) {
        return "";
    }
    GekkoRollbackStats rollbacks{};
    gekko_rollback_stats(m_session.Get(), &rollbacks);
    char text[128];
    if (m_options.mode == OnlineOptions::Mode::Stress) {
        snprintf(text, sizeof(text), "STRESS  rollbacks %u", rollbacks.total_count);
    } else {
        GekkoNetworkStats net{};
        gekko_network_stats(m_session.Get(), m_remote, &net);
        // (this fork: spectators) WATCHING on a spectator's screen; how many watch on the players'
        const int watching = ChatHost() ? Watching() : m_seenWatching;
        if (Spectating()) {
            snprintf(text, sizeof(text), "WATCHING   PING %u", static_cast<unsigned>(net.last_ping));
        } else if (watching > 0) {
            snprintf(text, sizeof(text), "PING %u  DELAY %d  ROLLBACK %u   %d WATCHING", static_cast<unsigned>(net.last_ping),
                     m_options.delay, rollbacks.last_frames, watching);
        } else {
            snprintf(text, sizeof(text), "PING %u  DELAY %d  ROLLBACK %u", static_cast<unsigned>(net.last_ping), m_options.delay,
                     rollbacks.last_frames);
        }
    }
    return text;
}

// (this fork) ChatLink: the in-game chat over this session
ChatLink::Said Online::ChatSay(const std::string &text, std::string &line) {
    if(m_options.matchFlow && Recovering())return Said::Wait;
    switch (m_chat ? m_chat->Say(text, NowMs(), &line) : recomp::chat::Channel::Said::Empty) {
    case recomp::chat::Channel::Said::Sent:
        return Said::Sent;  // (match-spectators 7.1: to the other player only; spectators get no chat)
    case recomp::chat::Channel::Said::Wait: return Said::Wait;
    default: return Said::Empty;
    }
}

std::vector<std::string> Online::ChatHeard() {
    std::vector<std::string> lines;
    for (Heard &one : ChatHeardLines()) {
        lines.push_back(std::move(one.text));
    }
    return lines;
}

// (this fork: spectators' chat) Every line heard, with who said it; the host relays each to whoever did not say it
std::vector<ChatLink::Heard> Online::ChatHeardLines() {
    std::vector<Heard> lines;
    if (m_chat) {
        for (std::string &text : m_chat->TakeHeard()) {
            lines.push_back({"", std::move(text), false});
        }
    }
    // (match-spectators 7.1, 11 November 2026: the in-game chat is the players'. The host relays nothing to spectators
    // and takes nothing from them, and a relay's lines from a host are not shown. The messages' role and origin fields
    // stay, for a later game with more than two players)
    if (m_relay) {
        m_relay->Take();
    }
    return lines;
}

// (the host's) a line to the other player (unless it said it: role 1) and to every spectator but `except`
void Online::RelayLine(int role, const std::string &name, const std::string &text, const std::string &except) {
    const uint64_t now = NowMs();
    if (role == 2 && m_relay && m_heard) {
        m_relay->Push({role, name, text}, now);
    }
    for (Spectator &one : m_spectators) {
        if (one.name != except && one.relay) {
            one.relay->Push({role, name, text}, now);
        }
    }
}

void Online::PumpChat() {
    const uint64_t now = NowMs();
    if (m_chat) {
        m_chat->Pump(now);
    }
    if (m_relay) {
        m_relay->Pump(now);
    }
    for (Spectator &one : m_spectators) {
        one.own->Pump(now);
        one.relay->Pump(now);
    }
}

std::string Online::ChatName(int player) const {
    if(Star())return player>=0&&unsigned(player)<m_players?Star()->Names()[player]:std::string();
    return player == ChatLocal() ? recomp::chat::Printable(m_options.name, recomp::chat::kNameMost) : m_theirName;
}

int Online::ChatLocal() const {
    if(Star())return Star()->Primary()<4?int(Star()->Primary()):0;
    return m_options.mode == OnlineOptions::Mode::Join ? 1 : 0;
}

bool Online::ChatHost() const {
    return m_options.mode == OnlineOptions::Mode::Host;
}

bool Online::ChatReady() const {
    return m_chat && m_heard && m_status == Status::Playing;
}

bool Online::ChatOver() const {
    return m_status == Status::Ended;
}

std::vector<std::string> Online::ChatUndelivered() const {
    return m_chat ? m_chat->Unacknowledged() : std::vector<std::string>{};
}

float Online::FramesAhead() const {
    return m_session && m_status == Status::Playing ? gekko_frames_ahead(m_session.Get()) : 0.0f;
}


MatchView Online::ReadMatch() const {
    MatchView view;
    switch (m_status) {
    case Status::Waiting: view.phase = MatchView::Phase::Waiting; break;
    case Status::Connecting: view.phase = MatchView::Phase::Connecting; break;
    case Status::Playing: view.phase = MatchView::Phase::Playing; break;
    case Status::Recovering: view.phase = MatchView::Phase::Connecting; break;
    case Status::Ended: view.phase = MatchView::Phase::Ended; break;
    }
    view.hosting = ChatHost(); view.found = m_heard; view.arcade = m_options.arcadeWait;
    view.local = ChatLocal(); view.delay = m_options.delay; view.failed = m_failed;
    view.names = {ChatName(0), ChatName(1), "", ""};
    if (const auto *star = Star()) {
        // (multiplayer-online 4.1) every seat's name; an empty seat has none
        view.players = static_cast<int>(m_players);
        for (unsigned seat = 0; seat < m_players && seat < 4; ++seat) view.names[seat] = star->Names()[seat];
    }
    if (Spectating()) {
        view.names = {m_statusNames[0], m_statusNames[1]}; // (this fork: the players', from the host's status)
    }
    view.message = Message(); view.stats = Stats();
    if (m_session && m_status == Status::Playing && m_options.mode != OnlineOptions::Mode::Stress) {
        GekkoNetworkStats stats{};
        gekko_network_stats(m_session.Get(), m_remote, &stats);
        view.ping = static_cast<int>(stats.last_ping);
        GekkoRollbackStats rollbacks{};
        gekko_rollback_stats(m_session.Get(), &rollbacks);
        view.rollback = static_cast<int>(rollbacks.last_frames);
    }
    view.stress = m_options.mode == OnlineOptions::Mode::Stress;
    view.spectating = Spectating();
    view.watching = ChatHost() ? Watching() : m_seenWatching;
    // Arcade, code, invitations, leave/loss reasons and silence timing await their flow implementation.
    return view;
}

} // namespace port
