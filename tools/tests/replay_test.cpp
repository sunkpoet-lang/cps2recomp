// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's native-replays 1.2; replay-compatibility 2.1) The .fareplay file: format 2 written and read
// back equal (its backend, identity, version, four players and 4-byte inputs too); a format-1 file as 0.1.2 wrote
// it still read; a cut file, a wrong magic, a newer format, a header over its length and damaged data each refused.
//   make check-replay
#include "../../port/replay.h"

#include <cstdio>
#include <string>
#include <zlib.h>

using port::Replay;

static int failures = 0;
static void Check(bool ok, const char *what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    failures += !ok;
}

static Replay Sample() {
    Replay r;
    r.info.game = "ssf2t";
    r.info.title = "Super Street Fighter II Turbo";
    r.info.build = std::string(64, 'a');
    r.info.forkade = "0.1.0-beta";
    r.info.identity = std::string(64, 'b');
    r.info.version = "0.1.3";
    r.info.mode = "online";
    r.info.role = "host";
    r.info.players = {"Daniel", "Iñaki \"Fabs\""};
    r.info.started = "2026-11-11T20:41:00-06:00";
    r.info.frames = 21345;
    r.info.result = true;
    r.info.wins = {2, 1};
    r.info.features = "widescreen=1,no_damage=0,skip_intro=1";
    r.info.online = true;
    for (int i = 0; i < 350000; i++) {
        r.state.push_back(static_cast<uint8_t>((i * 7) & 0xff));
    }
    r.players = 2;
    r.inputs[0] = {0, 0, 0, 0};
    r.inputs[12] = {0x0010, 0, 0, 0};
    r.inputs[300] = {0x0010, 0x8001, 0, 0};
    r.inputs[100000] = {0, 0xffff, 0, 0};
    for (uint64_t f = 60; f <= 21345; f += 60) {
        r.checks.emplace_back(f, static_cast<uint32_t>(f * 2654435761u));
    }
    r.checks.emplace_back(21345, 0xdeadbeef);
    return r;
}

