// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy and the fork's contributors.
#pragma once

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
#include <cerrno>
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <utility>

namespace recomp::net {

using Endpoint = sockaddr_in;
enum class BindScope { Any, Loopback };
enum class ReceiveKind { Packet, WouldBlock, PeerUnreachable, Truncated, Error };
struct Received { ReceiveKind kind; size_t size = 0; int error = 0; };

inline bool Same(const Endpoint& a, const Endpoint& b) {
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

class Udp {
#ifdef _WIN32
    using Handle = SOCKET;
    static constexpr Handle invalid = INVALID_SOCKET;
#else
    using Handle = int;
    static constexpr Handle invalid = -1;
#endif
public:
    static constexpr size_t maxDatagram = 65507;
    Udp() = default;
    Udp(const Udp&) = delete;
    Udp& operator=(const Udp&) = delete;
    Udp(Udp&& other) noexcept
        : handle_(std::exchange(other.handle_, invalid)), network_(std::exchange(other.network_, false)) {}
    Udp& operator=(Udp&& other) noexcept {
        if (this != &other) {
            Close();
            handle_ = std::exchange(other.handle_, invalid);
            network_ = std::exchange(other.network_, false);
        }
        return *this;
    }
    ~Udp() { Close(); }
    explicit operator bool() const noexcept { return handle_ != invalid; }

    bool Open(uint16_t port, std::string& error, BindScope scope = BindScope::Any) {
        if (handle_ != invalid || network_) { error = "the socket is already open"; return false; }
#ifdef _WIN32
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa)) { error = "the network could not be started"; return false; }
        network_ = true;
#endif
        handle_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (handle_ == invalid) { error = "no socket"; Close(); return false; }
        const int buffered = 4 << 20;
        setsockopt(handle_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&buffered), sizeof(buffered));
        setsockopt(handle_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&buffered), sizeof(buffered));
#ifdef _WIN32
        u_long nonblocking = 1;
        const bool nonblockingOk = ioctlsocket(handle_, FIONBIO, &nonblocking) == 0;
#else
        const int flags = fcntl(handle_, F_GETFL, 0);
        const bool nonblockingOk = flags >= 0 && fcntl(handle_, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
        if (!nonblockingOk) { error = "the socket could not be made non-blocking"; Close(); return false; }
        Endpoint at{};
        at.sin_family = AF_INET;
        at.sin_addr.s_addr = htonl(scope == BindScope::Loopback ? INADDR_LOOPBACK : INADDR_ANY);
        at.sin_port = htons(port);
        if (bind(handle_, reinterpret_cast<sockaddr*>(&at), sizeof(at))) {
            error = "UDP port " + std::to_string(port) + " is in use";
            Close();
            return false;
        }
        return true;
    }

    void Close() noexcept {
        if (handle_ != invalid) {
#ifdef _WIN32
            closesocket(handle_);
#else
            close(handle_);
#endif
            handle_ = invalid;
        }
#ifdef _WIN32
        if (network_) WSACleanup();
#endif
        network_ = false;
    }

    bool Local(Endpoint& at) const {
        if (handle_ == invalid) return false;
#ifdef _WIN32
        int size = sizeof(at);
#else
        socklen_t size = sizeof(at);
#endif
        return getsockname(handle_, reinterpret_cast<sockaddr*>(&at), &size) == 0;
    }

    bool Resolve(const std::string& host, uint16_t port, Endpoint& at, std::string& error) const {
        if (handle_ == invalid) { error = "the network socket is not open"; return false; }
        addrinfo hints{}, *found = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (host.empty() || getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) || !found) {
            if (found) freeaddrinfo(found);
            error = "the address " + host + " was not found";
            return false;
        }
        std::memcpy(&at, found->ai_addr, sizeof(at));
        freeaddrinfo(found);
        return true;
    }

    bool Send(std::span<const uint8_t> bytes, const Endpoint& to) const {
        if (handle_ == invalid || bytes.size() > maxDatagram || (!bytes.empty() && !bytes.data())) return false;
        return sendto(handle_, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0,
                      reinterpret_cast<const sockaddr*>(&to), sizeof(to)) == static_cast<int>(bytes.size());
    }

    Received Receive(std::span<uint8_t> bytes, Endpoint& from) const {
        if (handle_ == invalid || bytes.empty() || !bytes.data() || bytes.size() > 65536) {
            return {ReceiveKind::Error};
        }
#ifdef _WIN32
        int size = sizeof(from);
        const int n = recvfrom(handle_, reinterpret_cast<char*>(bytes.data()), static_cast<int>(bytes.size()), 0,
                               reinterpret_cast<sockaddr*>(&from), &size);
        if (n >= 0) return {ReceiveKind::Packet, static_cast<size_t>(n)};
        const int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) return {ReceiveKind::WouldBlock, 0, error};
        if (error == WSAECONNRESET) return {ReceiveKind::PeerUnreachable, 0, error};
        if (error == WSAEMSGSIZE) return {ReceiveKind::Truncated, 0, error};
#else
        socklen_t size = sizeof(from);
        const auto n = recvfrom(handle_, bytes.data(), bytes.size(), MSG_TRUNC,
                                reinterpret_cast<sockaddr*>(&from), &size);
        if (n > static_cast<decltype(n)>(bytes.size())) return {ReceiveKind::Truncated};
        if (n >= 0) return {ReceiveKind::Packet, static_cast<size_t>(n)};
        const int error = errno;
        if (error == EAGAIN || error == EWOULDBLOCK) return {ReceiveKind::WouldBlock, 0, error};
#endif
        return {ReceiveKind::Error, 0, error};
    }

private:
    Handle handle_ = invalid;
    bool network_ = false;
};

} // namespace recomp::net
