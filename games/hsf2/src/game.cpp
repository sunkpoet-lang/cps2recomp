// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II: what the game's sources add to the toolkit. Most of it is Super Street Fighter II Turbo's
// sources (games/ssf2t/src, built for this game: game.cfg's sources, the places in ../family.h): training mode, its
// dummy, the alternative modes (attract mode, the stage viewer, survival, endless battle, ring out), free play,
// straight to the title, the Board and the Cheats pages. Left out of them (game.cfg's sources_leave_out): the tag
// team battle, the model viewer, widescreen, custom colours and Super Turbo's hidden characters. Its own: its music,
// for a custom soundtrack (music.cpp), and what the left-out files gave the others (left_out.cpp).
#include "alternative.h"
#include "battle.h"
#include "game.h"
#include "session.h"

namespace cps2recomp {

void RegisterCoins(); // coins.cpp
void CoinsFrame(Machine &machine);
void RegisterCheats(); // cheats.cpp
void RegisterTitle(); // title.cpp
void TitleFrame(Machine &machine);
void RegisterBoard(); // board.cpp
void BoardBeforeFrame(Machine &machine);
void BoardState(Archive &ar);
void RegisterMusic();    // music.cpp (this game's)
void RegisterModes();    // modes.cpp
void RegisterTraining(); // training.cpp
void RegisterBattle();   // battle.cpp
void RegisterComputer(); // computer.cpp

void RegisterGame() {
    RegisterModes();
    RegisterTraining();
    RegisterBattle();
    RegisterComputer();
    RegisterCoins();
    RegisterCheats();
    RegisterTitle();
    RegisterBoard();
    RegisterMusic();
    TheGame().beforeFrame = [](Machine &machine) {
        BoardBeforeFrame(machine);
        session::BeforeFrame(machine);
        modes::BeforeFrame(machine);
        BattleBeforeFrame(machine);
    };
    // (the training aids see what the frame did before the cheats put things back)
    TheGame().frame = [](Machine &machine) {
        session::Frame(machine);
        modes::Frame(machine);
        BattleFrame(machine);
        TitleFrame(machine);
        CoinsFrame(machine);
    };
    // (run-ahead: not in the modes, whose workings a save state does not wholly hold)
    TheGame().noRunAhead = [] { return modes::AttractMode() || modes::StageViewer() || BattleMode(); };
    // (what the sources keep between frames, with save states and run-ahead's frames taken back)
    TheGame().state = [](Archive &ar) {
        BoardState(ar);
        session::State(ar);
        modes::State(ar);
        BattleState(ar);
    };
    // With the arcade timing, the game's own code runs in place of the rewrites but where they carry what the port adds
    TheGame().rewritesNeeded = [] { return session::InSession() || modes::AttractMode() || modes::StageViewer() || BattleMode(); };
    // Its own, never set for the other games by the collection's launcher
    OwnFeatures({"survival_best", "ringout_best"});
}

} // namespace cps2recomp
