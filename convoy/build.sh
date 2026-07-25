#!/usr/bin/env bash
# convoy -- builds the Windows submission target and, unless ONLY_WIN is set,
# the native headless harness. The contest limit is a hard fail, not a warning.
#
#   ZIG=/path/to/zig   override the compiler (CI sets ZIG=zig)
#   ONLY_WIN=1         skip the headless harness (CI does not need it)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIMIT=1474560   # 1.44MB floppy, in bytes

# Prefer an explicit ZIG, then one on PATH, then the local toolchain.
if [ -n "${ZIG:-}" ]; then
    :
elif command -v zig >/dev/null 2>&1; then
    ZIG=zig
else
    ZIG=/home/opc/toolchain/zig-aarch64-linux-0.16.0/zig
fi

mkdir -p "$ROOT/build"

SRC=("$ROOT/src/game.c" "$ROOT/src/render.c" "$ROOT/src/world.c" "$ROOT/src/audio.c" "$ROOT/src/scene.c" "$ROOT/src/ui.c" "$ROOT/src/cutscene.c")
SIZE_FLAGS="-Os -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables"
# -Werror is deliberate. Warnings were enabled from the start and accumulated
# anyway: an unused duplicate of a pricing formula, two dead price tables that
# two separate audits mistook for live data, and a misleading indentation in a
# switch case. None of them broke the game, and all of them cost review time.
WARN="-Wall -Wextra -Wno-unused-parameter -Werror"

echo "== windows x86_64 (submission target) =="
# shellcheck disable=SC2086
"$ZIG" cc -target x86_64-windows-gnu $SIZE_FLAGS $WARN \
    -Wl,--subsystem,windows \
    -o "$ROOT/build/convoy.exe" \
    "$ROOT/src/platform_win32.c" "${SRC[@]}" \
    -lgdi32 -luser32 -lwinmm

if [ -z "${ONLY_WIN:-}" ]; then
    echo "== native headless harness =="
    mkdir -p "$ROOT/out"
    # shellcheck disable=SC2086
    # -DCONVOY_INSTRUMENT compiles the measurement counters into World. They
    # are deliberately absent from the Windows target: the contest binary
    # should not carry its own test rig, and keeping them out means the shipped
    # exe is byte-identical whether or not the harness is instrumented.
    "$ZIG" cc -O1 -g $WARN -DCONVOY_INSTRUMENT \
        -o "$ROOT/build/convoy_headless" \
        "$ROOT/src/platform_headless.c" "$ROOT/src/bot.c" "$ROOT/src/bot_ref.c" "${SRC[@]}"
fi

BYTES=$(stat -c%s "$ROOT/build/convoy.exe" 2>/dev/null || stat -f%z "$ROOT/build/convoy.exe")
REMAIN=$((LIMIT - BYTES))
PCT=$(awk -v b="$BYTES" -v l="$LIMIT" 'BEGIN{printf "%.2f", b*100/l}')

echo
echo "-------------------------------------------------"
printf 'convoy.exe   %10d bytes  (%s%% of floppy)\n' "$BYTES" "$PCT"
printf 'remaining    %10d bytes\n' "$REMAIN"
echo "-------------------------------------------------"

if [ "$BYTES" -gt "$LIMIT" ]; then
    echo "FAIL: over the 1,474,560 byte limit by $((BYTES - LIMIT)) bytes" >&2
    exit 1
fi
