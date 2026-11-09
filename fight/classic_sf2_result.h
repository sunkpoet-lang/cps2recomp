// SPDX-License-Identifier: GPL-3.0-only
#pragma once
namespace cps2recomp::classic_sf2 {
// Inputs are native 68000 values, not byte indices in another emulator's RAM.
// Completed match units: the game has already awarded its two round wins.
inline int MatchResult(int mode, int phase, int human1, int human2, int wins1, int wins2) {
    if (mode != 4 || human1 != 1 || human2 != 1 || wins1 > 2 || wins2 > 2 || wins1 < 0 || wins2 < 0) return 0;
    if (phase != 0x0a && phase != 0x0c && phase != 0x0e && phase != 0x10 && phase != 0x12) return 0;
    if (wins1 == 2 && wins2 == 2) return 3;
    if (wins1 == 2) return 1;
    if (wins2 == 2) return 2;
    return 0;
}
}
