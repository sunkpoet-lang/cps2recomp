#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# (multiplayer-online) The backend-neutral seat match on a fake board, as separate processes on this PC: a room of
# three started with two, a third PC joining, the first guest leaving, a fourth joining the host left alone; every
# epoch's checkpoints equal on every PC that played it.
#   tools/tests/seat_match_check.sh OUT
set -u
OUT=${1:?usage: seat_match_check.sh OUT}
export PATH=/c/msys64/ucrt64/bin:$PATH
HERE=$(cd "$(dirname "$0")/../.." && pwd)
GEKKO=${GEKKO:-$HERE/build/ssf2t/_deps/gekkonet-src}
GEKKOLIB=${GEKKOLIB:-$(ls "$HERE"/build/ssf2t/_deps/gekkonet-build/*.a 2>/dev/null | head -1)}
rm -rf "$OUT"; mkdir -p "$OUT"
g++ -std=c++20 -O1 -Wall -Wextra -Werror -DGEKKONET_STATIC -DGEKKONET_NO_ASIO -I"$HERE/port" -I"$GEKKO/GekkoLib/include" "$HERE/tests/seat_match_peer.cpp" \
  "$GEKKOLIB" -o "$OUT/peer.exe" -lws2_32 -lbcrypt -lz || exit 1
PORT=$((63100 + RANDOM % 300))
"$OUT/peer.exe" host $PORT 1500 1 "$OUT/host.log" late start 2 > "$OUT/host.txt" 2>&1 & h=$!
sleep 1
"$OUT/peer.exe" join $PORT 600 2 "$OUT/guest.log" start 2 > "$OUT/guest.txt" 2>&1 & g=$!
sleep 6
"$OUT/peer.exe" join $PORT 300 3 "$OUT/joiner.log" start 2 > "$OUT/joiner.txt" 2>&1 & j=$!
sleep 26
"$OUT/peer.exe" join $PORT 1200 4 "$OUT/late.log" start 2 > "$OUT/late.txt" 2>&1 & l=$!
wait $h; hs=$?; wait $g $j $l
for n in host guest joiner late; do echo "== $n"; grep -E "transition|late join|alone|left|seat_match_peer|desync" "$OUT/$n.txt" | tail -8; done
python - "$OUT" <<'EOF'
import sys, os
o = sys.argv[1]
def read(n):
    r = {}; epoch = 1
    for l in open(os.path.join(o, n + '.log')):
        if l.startswith('# epoch'): epoch = int(l.split()[2]); continue
        v = l.split()
        if len(v) >= 2: r[(epoch, int(v[0]))] = v[1]
    return r
c = {n: read(n) for n in ('host', 'guest', 'joiner', 'late')}
bad = 0
for epoch in sorted({e for d in c.values() for e, _ in d}):
    who = [n for n in c if any(e == epoch for e, _ in c[n])]
    keys = set.intersection(*[{k for k in c[n] if k[0] == epoch} for n in who])
    top = max((f for _, f in keys), default=0) - 16
    keys = [k for k in keys if k[1] <= top]
    differ = sum(1 for k in keys if len({c[n][k] for n in who}) > 1); bad += differ
    print(f'epoch {epoch}: {who}, {len(keys)} common, differ {differ}')
print('seat_match_check:', 'PASSED' if bad == 0 else 'FAILED')
EOF
