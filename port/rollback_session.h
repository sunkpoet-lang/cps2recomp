// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 the fork's contributors. Backend-neutral rollback event execution.
#pragma once

#include <gekkonet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <vector>

namespace recomp::rollback {

struct Limits {
    unsigned players = 2;
    unsigned inputBytes = 2;
    size_t stateBytes = 0;
};

struct Frame {
    int number;
    bool rollback;
    bool runahead;
};

enum class Error { Configuration, Event, SaveBuffer, StateCapacity, LoadBuffer, InputBuffer };

struct Callbacks {
    std::function<bool()> active;
    std::function<bool(int, std::vector<uint8_t>&)> save;
    std::function<uint32_t(int)> checksum;
    std::function<bool(int, std::span<const uint8_t>)> load;
    std::function<bool(Frame, std::span<const uint8_t>)> advance;
    std::function<void(Error)> error;
};

// Own-state copying and event order are shared; board semantics and external effects belong to the backend.
inline bool Dispatch(GekkoGameEvent* const* events, int count, Limits limits,
                     std::vector<uint8_t>& state, const Callbacks& callbacks) {
    auto fail = [&](Error error) {
        if (callbacks.error) callbacks.error(error);
        return false;
    };
    if (limits.players < 2 || limits.players > 4 || !limits.inputBytes || limits.inputBytes > 64 ||
        !limits.stateBytes || count < 0 || (count && !events) || !callbacks.active ||
        !callbacks.save || !callbacks.checksum || !callbacks.load || !callbacks.advance) {
        return fail(Error::Configuration);
    }
    for (int i = 0; i < count; ++i) {
        if (!callbacks.active()) return false;
        if (!events[i]) return fail(Error::Event);
        auto& event = *events[i];
        switch (event.type) {
        case GekkoSaveEvent:
            if (!event.data.save.state || !event.data.save.state_len || !event.data.save.checksum) {
                return fail(Error::SaveBuffer);
            }
            if (!callbacks.save(event.data.save.frame, state)) return false;
            if (state.size() > limits.stateBytes || state.size() > UINT32_MAX) {
                return fail(Error::StateCapacity);
            }
            if (!state.empty()) std::memcpy(event.data.save.state, state.data(), state.size());
            *event.data.save.state_len = static_cast<unsigned int>(state.size());
            *event.data.save.checksum = callbacks.checksum(event.data.save.frame);
            break;
        case GekkoLoadEvent:
            if (!event.data.load.state || event.data.load.state_len > limits.stateBytes) {
                return fail(Error::LoadBuffer);
            }
            if (!callbacks.load(event.data.load.frame, {event.data.load.state, event.data.load.state_len})) {
                return false;
            }
            break;
        case GekkoAdvanceEvent:
            if (!event.data.adv.inputs || event.data.adv.input_len != limits.players * limits.inputBytes) {
                return fail(Error::InputBuffer);
            }
            if (!callbacks.advance({event.data.adv.frame, event.data.adv.rolling_back,
                                    event.data.adv.running_ahead},
                                   {event.data.adv.inputs, event.data.adv.input_len})) {
                return false;
            }
            break;
        default:
            break;
        }
    }
    return true;
}

inline bool Step(GekkoSession* session, Limits limits, std::vector<uint8_t>& state,
                 const Callbacks& callbacks) {
    if (!session || (callbacks.active && !callbacks.active())) return false;
    int count = 0;
    auto events = gekko_update_session(session, &count);
    return Dispatch(events, count, limits, state, callbacks);
}

} // namespace recomp::rollback
