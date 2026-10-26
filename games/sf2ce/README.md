# Street Fighter II: Champion Edition Recomp

> Part of **[CPS-2 Recomp](../../README.md)**: one download and one launcher for every Capcom arcade game this
> project has ported. This page is about Street Fighter II: Champion Edition; the [main page](../../README.md) says how
> to download and play.

An unofficial native PC port of the arcade game *Street Fighter II′: Champion Edition* (Capcom, 1992, CPS-1 board)
for macOS, Linux (Steam Deck included) and Windows, made by static recompilation.

> **A first port, and silent.** This is the game as the board plays it, translated, on the toolkit's classic CPS-1
> board: the picture and the six buttons, but no sound yet (the board's YM2151 and OKI MSM6295 are still to be
> written), and none of the features the other fighting games here have.

**No game files are included.** You need your own copy of the game: the arcade ROM set `sf2ce` (World, 920513), from
a board you own. This project cannot help you get one.

This project is not affiliated with, endorsed by, or sponsored by Capcom. *Street Fighter* is a trademark of Capcom
Co., Ltd.

## What you need

- The ROM set `sf2ce` (*Street Fighter II′: Champion Edition*, World 920513): `sf2ce.zip` or a folder with its files.
  A merged zip with its clones in it works too: the launcher finds each file by checksum. Other regions and revisions
  are refused.
- One of: macOS 13 or later; Linux x86_64 or SteamOS; Windows 10 or 11, 64-bit.

## Building from source

With your own ROM set in `rom/sf2ce/` (ignored by Git), CMake, Ninja, SDL 3 and Python 3, from the repository's top:

```sh
make GAME=sf2ce            # twice the first time
make GAME=sf2ce explore    # once: finds the game's code by made-up play and translates it
make GAME=sf2ce run
```

## Progress

**Where it stands.** The board is `cps1-classic` (machine/romset.h): a CPS-1 with no QSound, its Z80 stopped until
the toolkit has its sound chips. Three things the classic board needed and Cadillacs and Dinosaurs had not:

- **The graphics mapper.** The PAL S9263B keeps the three scroll screens' tiles in its third bank (`gfx_map` in
  game.cfg, `GfxRange`); without it the backgrounds and the text were other tiles.
- **The CPS-B-21's multiplier** (800140 and 800142 multiplied, the product at 800144 and 800146): the game looks up
  the jump at the start of a fight with it, and jumped to nowhere and restarted without it.
- **The kicks** at 800176: player 1's in bits 0 to 2, player 2's in bits 4 to 6, light to hard. Each kick checked
  from the start of a round: a light, a medium and Ryu's roundhouse.

Made-up play found 1,288 entries: 51,653 instructions translated, 25 left to the interpreter, 18 of them data a guess
took for code, all the code reached below 0C6232. A run of 6,000 frames (the title, player select, the versus
screen, a whole round to its time over) ran 0.12% of its instructions in the interpreter, in silence.

**Next.** The sound: the YM2151 and the OKI MSM6295, then the requests the 68000 leaves at 800180. Then the DIP
switches by name (free play, difficulty), and the features the other fighting games have.
