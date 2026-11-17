# SPDX-License-Identifier: GPL-3.0-only
"""(this fork, replay-compatibility) A CPS game's gameplay identity for a source tree, as CMake makes it
(CMakeLists.txt, replay_identity.txt): the listed sources, the save state's version and the game's id, hashed.

  python tools/replays/identity.py --source . --game ssf2t [--game-dir games/ssf2t] [--work-dir work/ssf2t] [--text]

--list FILE uses another identity list (for an earlier tree that had none: this tree's)."""
from pathlib import Path
import argparse, fnmatch, hashlib, re, sys


def identity(source, game, game_dir=None, work_dir=None, listing=None):
    source = Path(source)
    game_dir = Path(game_dir) if game_dir else source / 'games' / game
    work_dir = Path(work_dir) if work_dir else source / 'work' / game
    main = (source / 'port/main.cpp').read_text(encoding='utf-8', errors='replace')
    state = re.search(r'kStateVersion = ([0-9]+)', main)
    if not state:
        raise SystemExit('kStateVersion not found in port/main.cpp')
    text = f'cps-replay-identity-1\nstate {state.group(1)}\ngame {game}\n'
    lines = (Path(listing) if listing else source / 'replay_identity.txt').read_text(encoding='utf-8').splitlines()
    for pattern in [l for l in lines if l and not l.startswith('#')]:
        if pattern.startswith('@GAME@/'): root, label, rel = game_dir, 'game', pattern[7:]
        elif pattern.startswith('@WORK@/'): root, label, rel = work_dir, 'work', pattern[7:]
        else: root, label, rel = source, 'src', pattern
        # (CMake's GLOB_RECURSE: the pattern's last part matched in its directory and every one below it)
        base, last = (rel.rsplit('/', 1) + [''])[:2] if '/' in rel else ('', rel)
        top = root / base if base else root
        files = []
        if top.is_dir():
            for f in top.rglob('*'):
                if f.is_file() and fnmatch.fnmatchcase(f.name, last):
                    files.append(f.relative_to(root).as_posix())
        for f in sorted(files):
            text += f'{label}/{f} {hashlib.sha256((root / f).read_bytes()).hexdigest()}\n'
    return hashlib.sha256(text.encode()).hexdigest(), text


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--source', default='.')
    p.add_argument('--game', required=True)
    p.add_argument('--game-dir')
    p.add_argument('--work-dir')
    p.add_argument('--list')
    p.add_argument('--text', action='store_true')
    a = p.parse_args()
    value, text = identity(a.source, a.game, a.game_dir, a.work_dir, a.list)
    print(text if a.text else value, end='' if a.text else '\n')
