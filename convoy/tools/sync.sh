#!/usr/bin/env bash
# Copies the working tree into the ship tree.
#
# /home/opc/convoy is where the game is developed and measured; the repo at
# /home/opc/144mb/convoy is what actually ships and what CI builds. They are
# two separate copies with no link between them, so for three releases the
# only thing keeping them in step was remembering to cp by hand. Nothing would
# have reported a drift: the tested binary and the shipped binary are built
# from different files, and every sweep runs against the former.
#
# Run this before every commit. It reports what it changed rather than copying
# silently, because a sync that quietly overwrites is how work gets lost.
set -euo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DST="${1:-/home/opc/144mb/convoy}"

[ -d "$DST" ] || { echo "no ship tree at $DST" >&2; exit 1; }

changed=0
for rel in src tools build.sh; do
    if ! diff -rq "$SRC/$rel" "$DST/$rel" >/dev/null 2>&1; then
        # diff exits 1 when files differ, which is the expected case here --
        # without the guard, pipefail treats the report as a failure.
        diff -rq "$SRC/$rel" "$DST/$rel" 2>&1 | sed 's/^/  /' || true
        changed=1
    fi
done

# --delete would drop files the ship tree has and the working tree does not,
# which is exactly what a rename leaves behind. Explicitly mirror instead.
rsync -a --delete \
      --exclude '__pycache__' --exclude '*.pyc' \
      "$SRC/src/" "$DST/src/"
rsync -a --delete \
      --exclude '__pycache__' --exclude '*.pyc' \
      "$SRC/tools/" "$DST/tools/"
cp "$SRC/build.sh" "$DST/build.sh"

if [ "$changed" -eq 0 ]; then
    echo "sync: already in step"
else
    echo "sync: ship tree updated from $SRC"
fi

# The ship tree is what CI compiles, so prove it builds here rather than
# finding out from a red CI run ten minutes later.
( cd "$DST" && ./build.sh >/dev/null ) && echo "sync: ship tree builds"
