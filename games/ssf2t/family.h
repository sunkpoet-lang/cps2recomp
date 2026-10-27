// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Super Street Fighter II Turbo as one of the games made from its program, which build its sources (src/): what is its
// own. The places its sources name are its own, so there is no list of them (src/places.h).
#pragma once

#include <array>
#include <cstdint>

namespace ssf2t::family {

struct Place {
    uint32_t ssf2t, here;
};
inline constexpr std::array<Place, 0> kPlaces = {};

// The game's steps (src/ssf2t.h: kMode, kGameStep, kMatchStep), traced with --watch through a one-player game and the
// show with no coin in. A game: kMode 2, under it 0 while the players choose and 4 for the match, under that 0A while
// a round is fought. The show: 2 the introduction and the title (the title from kGameStep 16), 0E a demonstration
// fight, 18 the end of the loop; 10 the board's start's last step (frame 805).
inline constexpr uint16_t kModeGame = 2, kModeChoosing = 2, kGameChoosing = 0, kGameMatch = 4, kMatchFought = 0x0A;
inline constexpr uint16_t kIntro = 0x02, kDemo = 0x0E, kLoopEnd = 0x18, kBooted = 0x10, kTitle = 0x16;
inline constexpr int kBootFrames = 810;
// Frames a stage's machine (the battle modes') takes from switching on to a round, at most, and a few
inline constexpr int kStageToRound = 2000;

// The players' records (src/ssf2t.h): the bytes past 2B4 that are not where Hyper Street Fighter II has them
inline constexpr uint32_t kChosen = 0x284, kSide = 0x381, kPerson = 0x38E, kCharacter = 0x391, kOpponent = 0x39E;
inline constexpr uint32_t kColourChoice = 0x3B1, kOld = 0x3B6, kAkuma = 0x3BD;
// The character choice's tune
inline constexpr uint16_t kTuneChoose = 0xD3;

// The boxes (src/ssf2t.h, src/training.cpp): the frame's bytes that name a box of each kind, where each kind's table is
// in the record's box tables, and a box's size. A box is four signed bytes (its middle across and up from where the
// player stands, half its width and half its height); an attack's byte 5 with its top bit set reaches that far
// across instead. Three boxes it can be hit in (no fourth: 0).
inline constexpr uint32_t kFrameHead = 0x08, kFrameBody = 0x09, kFrameLegs = 0x0A, kFrameFourth = 0, kFrameAttack = 0x0C,
                          kFramePush = 0x0D;
inline constexpr uint32_t kTableHead = 0x0, kTableBody = 0x2, kTableLegs = 0x4, kTableFourth = 0, kTableAttack = 0x6,
                          kTablePush = 0x8;
inline constexpr uint32_t kBoxSize = 4, kAttackBoxSize = 0x10;
inline constexpr bool kBoxWords = false, kLongReach = true;

} // namespace ssf2t::family
