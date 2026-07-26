#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZIG="${ZIG:-/home/opc/toolchain/zig-aarch64-linux-0.16.0/zig}"
mkdir -p "$ROOT/build"
COMMON=(-Os -Wall -Wextra -Werror)
"$ZIG" cc -target x86_64-windows-gnu "${COMMON[@]}" -Wl,--subsystem,windows -o "$ROOT/build/deepscan.exe" "$ROOT/src/platform.c" "$ROOT/src/game.c" -lgdi32 -luser32 -lwinmm
if [ -z "${ONLY_WIN:-}" ]; then
  "$ZIG" cc -O1 -Wall -Wextra -Werror -o "$ROOT/build/deepscan_headless" "$ROOT/src/platform.c" "$ROOT/src/game.c"
fi
bytes="$(stat -c%s "$ROOT/build/deepscan.exe")"
(( bytes <= 1474560 )) || { echo "over floppy limit: $bytes" >&2; exit 1; }
echo "deepscan.exe $bytes bytes"
