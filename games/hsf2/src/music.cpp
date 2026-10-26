// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II: its music, for a custom soundtrack (port/soundtrack.h, machine/soundtrack.h). The sound
// program keeps its table of songs as Super Turbo's does, at 0x9006, four bytes a song: the word at 0x9000 is how many
// (0x280, more effects and voices than Super Turbo's 0x1FF), and a song whose data starts with a 0 is music. Its 68
// pieces of music have the same numbers as Super Turbo's, so they have Super Turbo's names (games/ssf2t/src/music.cpp)
// and a soundtrack made for one plays for the other, but for player select: here 0x34 (the screen with the choice of
// version), where Super Turbo's is 0xD3, which this game was not seen to ask for.
#include "game.h"

namespace cps2recomp {

void RegisterMusic() {
    TheGame().songTable = 0x9006;
    TheGame().songCount = 0x280;
    // Found by made-up play (CPS2RECOMP_SOUNDTRACK_LOG, with pictures where each starts): a stage's tune is the stage's,
    // whoever fights there, and has a second version, sixteen on, that the game goes to later in a round. Seen here: the
    // stages of Ryu, Guile, Dhalsim, Cammy and T. Hawk and three later versions, player select, versus and the
    // winner; the others are named as Super Turbo has them, not yet seen in this game.
    TheGame().songs = {
        // Each stage's
        {0x02, "ryu", "Ryu's stage"},
        {0x03, "honda", "E. Honda's stage"},
        {0x05, "blanka", "Blanka's stage"},
        {0x07, "guile", "Guile's stage"},
        {0x01, "ken", "Ken's stage"},
        {0x04, "chunli", "Chun-Li's stage"},
        {0x06, "zangief", "Zangief's stage"},
        {0x08, "dhalsim", "Dhalsim's stage"},
        {0x0C, "bison", "M. Bison's stage"},
        {0x0B, "sagat", "Sagat's stage"},
        {0x09, "balrog", "Balrog's stage"},
        {0x0A, "vega", "Vega's stage"},
        {0x0E, "cammy", "Cammy's stage"},
        {0x0F, "thawk", "T. Hawk's stage"},
        {0x0D, "feilong", "Fei Long's stage"},
        {0x10, "deejay", "Dee Jay's stage"},
        // ... and their later versions in a round
        {0x12, "ryu_later", "Ryu's stage, later in a round"},
        {0x13, "honda_later", "E. Honda's stage, later in a round"},
        {0x15, "blanka_later", "Blanka's stage, later in a round"},
        {0x17, "guile_later", "Guile's stage, later in a round"},
        {0x11, "ken_later", "Ken's stage, later in a round"},
        {0x14, "chunli_later", "Chun-Li's stage, later in a round"},
        {0x16, "zangief_later", "Zangief's stage, later in a round"},
        {0x18, "dhalsim_later", "Dhalsim's stage, later in a round"},
        {0x1C, "bison_later", "M. Bison's stage, later in a round"},
        {0x1B, "sagat_later", "Sagat's stage, later in a round"},
        {0x19, "balrog_later", "Balrog's stage, later in a round"},
        {0x1A, "vega_later", "Vega's stage, later in a round"},
        {0x1E, "cammy_later", "Cammy's stage, later in a round"},
        {0x1F, "thawk_later", "T. Hawk's stage, later in a round"},
        {0x1D, "feilong_later", "Fei Long's stage, later in a round"},
        {0x20, "deejay_later", "Dee Jay's stage, later in a round"},
        // The last fight of the ladder, against M. Bison
        {0xD5, "final_fight", "The final fight"},
        {0xD6, "final_fight_later", "The final fight, later in a round"},
        // Between the fights
        {0xD4, "title", "The introduction and the title", true},
        {0x3D, "qsound", "The QSound notice", true},
        {0x34, "player_select", "Player select (and the choice of version)"},
        {0x35, "versus", "Versus screen", true},
        {0x36, "winner", "Winner (the winner's quote)", true},
        {0x37, "continue", "Continue (the countdown)"},
        {0x39, "game_over", "Game over", true},
        {0x3A, "ranking", "Score ranking", true},
        // The ending and the staff roll
        {0x23, "ryu_ending", "Ryu's ending"},
        {0x3C, "staff_roll", "Staff roll"},
    };
}

} // namespace cps2recomp
