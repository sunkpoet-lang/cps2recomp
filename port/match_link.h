// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (this fork). Read-only match flow shared by both transports.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace port {

struct MatchView {
    enum class Phase { Waiting, Connecting, Playing, Ended } phase = Phase::Waiting;
    bool hosting = false, arcade = false, found = false, hostIn = false;
    bool failed = false, left = false, lost = false, listed = false;
    int local = 0, ping = 0, delay = 0;
    // Unknown is distinct from a peer that was just heard: not every transport exposes it.
    std::optional<uint64_t> silentMs;
    std::array<std::string, 2> names;
    std::string message, code, invited, invitation, stats;
};

class MatchLink {
public:
    virtual ~MatchLink() = default;
    // Observation only: never polls the network, resets the board, runs frames or restores a state.
    virtual MatchView ReadMatch() const = 0;
};

} // namespace port
