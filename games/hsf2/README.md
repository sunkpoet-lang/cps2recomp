# Hyper Street Fighter II Recomp

> Part of **[CPS-2 Recomp](../../README.md)**: one download and one launcher for every CPS-2 game this
> project has ported. This page is about Hyper Street Fighter II; the [main page](../../README.md) says how to
> download and play.

An unofficial native PC port of the arcade game *Hyper Street Fighter II: The Anniversary Edition* (Capcom, 2004,
CPS-2 board) for macOS, Linux (Steam Deck included) and Windows, made by static recompilation.

> **Super Turbo's modes (this fork).** The game as the board plays it, translated, with most of what Super Turbo has
> here, from Super Turbo's own sources: training mode and its dummy, hit boxes and frame data, attract mode, the stage
> viewer, survival, endless battle and ring out, free play, straight to the title, the Board and the Cheats pages,
> and its music for a custom soundtrack with F12. Not yet: widescreen, the tag team battle's tag style, the model
> viewer and custom colours.

**No game files are included.** You need your own copy of the game: the arcade ROM set `hsf2j` (Japan, 040202) with
its key file, and its parent set `hsf2`, which holds the graphics and the sound, from a board you own. This project
cannot help you get one.

This project is not affiliated with, endorsed by, or sponsored by Capcom. *Street Fighter* is a trademark of Capcom
Co., Ltd.

## What you need

- The ROM set `hsf2j` (*Hyper Street Fighter II: The Anniversary Edition*, Japan 040202): `hsf2j.zip` with
  `hsf2j.key`, and beside it the parent set `hsf2.zip` (or one folder with all the files). The launcher checks every
  file by checksum; other regions and revisions are refused.
- One of: macOS 13 or later; Linux x86_64 or SteamOS; Windows 10 or 11, 64-bit.

## Building from source

With your own ROM sets in `rom/hsf2/` (ignored by Git), CMake, Ninja, SDL 3 and Python 3, from the repository's top:

```sh
make GAME=hsf2            # twice the first time
make GAME=hsf2 explore    # once: finds the game's code by made-up play and translates it
make GAME=hsf2 run
```

The game has no sources of its own yet, so made-up play only inserts coins and presses buttons at random.
Hyper Street Fighter II was made from Super Street Fighter II Turbo's program: a build of [ssf2t](../ssf2t/README.md)
first lets `tools/portmap.py` move that game's known entry points to this one's before exploring
(`--from work/ssf2t --to work/hsf2 --limit 400000 --entries ...`, added to `work/hsf2/entries.txt`).

## Progress

**Where it stands (first port).** Made-up play from Super Turbo's moved entry points found 553 entries: 120,595
instructions translated (more than Super Turbo's 108,415), 88 left to the interpreter, 65 of them data a guess took for
code. All the code reached so far is in the program's first megabyte (000000 to 0E8AF8); `code_end` stays at 400000
until play reaches past it. A run of 6,000 frames of made-up play (title, attract mode, the versus screen, a fight)
ran nothing in the interpreter, at about 600 frames a second without a window. The Z80's sound program is translated
whole.

**Its music, for a custom soundtrack** (`src/music.cpp`). The sound program's table of songs is Super Turbo's
layout with the same 68 pieces of music under the same numbers, so they carry Super Turbo's names and its soundtracks
play here; F12 switches between the player's files and the board's music, as in every game with named music. Seen in
made-up play (three runs of 20,000 frames): the stages of Ryu, Guile, Dhalsim, Cammy and T. Hawk (a stage's tune is
the stage's, whoever fights there), the later versions of Guile's, Dhalsim's and T. Hawk's, versus and the winner,
all as in Super Turbo; player select is 0x34 here (the screen with the choice of version), not Super Turbo's 0xD3.
Not seen yet, named as Super Turbo has them: the title (attract mode plays no sound unless the board's demo sound is
on), the other stages, the final fight, continue, game over, the ranking, the ending and the staff roll.

**How much of Super Turbo it shares.** `tools/portmap.py --from work/ssf2t --to work/hsf2 --limit 400000` placed
11,988 of Super Turbo's 29,411 walked instructions (40.8%) on 602 anchors, named 2,325 places, and moved 244 of its
825 entry points. Much of the program is Super Turbo's, but moved and changed, as the version select and the added
characters' versions would make it.

**Super Turbo's sources, built for this game.** game.cfg's `sources = ssf2t` builds Super Turbo's `src/` here too,
each place they name written `At(0x...)` and found in this game by `family.h` (as Vampire Savior 2 builds Vampire
Savior's): 43 by `tools/places.py` from the portmap, 17 by hand (`// by hand`, each with how). Besides places, what
is this game's own is in `family.h` as well:

- **The game's steps.** A level fewer than Super Turbo's: FF8000 is 2 while the players choose (the character and
  the version) and 4 for the match; FF8004 is the match's step (8 the round); FF8008 the round's (2 while it is
  fought). The show: 6 the introduction and the title, 0C a demonstration fight, 0E the rankings.
- **The players' records.** Super Turbo's to +2B4; past it moved (the side +301, the character +32B, the person +328,
  the colours +32C, the version +32A, the other player +344).
- **The boxes.** Four it can be hit in (Super Turbo's three), each four words (Super Turbo's four bytes), the
  attack's 0x14 bytes, the push box's table at 0A.
- **The computer's first step** calls the character's tables, its plan and the round's plan at 06BD16, 06C922 and
  06A1C8, as Super Turbo's does at 07D6F4, 07E36E and 07BB82.

Left out of Super Turbo's sources (game.cfg's `sources_leave_out`): the tag team battle (its tag style; the knock-out
style is the game's own rounds), the model viewer, widescreen, custom colours and Super Turbo's hidden characters
(this game has all the versions). What those gave the others is `src/left_out.cpp`; this game's own `game.cpp` and
`music.cpp` take the place of Super Turbo's.

Checked without a window: with nothing on, the same frames as before, hash for hash; a training session with hit
boxes, frame data, the input display and the readouts; the dummy crouching, jumping, guarding, recording and
replaying; survival, endless battle and ring out; attract mode's parts and the stage viewer; free play; the Board's
choices in the board's memory; no damage, infinite time and one-hit knockouts; run-ahead the same, hash for hash.

**Next.** The modes' fighters all play the Super X version (Super Turbo's): a choice of version for each; widescreen
(Super Turbo's rewrites its camera routines, which this game has elsewhere), the tag style, the model viewer.
