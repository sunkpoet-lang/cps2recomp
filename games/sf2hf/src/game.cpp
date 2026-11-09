// SPDX-License-Identifier: GPL-3.0-only
// Read-only native result mapping, independently traced for sf2hf.
#include "game.h"
#include "../../../fight/classic_sf2_result.h"
namespace cps2recomp {
namespace {
int MatchResult() {
    return classic_sf2::MatchResult(Ram16(0xff8004), Ram16(0xff8008),
        Ram8(0xff864c), Ram8(0xff894c), Ram8(0xff864e), Ram8(0xff894e));
}
}
void RegisterGame() { TheGame().onlineMatchResult = MatchResult; }
}
