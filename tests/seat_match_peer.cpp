// SPDX-License-Identifier: GPL-3.0-only
// One PC of a seat match on a fake deterministic board (port/seat_match.h), for tools/tests/seat_match_check.sh:
//   seat_match_peer host|join PORT FRAMES SEED CHECKPOINT-LOG [late] [start N] [capacity N]
// The board's state is a 64-bit value that every frame mixes with its input lanes; its hash is that value's CRC.
#include "seat_match.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <zlib.h>

int main(int argc, char **argv) {
    if (argc < 6) { std::fprintf(stderr, "usage: seat_match_peer host|join PORT FRAMES SEED LOG [late] [start N] [capacity N]\n"); return 2; }
    const bool host = !std::strcmp(argv[1], "host");
    recomp::match::Options o;
    o.host = host; o.port = uint16_t(std::atoi(argv[2])); o.address = "127.0.0.1";
    const uint64_t frames = std::strtoull(argv[3], nullptr, 10);
    uint64_t seed = std::strtoull(argv[4], nullptr, 10);
    o.checkpointLog = std::fopen(argv[5], "w");
    o.capacity = 3;
    for (int i = 6; i < argc; ++i) {
        if (!std::strcmp(argv[i], "late")) o.lateJoins = true;
        else if (!std::strcmp(argv[i], "start") && i + 1 < argc) o.startPlayers = unsigned(std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "capacity") && i + 1 < argc) o.capacity = unsigned(std::atoi(argv[++i]));
    }
    o.name = host ? "Host" : "Guest" + std::to_string(seed);
    const std::string profile = "fake-board-1";
    o.profile.assign(profile.begin(), profile.end());
    o.build = std::string(64, 'a');
    o.stateBytes = 64;
    o.scope = recomp::net::BindScope::Loopback;

    uint64_t board = 0x12345678;
    recomp::match::Backend b;
    b.save = [&](std::vector<uint8_t> &s) { s.resize(8); std::memcpy(s.data(), &board, 8); return true; };
    b.load = [&](std::span<const uint8_t> s) { if (s.size() != 8) return false; std::memcpy(&board, s.data(), 8); return true; };
    b.frame = [&](std::span<const uint16_t> lanes, bool) {
        for (size_t i = 0; i < lanes.size(); ++i) board = board * 6364136223846793005ull + lanes[i] * (i + 3) + 1442695040888963407ull;
        return true;
    };
    b.hash = [&] { return uint32_t(crc32(0, reinterpret_cast<const Bytef *>(&board), 8)); };

    recomp::match::SeatMatch m;
    std::string error;
    if (!m.Start(o, b, error)) { std::fprintf(stderr, "start: %s\n", error.c_str()); return 3; }
    uint64_t shown = 0;
    auto rng = [&] { seed = seed * 6364136223846793005ull + 1; return uint16_t(seed >> 48); };
    uint16_t held = 0;
    const auto begun = std::chrono::steady_clock::now();
    while (shown < frames && m.GetStatus() != recomp::match::SeatMatch::Status::Ended) {
        if (std::chrono::steady_clock::now() - begun > std::chrono::seconds(240)) { std::fprintf(stderr, "timeout\n"); return 4; }
        if (rng() % 8 == 0) held = rng();
        const uint16_t local[1] = {held};
        shown += uint64_t(m.Tick(std::span<const uint16_t>(local, 1)));
        std::this_thread::sleep_for(std::chrono::microseconds(16700));
    }
    std::fprintf(stderr, "seat_match_peer: %llu frames, global frame %llu, epoch %llu, %s\n", static_cast<unsigned long long>(shown),
                 static_cast<unsigned long long>(m.Frame()), static_cast<unsigned long long>(m.Epoch()), m.Message().c_str());
    std::fclose(o.checkpointLog);
    return m.Failed() ? 1 : 0;
}
