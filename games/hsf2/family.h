// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Hyper Street Fighter II as one of the games made from Super Street Fighter II Turbo's program, whose sources it
// builds (games/ssf2t/src; places.h there): where the places those sources name are in this game, and what of it is
// its own.
#pragma once

#include <array>
#include <cstdint>

namespace ssf2t::family {

struct Place {
    uint32_t ssf2t, here;
};
inline constexpr Place kPlaceList[] = {
    // places: begin (tools/places.py; a line ending "by hand" is kept)
    {0x002EE2, 0x0032D8}, // where the code calls or jumps to it (62/64), not its first instruction's place
    {0x002F50, 0x003344}, // an instruction, 15 on alike
    {0x00490A, 0x0049AE}, // the same bytes, once only
    {0x0049B0, 0x004A52}, // an instruction, 6 on alike
    {0x004A58, 0x004AFE}, // moved as the code before or after it, 4 on alike
    {0x008FFC, 0x008374}, // jsr to the settings' reading (Super Turbo's 00894C, here 007CC2): the same routine, by hand
    {0x00D69C, 0x00B1AE}, // addq.w #2,0x0(a5): a game begins (--watch FF8000 as Start was pressed), by hand
    {0x00D714, 0x00B1FA}, // the match's first step, the first entry of its table (B1EA, as Super Turbo's D700), by hand
    {0x00FE74, 0x00DA76}, // subq.b #1,0xBFD(a5): the round clock's second (the same code, the clock at FF8BFC), by hand
    {0x07804A, 0x06697E}, // moveq #0,d0 before the computer's step (jsr 06A142, Super Turbo's 07BB22), by hand
    {0x078052, 0x066986}, // the jsr after the computer's step (the same code), by hand
    {0x078270, 0x066B32}, // an instruction, 3 on alike
    {0x0782D4, 0x066BBE}, // an instruction, 4 on alike
    {0x07BB82, 0x06A1C8}, // the round's plan, the last bsr of that step (06A1A0; here with a check of its own first), by hand
    {0x07BC12, 0x06A246}, // an instruction, 9 on alike
    {0x07D6F4, 0x06BD16}, // the character's tables, the first bsr of the computer's first step (06A188; Super Turbo's 07BB40), by hand
    {0x07E36E, 0x06C922}, // an instruction, 4 on alike
    {0x07F1C6, 0x06D830}, // a place the code names so (2/2)
    {0x0BE646, 0x0C26A0}, // an instruction, 3 on alike
    {0x0BE678, 0x0C26CC}, // where a hit's damage goes on when it does not knock out (bpl after sub.w d6,0x2A(a2) at 0C26A0), by hand
    {0x0BEBA6, 0x0C2C90}, // sub.w d6,0x2A(a2): a thrown thing's damage (the second of the two), by hand
    {0x0BEBDA, 0x0C2CBE}, // where a thrown thing's damage goes on when it does not knock out (bpl at 0C2C94), by hand
    {0x0CCC5C, 0x0DA446}, // inside code that moved alike, 11 on alike
    {0x24EAE8, 0x316E3E}, // the same bytes, once only
    {0xFF0A7A, 0xFF0D00}, // a variable the code names so (3/3)
    {0xFF0A7E, 0xFF0D04}, // a variable the code names so (3/3)
    {0xFF8000, 0xFF8000}, // a variable the code names so (2/2)
    {0xFF8004, 0xFF8004}, // the game's steps are each game's own (family.h below): the words stay, by hand
    {0xFF8008, 0xFF8008}, // the same, by hand
    {0xFF800C, 0xFF800C}, // the same, by hand
    {0xFF8086, 0xFF8076}, // a variable the code names so (5/5)
    {0xFF808A, 0xFF807A}, // a variable the code names so (5/5)
    {0xFF82AE, 0xFF829E}, // a variable the code names so (1/1)
    {0xFF82B6, 0xFF82A6}, // a variable the code names so (1/1)
    {0xFF82D0, 0xFF0CB2}, // a variable the code names so (2/2)
    {0xFF82D3, 0xFF82BC}, // a variable the code names so (1/1)
    {0xFF82DA, 0xFF82BD}, // a variable the code names so (2/2)
    {0xFF82DD, 0xFF82BE}, // a variable the code names so (2/2)
    {0xFF82DF, 0xFF82C0}, // a variable the code names so (4/4)
    {0xFF82E9, 0xFF830A}, // credits: 0, 1, 2 with two coins (RAM dumps), by hand
    {0xFF82F0, 0xFF8316}, // a variable the code names so (2/2)
    {0xFF8342, 0xFF82C4}, // a variable the code names so (1/1)
    {0xFF83A9, 0xFF82C2}, // a variable the code names so (1/1)
    {0xFF844E, 0xFF833C}, // a variable the code names so (4/4)
    {0xFF8C4E, 0xFF8B64}, // a variable the code names so (10/10)
    {0xFF8C54, 0xFF8B50}, // a variable the code names so (3/3)
    {0xFF8C56, 0xFF8B52}, // a variable the code names so (3/3)
    {0xFF8C58, 0xFF8B54}, // a variable the code names so (3/3)
    {0xFF8C5A, 0xFF8B56}, // a variable the code names so (3/3)
    {0xFF8DCE, 0xFF8BFC}, // a variable the code names so (7/7)
    {0xFF8DCF, 0xFF8BFD}, // the round clock's frames, the byte after the clock (00DA76 counts it down), by hand
    {0xFF8DD2, 0xFF8C03}, // a variable the code names so (14/14)
    {0xFF8ECE, 0xFF8CBC}, // a variable the code names so (3/3)
    {0xFF8ED4, 0xFF8CC2}, // a variable the code names so (12/12)
    {0xFF8ED8, 0xFF8CC6}, // a variable the code names so (5/5)
    {0xFF8FEA, 0xFF8DD8}, // a variable between two that moved alike
    {0xFF8FEC, 0xFF8DDA}, // a variable between two that moved alike
    {0xFF8FEE, 0xFF8DDC}, // a variable between two that moved alike
    {0xFF8FF0, 0xFF8DDE}, // a variable between two that moved alike
    {0xFF97A2, 0xFF9554}, // the thrown things: lea 0x1554(a5),a6 at 0038AC sets up its 8 records as Super Turbo's 0034DE, by hand
    // places: end
};
inline constexpr auto kPlaces = std::to_array(kPlaceList);

// The game's steps (the words at FF8000, FF8004, FF8008; ssf2t.h: kMode, kGameStep, kMatchStep), traced with --trace
// through a one-player game and the show with no coin in. A game: kMode 2 while the players choose (kGameStep 2 the
// character and version, 4 the choice made), then 4 for the match: kGameStep 2 and 4 the versus screen, 6 a round's
// introduction, 8 the round (kMatchStep 2 while it is fought, 4 its end), 0A and 0C its result, then 6 again. The show:
// 2 and 4 the 15th anniversary's logo and Capcom's, 6 the introduction and the title (the title from kGameStep 20),
// 0A the QSound notice, 0C a demonstration fight, 0E the rankings, 10 the end of the loop; 10 is also the board's
// start's last step (frame 770; the loop begins at 826).
inline constexpr uint16_t kModeGame = 4, kModeChoosing = 2, kGameChoosing = 2, kGameMatch = 8, kMatchFought = 2;
inline constexpr uint16_t kIntro = 0x06, kDemo = 0x0C, kLoopEnd = 0x10, kBooted = 0x10, kTitle = 0x20;
inline constexpr int kBootFrames = 826;
// Frames a stage's machine takes to a round: longer than Super Turbo's (two logos more, the choice of version), about
// 2,700 seen, and a few
inline constexpr int kStageToRound = 3200;

// The players' records (games/ssf2t/src/ssf2t.h): the same as Super Turbo's to +2B4 (instructions placed alike name
// the same offsets), past it moved. Found with RAM dumps: kSide 0 and 1 for the two players (code: 0C26B2 reads it as
// Super Turbo's 0BE658 reads +381); kCharacter Dhalsim 7 and Ken 4 in a fight; kPerson 1 and 0 in a one-player game;
// kOpponent the other's record; kColourChoice 0 chosen with light punch, 5 with heavy kick; kChosen 0 to 1 as the
// character was chosen. kOld is the version (kVersion), 0 Super X (Super Turbo), 4 Normal: the modes put 0, as Super
// Turbo's put its new characters. No Akuma byte: 0 (the sources leave it alone).
inline constexpr uint32_t kChosen = 0x91, kSide = 0x301, kPerson = 0x328, kCharacter = 0x32B, kOpponent = 0x344;
inline constexpr uint32_t kColourChoice = 0x32C, kOld = 0x32A, kAkuma = 0;
inline constexpr uint32_t kVersion = 0x32A; // 0 Super X, 1 Super, 2 Turbo (Hyper Fighting), 3 Dash (Champion Edition), 4 Normal
// The character choice's tune (here with the choice of version: games/hsf2/src/music.cpp)
inline constexpr uint16_t kTuneChoose = 0x34;

// The boxes, as the hit tests 0C38FE and 0C3928-0C39F4 take them (Super Turbo's 0BF534, 0BF558-0BF5AE): four boxes it
// can be hit in (the frame's bytes 8 to 0B, tables 0 to 6), the attack's (byte 0C, table 8, 0x14 bytes a box), the
// push box (byte 0D, table 0A). A box is four words: its middle across and up from where the player stands, half its
// width and half its height; no long reach.
inline constexpr uint32_t kFrameHead = 0x08, kFrameBody = 0x09, kFrameLegs = 0x0A, kFrameFourth = 0x0B, kFrameAttack = 0x0C,
                          kFramePush = 0x0D;
inline constexpr uint32_t kTableHead = 0x0, kTableBody = 0x2, kTableLegs = 0x4, kTableFourth = 0x6, kTableAttack = 0x8,
                          kTablePush = 0xA;
inline constexpr uint32_t kBoxSize = 8, kAttackBoxSize = 0x14;
inline constexpr bool kBoxWords = true, kLongReach = false;

} // namespace ssf2t::family
