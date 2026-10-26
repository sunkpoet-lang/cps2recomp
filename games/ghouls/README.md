# Ghouls'n Ghosts Recomp

> Part of **[CPS-2 Recomp](../../README.md)**: one download and one launcher for every Capcom arcade game this
> project has ported. This page is about Ghouls'n Ghosts; the [main page](../../README.md) says how to download and
> play.

An unofficial native PC port of the arcade game *Ghouls'n Ghosts* (Capcom, 1988, CPS-1 board; *Daimakaimura* in
Japan) for macOS, Linux (Steam Deck included) and Windows, made by static recompilation.

> **A first port, with its sound (this fork).** The game as the board plays it, translated, on the toolkit's classic
> CPS-1 board: the picture, the two buttons (fire and jump) and the board's FM music and effects, but no features of
> its own yet. The sound plays the board's YM2151 with [ymfm](https://github.com/aaronsgiles/ymfm) (BSD-3-Clause,
> `third_party/`): an emulator's code, which upstream does not take (CONTRIBUTING.md, rule 4), so it lives only in this
> fork.

**No game files are included.** You need your own copy of the game: the arcade ROM set `ghouls` (World), from a board
you own. This project cannot help you get one.

This project is not affiliated with, endorsed by, or sponsored by Capcom. *Ghouls'n Ghosts* is a trademark of Capcom
Co., Ltd.

## What you need

- The ROM set `ghouls` (*Ghouls'n Ghosts*, World): `ghouls.zip` or a folder with its files (a merged zip, with
  `ghoulsu`, `daimakai` and `daimakair` in it, works too: files are found by checksum). Other regions and revisions
  are refused.
- One of: macOS 13 or later; Linux x86_64 or SteamOS; Windows 10 or 11, 64-bit.

## Building from source

With your own ROM set in `rom/ghouls/` (ignored by Git), from the repository's top:

```sh
make GAME=ghouls            # twice the first time
make GAME=ghouls explore    # once: finds the game's code by made-up play and translates it
make GAME=ghouls run
```

## Progress

**Where it stands.** On the classic CPS-1 board of [Street Fighter II](../sf2ce/README.md), with four things a board
of 1988 needed and the toolkit had not:

- **The 68000 at 10 MHz** (`cpu_clock` in game.cfg): 640 cycles a line instead of 768, as MAME's `cps1_10MHz`, so the
  game slows down where the board does.
- **Its program in pairs of files**, one with the even bytes and one with the odd (`a+b`), and a file of words high
  byte first (`/be`).
- **Its graphics in groups of eight files** of a byte each (eight joined with `+`), after four files of two bytes.
- **The graphics mapper on the objects too.** The PAL DM620 keeps objects 2000-3FFF in its third bank and scroll 3's
  tiles from 8000 in its second (`gfx_map`); the objects had not gone through the mapper, which Street Fighter II's
  S9263B did not need.

The CPS-B-01 has the same registers as Street Fighter II's CPS-B-21, with no multiplier. The board has no OKI ROMs,
so there are no voices: the YM2151 plays the music and the effects. The DIP switches are as the boards left the
factory (`dip` in game.cfg): 3 lives, difficulty 2, a bonus life at 30K, 60K and every 70K, continues allowed.

Made-up play found 568 entries: 41,824 instructions translated, 51 left to the interpreter, 29 of them data a guess
took for code. A run of 6,000 frames (the board's tests, the story, the first stage's graveyard with Arthur losing
his armour) ran nothing in the interpreter, at 551 frames a second without a window; its sound starts with the game's
first request as a game begins (the attract mode asks for none) and is the same, byte for byte, with run-ahead.

**The Board page** (the in-game menu's, F1 or Esc, and the launcher's): the DIP switches by name, from game.cfg's
`board_option_N` lines: difficulty (1 to 8), lives (3 to 6), the bonus life, continue and demo sound, each "As the
board is set" unless chosen. The board reads them only as it starts, so a change takes effect after Reset (the menu's
Reset, or the next time the game opens).

**Next.** Features of its own.
