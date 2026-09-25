#!/bin/sh
#
# Boots the getting-started executable under pcsx-redux and waits for the
# engine to hand control to the game code.
#
#   make -C getting-started
#   tools/boot-demo.sh /path/to/pcsx-redux
#
# There is no disc, so the archive fails to mount; the engine carries on to
# MadnightGame::InitialLoad regardless, and that is what this waits for.

set -e

EMU=${1:?usage: $0 <pcsx-redux binary>}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EXE=$ROOT/getting-started/build/madnight-engine-game.ps-exe
TOKEN="welcome to your game code!"

if [ ! -f "$EXE" ]; then
    echo "$EXE is missing; build it with make -C getting-started" >&2
    exit 1
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# The game loop never exits on its own, so the emulator is killed once the
# token shows up, and the timeout caps a run that never gets there. stdbuf
# because the log is polled while the emulator is still running.
timeout 120 stdbuf -oL -eL "$EMU" -no-ui -testmode -run -stdout \
        -loadexe "$EXE" > "$WORK/log" 2>&1 &
PID=$!

found=0
i=0
while [ $i -lt 110 ]; do
    if grep -q "$TOKEN" "$WORK/log"; then
        found=1
        break
    fi
    kill -0 $PID 2> /dev/null || break
    sleep 1
    i=$((i + 1))
done

kill $PID 2> /dev/null || true
wait $PID 2> /dev/null || true
cat "$WORK/log"

if [ $found -ne 1 ]; then
    echo "game code never started" >&2
    exit 1
fi

echo "game code started after ${i}s"
