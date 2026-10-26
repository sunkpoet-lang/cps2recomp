// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II: what the game's sources add to the toolkit. For now only its music, for a custom
// soundtrack (music.cpp); everything else is the game as the board plays it.
#include "game.h"

namespace cps2recomp {

void RegisterMusic(); // music.cpp

void RegisterGame() {
    RegisterMusic();
}

} // namespace cps2recomp
