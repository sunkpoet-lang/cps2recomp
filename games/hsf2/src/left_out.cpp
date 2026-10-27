// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II: what Super Turbo's files this game leaves out (game.cfg's sources_leave_out) give the ones it
// builds. Widescreen (widescreen.cpp): the picture is the board's, with nothing at its sides. The tag team battle
// (livetag.cpp): no fighter is ever swapped in, so the team battle's tag style is not there; its knock-out style is
// the game's own rounds.
#include "machine.h"

#include <cstdint>

namespace cps2recomp {

// widescreen.cpp
int WideMargin() {
    return 0;
}
int WideSide() {
    return 0;
}
int WideStage() {
    return 0;
}
void WideForSpare(Machine &) {}
void WideForPort(Machine &) {}

// livetag.cpp
bool CanTag(int) {
    return false;
}
bool TaggedIn(int) {
    return true;
}
void TagIn(int, int, int) {}
void TagDifficulty(int) {}
int TagDifficultyNow() {
    return 0;
}
void TagOffStage(int, bool) {}
void TagUntouchable(int) {}
void TagKnockDown(uint32_t) {}

} // namespace cps2recomp
