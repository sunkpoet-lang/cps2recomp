# Street Fighter II: Hyper Fighting Recomp

> Part of **[CPS-2 Recomp](../../README.md)**: one download and one launcher for every Capcom arcade game this
> project has ported. This page is about Street Fighter II: Hyper Fighting; the [main page](../../README.md) says how
> to download and play.

An unofficial native PC port of the arcade game *Street Fighter II′: Hyper Fighting* (Capcom, 1992, CPS-1 board;
*Street Fighter II′ Turbo* in Japan) for macOS, Linux (Steam Deck included) and Windows, made by static
recompilation.

> **A first port, and silent.** Like [Champion Edition's](../sf2ce/README.md), whose graphics and sound it shares:
> the game as the board plays it, translated, on the toolkit's classic CPS-1 board, with the picture and the six
> buttons but no sound yet, and none of the other fighting games' features.

**No game files are included.** You need your own copy of the game: the arcade ROM set `sf2hf` (World, 921209), from
a board you own. This project cannot help you get one.

This project is not affiliated with, endorsed by, or sponsored by Capcom. *Street Fighter* is a trademark of Capcom
Co., Ltd.

## What you need

- The ROM set `sf2hf` (*Street Fighter II′: Hyper Fighting*, World 921209): `sf2hf.zip` or a folder with its files
  (a merged zip works too: files are found by checksum). Other regions and revisions are refused.
- One of: macOS 13 or later; Linux x86_64 or SteamOS; Windows 10 or 11, 64-bit.

## Building from source

With your own ROM set in `rom/sf2hf/` (ignored by Git), from the repository's top:

```sh
make GAME=sf2hf            # twice the first time
make GAME=sf2hf explore    # once: finds the game's code by made-up play and translates it
make GAME=sf2hf run
```

A build of `sf2ce` first lets `tools/portmap.py` move its entry points to this program before exploring
(`--from work/sf2ce --to work/sf2hf --limit 180000 --entries ...`, added to `work/sf2hf/entries.txt`).

## Progress

**Where it stands.** On Champion Edition's board (`cps1-classic`, the S9263B mapper, the CPS-B-21's multiplier and the
kicks at 800176), with Champion Edition's entry points moved over, made-up play found 1,599 entries: 54,184
instructions translated, 27 left to the interpreter, 17 of them data a guess took for code. A run of 6,000 frames
(the title, Ryu against Ryu, then rounds against M. Bison on Ryu's stage) ran nothing in the interpreter, in silence.

**How much of Champion Edition it shares.** `tools/portmap.py` placed 16,514 of Champion Edition's 18,036 walked
instructions (91.6%) on 970 anchors and moved 1,110 of its entry points: nearly the same program, so the features
written for one would serve the other (`sources = sf2ce` with a `family.h`).