int main() {
    const Replay a = Sample();
    const std::vector<uint8_t> bytes = a.Bytes();
    std::printf("(a sample of %zu bytes)\n", bytes.size());
    Replay b;
    std::string error;
    Check(b.Read(bytes, error), "a written replay reads back");
    Check(b.info.game == a.info.game && b.info.title == a.info.title && b.info.build == a.info.build && b.info.forkade == a.info.forkade &&
              b.info.mode == a.info.mode && b.info.role == a.info.role && b.info.players == a.info.players &&
              b.info.started == a.info.started && b.info.frames == a.info.frames && b.info.result && b.info.wins == a.info.wins &&
              b.info.features == a.info.features && b.info.online,
          "its header is the same (names with quotes and accents too)");
    Check(b.info.format == 2 && b.info.backend == "cps" && b.info.identity == a.info.identity && b.info.version == "0.1.3" &&
              b.info.inputBytes == 2 && b.info.inputPlayers == 2,
          "format 2: its backend, identity, version and inputs' width are kept");
    Check(b.state == a.state, "its state is the same");
    Check(b.players == a.players && b.inputs == a.inputs, "its inputs are the same");
    Check(b.checks == a.checks, "its checksums are the same");

    auto refused = [&](std::vector<uint8_t> broken, const char *reason, const char *what) {
        Replay c;
        std::string why;
        const bool read = c.Read(broken, why);
        Check(!read && why.find(reason) != std::string::npos, what);
        if (read || why.find(reason) == std::string::npos) {
            std::printf("     (said: %s)\n", why.c_str());
        }
    };
    refused(std::vector<uint8_t>(bytes.begin(), bytes.begin() + bytes.size() / 2), "cut short", "a cut file is refused: cut short");
    refused(std::vector<uint8_t>(bytes.begin(), bytes.begin() + 10), "cut short", "a file cut in its start is refused");
    {
        std::vector<uint8_t> wrong = bytes;
        wrong[0] = 'X';
        refused(wrong, "not a ForkAde replay", "a wrong magic is refused: not a ForkAde replay");
    }
    {
        std::vector<uint8_t> newer = bytes;
        newer[8] = 3;
        refused(newer, "newer ForkAde", "a newer format is refused: update ForkAde");
    }
    {
        std::vector<uint8_t> big = bytes;
        big[12] = 0xff; big[13] = 0xff; big[14] = 0xff; big[15] = 0x00;
        refused(big, "header is too long", "a header over its length is refused");
    }
    {
        std::vector<uint8_t> damaged = bytes;
        const size_t header = damaged[12] | (damaged[13] << 8);
        damaged[16 + header + 8 + 20] ^= 0xff;  // (inside the compressed state)
        refused(damaged, "damaged", "damaged data is refused");
    }
    {
        // Four players, 4 bytes each
        Replay four = Sample();
        four.players = 4;
        four.info.inputBytes = 4;
        four.inputs[500] = {1, 2, 0x8000, 0xffff};
        Replay back;
        Check(back.Read(four.Bytes(), error) && back.players == 4 && back.inputs == four.inputs && back.info.inputBytes == 4 &&
                  back.info.inputPlayers == 4,
              "four players with 4-byte inputs read back equal");
    }
    {
        // A format-1 file as 0.1.2 wrote it: backend 1 in the fixed start, no identity, two bytes a player
        auto put16 = [](std::vector<uint8_t> &o, unsigned v) { o.push_back(v & 0xff); o.push_back((v >> 8) & 0xff); };
        auto put32 = [](std::vector<uint8_t> &o, uint32_t v) { for (int i = 0; i < 4; i++) o.push_back((v >> (8 * i)) & 0xff); };
        auto part = [&](std::vector<uint8_t> &o, const std::vector<uint8_t> &raw) {
            uLongf size = compressBound(static_cast<uLong>(raw.size())); std::vector<uint8_t> z(size);
            compress2(z.data(), &size, raw.data(), static_cast<uLong>(raw.size()), 6);
            put32(o, static_cast<uint32_t>(raw.size())); put32(o, static_cast<uint32_t>(size)); o.insert(o.end(), z.begin(), z.begin() + size);
        };
        std::vector<uint8_t> old = {'F', 'A', 'R', 'E', 'P', 'L', 'A', 'Y'};
        put16(old, 1); put16(old, 1);
        const std::string header = "{\"game\":\"ssf2t\",\"title\":\"Super Street Fighter II Turbo\",\"build\":\"" + std::string(64, 'c') +
                                   "\",\"forkade\":\"0.1.2-beta\",\"mode\":\"local\",\"players\":[\"A\",\"B\"],\"started\":\"x\",\"frames\":120,\"features\":\"\",\"online\":false}";
        put32(old, static_cast<uint32_t>(header.size())); old.insert(old.end(), header.begin(), header.end());
        part(old, std::vector<uint8_t>(1000, 7));
        part(old, {2, 0, 0x10, 0, 0, 0, 5, 0x20, 0, 0, 0}); // (2 players; frame 0: 0x0010, 0; frame 5: 0x0020, 0)
        put32(old, 1); for (int i = 0; i < 8; i++) old.push_back(i == 0 ? 60 : 0); put32(old, 0x12345678);
        Replay legacy;
        Check(legacy.Read(old, error) && legacy.info.format == 1 && legacy.info.backend == "cps" && legacy.info.identity.empty() &&
                  legacy.info.build == std::string(64, 'c') && legacy.players == 2 && legacy.inputs.size() == 2 && legacy.inputs.at(5)[1] == 0 &&
                  legacy.inputs.at(5)[0] == 0x20 && legacy.checks.size() == 1,
              "a format-1 replay of 0.1.2 still reads (backend cps, no identity)");
        if (!error.empty()) std::printf("     (said: %s)\n", error.c_str());
    }
    // (a file on disk, and only its header)
    const std::string path = "replay_test.fareplay";
    Check(a.Save(path, error), "it saves to a file");
    Replay d;
    Check(d.Load(path, error) && d.inputs == a.inputs, "it loads from the file");
    Replay::Info info;
    Check(Replay::ReadInfo(path, info, error) && info.players == a.info.players && info.result, "its header alone reads");
    std::remove(path.c_str());
    std::printf(failures ? "replay_test: %d FAILED\n" : "replay_test: PASSED\n", failures);
    return failures != 0;
}
