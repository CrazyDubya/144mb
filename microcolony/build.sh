#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)";ZIG="${ZIG:-/home/opc/toolchain/zig-aarch64-linux-0.16.0/zig}";mkdir -p "$ROOT/build"
"$ZIG" cc -target x86_64-windows-gnu -Os -Wall -Wextra -Werror -Wl,--subsystem,windows -o "$ROOT/build/microcolony.exe" "$ROOT/src/platform.c" "$ROOT/src/game.c" -lgdi32 -luser32 -lwinmm
if [ -z "${ONLY_WIN:-}" ]; then
  "$ZIG" cc -O1 -Wall -Wextra -Werror -o "$ROOT/build/microcolony_headless" "$ROOT/src/platform.c" "$ROOT/src/game.c"
fi
bytes="$(stat -c%s "$ROOT/build/microcolony.exe")";((bytes<=1474560))||exit 1;echo "microcolony.exe $bytes bytes"
