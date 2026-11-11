// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's native-replays) A ForkAde replay file, .fareplay: what a match was (a JSON header), the
// machine's state when the recording began, every player's confirmed inputs frame by frame, and frame checksums at
// intervals, so that playback can tell when it no longer matches. Little-endian:
//   "FAREPLAY", format (uint16, 1), backend (uint16, 1: the CPS games)
//   header: length (uint32), UTF-8 JSON
//   state: raw length, compressed length (uint32 each), zlib (the port's save state)
//   inputs: raw length, compressed length, zlib of: players (uint8), then each change: frame delta (varint), each
//           player's pressed bits (uint16)
//   checksums: count (uint32), then each: frame (uint64), hash (uint32)
// Tommy's text movie (--record, --movie) is unchanged; this is ForkAde's file beside it.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace port {

struct Replay {
    static constexpr uint16_t kFormat = 1;
    static constexpr uint16_t kBackendCps = 1;
    using Pads = std::array<uint16_t, 4>;

    // The header's fields (and the JSON they are kept as)
    struct Info {
        std::string game, title, build, forkade;  // (build: the game program's SHA-256, as the launcher computes it)
        std::string mode = "local";                // "local" or "online"
        std::string role;                          // online: "host" or "guest"
        std::vector<std::string> players;          // (player 1's name first)
        std::string started;                       // (local time, ISO 8601 with its offset)
        uint64_t frames = 0;                       // (how many were recorded)
        bool result = false;                       // (the game said who won: wins)
        std::array<int, 2> wins = {0, 0};          // (matches won by each, as the online strip counts them)
        // The game's options when the recording began ("name=value,...", every one of the game's features as the
        // player had chosen it), which the save state does not hold, and whether online play's rules turned the
        // player's own ones off (Game::online): playback sets them so for its run
        std::string features;
        bool online = false;
    };

    uint16_t backend = kBackendCps;
    Info info;
    std::vector<uint8_t> state;                         // the save state at the first recorded frame
    int players = 2;                                    // (how many players the inputs have)
    std::map<uint64_t, Pads> inputs;                    // frame -> the pressed bits from that frame on
    std::vector<std::pair<uint64_t, uint32_t>> checks;  // frame -> its hash, every 60 frames and the last

    bool Save(const std::string &path, std::string &error) const;
    bool Load(const std::string &path, std::string &error);
    // (in memory: the file's bytes)
    std::vector<uint8_t> Bytes() const;
    bool Read(const std::vector<uint8_t> &bytes, std::string &error);
    // Only the header (for a list of replays: the rest is not read)
    static bool ReadInfo(const std::string &path, Info &info, std::string &error);

    static std::string HeaderJson(const Info &info);
    static bool ParseHeader(const std::string &json, Info &info);
};

}  // namespace port
