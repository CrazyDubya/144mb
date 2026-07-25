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

gcc -O0 -g -fsanitize=address,undefined -DCONVOY_INSTRUMENT -o "$OUT" \
    "$ROOT"/src/platform_headless.c "$ROOT"/src/bot.c "$ROOT"/src/bot_ref.c \
    "$ROOT"/src/game.c \
    "$ROOT"/src/render.c "$ROOT"/src/world.c "$ROOT"/src/audio.c \
    "$ROOT"/src/scene.c "$ROOT"/src/ui.c "$ROOT"/src/cutscene.c -lm

# The harness allocates its framebuffer and arena for the process lifetime and
# never frees them, which is correct and not worth reporting.
export ASAN_OPTIONS=detect_leaks=0

fail=0
# The in-process sweep reuses one arena across seeds, so it is the path most
# likely to carry stale state. Run it under the sanitizers as its own case,
# with both agents, before the per-seed spread.
for agent in v4 ref; do
    if ! "$OUT" -N 12 -A "$agent" -e 0 -o /tmp >/tmp/asan_sweep.txt 2>&1; then
        echo "sweep ($agent): failed"; cat /tmp/asan_sweep.txt; fail=1
    fi
    if grep -qE 'ERROR|runtime error' /tmp/asan_sweep.txt; then
        echo "sweep ($agent): sanitizer report"; sed -n '1,12p' /tmp/asan_sweep.txt; fail=1
    fi
done

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
