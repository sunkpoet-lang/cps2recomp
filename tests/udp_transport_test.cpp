// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 the fork's contributors.
#include "udp_transport.h"
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>
#include <algorithm>

int main() {
    using namespace recomp::net;
    static_assert(!std::is_copy_constructible_v<Udp> && !std::is_copy_assignable_v<Udp>);
    unsigned checks = 0;
    auto check = [&](bool ok) { if (!ok) throw std::runtime_error("UDP check " + std::to_string(checks)); ++checks; };
    auto receive = [&](Udp& socket, std::span<uint8_t> buffer, Endpoint& from) {
        auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        for (;;) {
            auto r = socket.Receive(buffer, from);
            if (r.kind != ReceiveKind::WouldBlock && r.kind != ReceiveKind::PeerUnreachable) return r;
            if (std::chrono::steady_clock::now() > until) throw std::runtime_error("loopback receive timeout");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    for (unsigned cycle = 0; cycle < 8; ++cycle) {
        Udp first, second; std::string error; Endpoint a{}, b{}, source{};
        check(!first && !first.Local(a)); check(!first.Resolve("127.0.0.1", 1, a, error));
        check(first.Open(0, error, BindScope::Loopback)); check(second.Open(0, error, BindScope::Loopback));
        check(first.Local(a) && second.Local(b)); check(ntohs(a.sin_port) && ntohs(b.sin_port) && !Same(a,b));
        check(!first.Open(0, error, BindScope::Loopback)); Endpoint resolved{};
        check(first.Resolve("127.0.0.1", ntohs(b.sin_port), resolved, error)); check(Same(b,resolved));
        check(!first.Resolve("", 1, resolved, error));
        Udp collision; check(!collision.Open(ntohs(a.sin_port), error, BindScope::Loopback)); check(!collision);
        std::array<uint8_t,32> small{}; check(first.Receive(small,source).kind==ReceiveKind::WouldBlock);
        for (size_t size : {size_t(0),size_t(1),size_t(31),size_t(1024),Udp::maxDatagram}) {
            std::vector<uint8_t> data(size), buffer(size+1,0xcc);for(size_t i=0;i<size;++i)data[i]=uint8_t(i*17+cycle);
            check(first.Send(data,b));auto result=receive(second,buffer,source);
            check(result.kind==ReceiveKind::Packet && result.size==size && Same(source,a));
            check(std::equal(data.begin(),data.end(),buffer.begin()) && buffer[size]==0xcc);
        }
        std::vector<uint8_t> excess(Udp::maxDatagram+1);check(!first.Send(excess,b));
        std::array<uint8_t,64> data{};check(first.Send(data,b));auto shortRead=receive(second,small,source);
        check(shortRead.kind==ReceiveKind::Truncated);check(second.Receive(small,source).kind==ReceiveKind::WouldBlock);
        check(second.Receive({},source).kind==ReceiveKind::Error);
        Udp moved(std::move(first));check(!first && moved);Udp assigned;assigned=std::move(moved);check(!moved&&assigned);
        assigned=std::move(assigned);check(bool(assigned));check(assigned.Send(std::span<const uint8_t>(data.data(),1),b));
        check(receive(second,small,source).kind==ReceiveKind::Packet && Same(source,a));
        assigned.Close();assigned.Close();check(!assigned);check(!assigned.Send(data,b));
        check(assigned.Receive(small,source).kind==ReceiveKind::Error);check(assigned.Open(0,error,BindScope::Loopback));
        second=std::move(assigned);check(!assigned&&second);second.Close();
    }
#ifdef _WIN32
    SOCKET uninitialised=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    check(uninitialised==INVALID_SOCKET && WSAGetLastError()==WSANOTINITIALISED);
    if(uninitialised!=INVALID_SOCKET)closesocket(uninitialised);
#endif
    std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"cycles\":8,\"network\":\"loopback-only\"}\n";
}
