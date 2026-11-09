// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <map>

namespace port {
// Presentation only: never part of a board archive, checksum or network packet.
class MatchFlow {
public:
    struct State {
        std::array<bool, 2> joined{};
        std::array<int, 2> streak{}, rounds{};
        bool counted = false;
        int quiet = 0;
    };
    void Reset() { state = {}; checkpoints.clear(); }
    void Save(int frame) {
        checkpoints[frame] = state;
        while (checkpoints.size() > 128) checkpoints.erase(checkpoints.begin());
    }
    bool Load(int frame) {
        const auto found = checkpoints.find(frame);
        if (found == checkpoints.end()) return false;
        state = found->second;
        checkpoints.erase(checkpoints.upper_bound(frame), checkpoints.end());
        return true;
    }
    void Advance(const uint16_t players[2], uint16_t start, int result, int roundsToWin) {
        for (int side = 0; side < 2; ++side) state.joined[side] = state.joined[side] || (players[side] & start);
        if (result < 0 || result > 3 || roundsToWin < 1) return;
        if (result == 0) {
            if (state.quiet < 60) ++state.quiet;
            if (state.quiet >= 60) state.counted = false;
            return;
        }
        state.quiet = 0;
        if (state.counted) return;
        state.counted = true;
        if (result == 3 || !state.joined[0] || !state.joined[1]) return;
        const int winner = result - 1;
        if (++state.rounds[winner] >= roundsToWin) {
            ++state.streak[winner];
            state.streak[1 - winner] = 0;
            state.rounds = {};
        }
    }
    State state;
private:
    std::map<int, State> checkpoints;
};
} // namespace port
