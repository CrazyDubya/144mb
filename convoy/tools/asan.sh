#!/usr/bin/env bash
# Builds the harness with AddressSanitizer and UBSan and plays a spread of
# seeds under it. The native gcc is used rather than zig cc because zig's
# bundled asan runtime does not link for this target.
#
# This exists because a one-element stack overflow in roll_offers() sat behind
# a plain segfault with no line number. Sanitizers named the file, the line and
# the array in one run.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT=/tmp/convoy_asan

gcc -O0 -g -fsanitize=address,undefined -o "$OUT" \
    "$ROOT"/src/platform_headless.c "$ROOT"/src/bot.c "$ROOT"/src/game.c \
    "$ROOT"/src/render.c "$ROOT"/src/world.c "$ROOT"/src/audio.c \
    "$ROOT"/src/scene.c "$ROOT"/src/ui.c -lm

# The harness allocates its framebuffer and arena for the process lifetime and
# never frees them, which is correct and not worth reporting.
export ASAN_OPTIONS=detect_leaks=0

fail=0
for s in "${@:-1 2 3 5 8 13 21 34 42 55}"; do
    if ! "$OUT" -s "$s" -B -e 0 -o /tmp 2>&1 | tee /tmp/asan_run.txt | grep -q '^BOT'; then
        echo "seed $s: no result"; fail=1
    fi
    if grep -qE 'ERROR|runtime error' /tmp/asan_run.txt; then
        echo "seed $s: sanitizer report"; sed -n '1,12p' /tmp/asan_run.txt; fail=1
    fi
done
[ "$fail" -eq 0 ] && echo "sanitizers clean"
exit "$fail"
