// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
namespace recomp::rollback {
using Pads = std::array<uint16_t, 4>;
inline bool DecodeInputs(std::span<const uint8_t> bytes, size_t players, Pads& pads) {
    pads.fill(0);
    if (players < 2 || players > pads.size() || bytes.size() != players * sizeof(uint16_t) || !bytes.data()) return false;
    std::memcpy(pads.data(), bytes.data(), bytes.size());
    return true;
}
inline bool BoardInputs(std::span<const uint16_t> inputs, std::span<uint16_t> board) {
    std::fill(board.begin(), board.end(), uint16_t{0});
    if (inputs.size() < 2 || inputs.size() > 4 || inputs.size() > board.size() || !inputs.data()) return false;
    std::copy(inputs.begin(), inputs.end(), board.begin());
    return true;
}
inline bool LocalInputs(std::span<const uint16_t> local, unsigned mask, unsigned players, Pads& pads) {
    pads.fill(0);
    if (players < 2 || players > 4 || !mask || (mask >> players) || std::popcount(mask) > 2 || local.size() != static_cast<size_t>(std::popcount(mask)) || !local.data()) return false;
    size_t at = 0;
    for (unsigned seat = 0; seat < players; ++seat) if (mask & (1u << seat)) pads[seat] = local[at++];
    return true;
}

}
