#!/bin/bash
# online-abuse-protection 4.2: a host/guest pair with a spectator (CPS, --gekko-arcade), and an attacker sending the
# spectator forged host statuses (phase 4: "stopped hosting", other names and streaks) and refusals from its own
# address. The spectator must keep watching: its frames hashed like the players', no "stopped hosting".
# usage: fuzz_spec.sh OUT EXE ROM FRAMES PORT [none|forged]
export PATH=/c/msys64/ucrt64/bin:$PATH SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=dummy
OUT=$1; EXE=$2; ROM=$3; FRAMES=$4; PORT=$5; ATTACK=${6:-forged}
HERE=$(dirname "$0")
mkdir -p "$OUT/h" "$OUT/g" "$OUT/s"
common=(--rom "$ROM" --headless --gekko-arcade --frames $FRAMES)
"$EXE" "${common[@]}" --gekko-host $PORT --auto 7 --nick Host --settings "$OUT/h/s.ini" --eeprom "$OUT/h/e.bin" \
  --gekko-checkpoint-log "$OUT/h/state.hash" > "$OUT/host.log" 2>&1 &
hp=$!
sleep 1
"$EXE" "${common[@]}" --gekko-join 127.0.0.1:$PORT --auto 9 --nick Guest --settings "$OUT/g/s.ini" --eeprom "$OUT/g/e.bin" \
  --gekko-checkpoint-log "$OUT/g/state.hash" > "$OUT/guest.log" 2>&1 &
gp=$!
sleep 4
"$EXE" --rom "$ROM" --headless --gekko-arcade --frames $((FRAMES - 600)) --gekko-watch 127.0.0.1:$PORT --nick Watcher \
  --settings "$OUT/s/s.ini" --eeprom "$OUT/s/e.bin" --gekko-checkpoint-log "$OUT/s/state.hash" > "$OUT/spec.log" 2>&1 &
sp=$!
sleep 3
# (the spectator's own UDP port, from its process id)
wpid=$(ps -p $sp | awk 'NR==2{print $4}')
sport=$(netstat -ano -p udp | tr -d '\r' | awk -v pid="$wpid" '$NF==pid {split($2,a,":"); print a[length(a)]; exit}')
echo "spectator pid $wpid, UDP port $sport"
ap=
if [ "$ATTACK" = forged ] && [ -n "$sport" ]; then
  /c/msys64/ucrt64/bin/python3 "$HERE/attacker.py" $sport $((FRAMES / 60)) 5 --forged > "$OUT/attacker.log" 2>&1 &
  ap=$!
fi
( sleep $((FRAMES / 60 * 3 + 90)); kill $hp $gp $sp 2>/dev/null ) &
wait $sp; ss=$?
wait $hp; hs=$?
wait $gp; gs=$?
[ -n "$ap" ] && kill $ap 2>/dev/null
echo "host $hs, guest $gs, spectator $ss; spectator lines $(wc -l < "$OUT/s/state.hash" 2>/dev/null)"
grep -a "online:" "$OUT/spec.log" | grep -v "frames; state\|KB" | sort | uniq -c | head -8
cat "$OUT/attacker.log" 2>/dev/null
