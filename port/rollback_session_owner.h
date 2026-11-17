// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 the fork's contributors. Backend-neutral Gekko session ownership.
#pragma once

#include <gekkonet.h>
#include <utility>

namespace recomp::rollback {

class Session {
public:
    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Session& operator=(Session&& other) noexcept {
        if (this != &other) {
            Reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }
    ~Session() { Reset(); }

    explicit operator bool() const noexcept { return handle_ != nullptr; }
    GekkoSession* Get() const noexcept { return handle_; }

    bool Start(GekkoSessionType type, GekkoConfig config, GekkoNetAdapter* adapter = nullptr,
               unsigned timeout = 0) {
        if (handle_ || config.num_players < 2 || config.num_players > 4 || !config.input_size ||
            config.input_size > 64 || !config.state_size ||
            (type != GekkoGameSession && type != GekkoStressSession && type != GekkoSpectateSession)) {
            return false;
        }
        if (!gekko_create(&handle_, type)) {
            Reset();
            return false;
        }
        if (!handle_) return false;
        gekko_start(handle_, &config);
        if (adapter) gekko_net_adapter_set(handle_, adapter);
        if (timeout) gekko_set_disconnect_timeout(handle_, timeout);
        return true;
    }

    int AddActor(GekkoPlayerType type, GekkoNetAddress* address = nullptr) {
        if (!handle_ || (type != GekkoLocalPlayer && type != GekkoRemotePlayer && type != GekkoSpectator) ||
            (type != GekkoLocalPlayer && (!address || !address->data || !address->size))) {
            return -1;
        }
        return gekko_add_actor(handle_, type, address);
    }

    bool Delay(int actor, unsigned delay) {
        if (!handle_ || actor < 0 || delay > 255) return false;
        gekko_set_local_delay(handle_, actor, static_cast<unsigned char>(delay));
        return true;
    }

    void Reset() noexcept {
        if (handle_) gekko_destroy(&handle_);
    }

private:
    GekkoSession* handle_ = nullptr;
};

} // namespace recomp::rollback
