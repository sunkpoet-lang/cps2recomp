// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's native-replays 1.2) The .fareplay file: written and read back equal; a cut file, a wrong
// magic, a newer format, a header over its length and damaged data each refused with their reason.
//   make check-replay
#include "../../port/replay.h"

#include <cstdio>
#include <string>

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
        newer[8] = 2;
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
