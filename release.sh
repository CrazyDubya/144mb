#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
limit=1474560
games=(deepscan switchyard last-light microcolony ten-paces)

for game in "${games[@]}"; do
  (cd "$root/$game" && ONLY_WIN=1 ./build.sh)
  exe="$root/$game/build/$game.exe"
  bytes="$(stat -c%s "$exe")"
  if (( bytes > limit )); then
    echo "$game exceeds the floppy: $bytes > $limit" >&2
    exit 1
  fi
  package="$root/dist/$game-1.0-rc1"
  mkdir -p "$package"
  cp "$exe" "$package/"
  cp "$root/$game/README.md" "$package/README.md"
  {
    echo "game=$game"
    echo "version=1.0-rc1"
    echo "bytes=$bytes"
    echo "floppy_limit=$limit"
    echo "headroom=$((limit - bytes))"
    sha256sum "$exe" | sed "s|$exe|$game.exe|"
  } > "$package/MANIFEST.txt"
  echo "$game: $bytes bytes -> ${package#$root/}"
done
