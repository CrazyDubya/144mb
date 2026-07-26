#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
games=(deepscan switchyard last-light microcolony ten-paces)
limit=1474560
minimum=$((limit * 80 / 100))
maximum=$((limit * 95 / 100))
declare -A victory=(
  [deepscan]="EXPEDITION COMPLETE"
  [switchyard]="CAMPAIGN COMPLETE"
  [last-light]="HARBOR SAFE"
  [microcolony]="RESEARCH COMPLETE"
  [ten-paces]="THE TOWN REMEMBERS"
)
declare -A failure=(
  [deepscan]="SUBMERSIBLE LOST"
  [switchyard]="NETWORK FAILED"
  [last-light]="SOMETHING CAME ASHORE"
  [microcolony]="CULTURE COLLAPSED"
  [ten-paces]="YOU ARE DOWN"
)

scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT

for game in "${games[@]}"; do
  echo "== $game: build =="
  (cd "$root/$game" && ./build.sh)
  exe="$root/$game/build/$game.exe"
  harness="$root/$game/build/${game}_headless"
  bytes="$(stat -c%s "$exe")"
  if (( bytes < minimum || bytes > maximum )); then
    echo "$game is outside the meaningful RC production range: $bytes" >&2
    exit 1
  fi
  file "$exe" | grep -Fq "PE32+ executable (GUI)"
  strings "$exe" | grep -Fq "1.0 RC1"
  strings "$exe" | grep -Fq "PRESS ENTER TO BEGIN"
  test -s "$root/$game/assets/title-source.png"
  test -s "$root/$game/assets/scenes-rc1.png"
  test -s "$root/$game/assets/story-rc1.png"

  echo "== $game: campaign and failure probes =="
  win="$("$harness" -s 1 -N 100)"
  [[ "$win" == *"${victory[$game]}"* ]]
  [[ "$win" == *" 100/100 completed"* ]]
  loss="$("$harness" -s 1 -L -N 20)"
  [[ "$loss" == *"${failure[$game]}"* ]]
  [[ "$loss" == *" 20/20 completed"* ]]

  echo "== $game: deterministic replay =="
  "$harness" -s 41 -N 20 > "$scratch/$game-a"
  "$harness" -s 41 -N 20 > "$scratch/$game-b"
  cmp "$scratch/$game-a" "$scratch/$game-b"
  "$harness" -s 41 -Q -t 600 -o "$scratch/$game-a.bmp" >/dev/null
  "$harness" -s 41 -Q -t 600 -o "$scratch/$game-b.bmp" >/dev/null
  cmp "$scratch/$game-a.bmp" "$scratch/$game-b.bmp"
  [[ "$(stat -c%s "$scratch/$game-a.bmp")" -eq 921654 ]]
  file "$scratch/$game-a.bmp" | grep -Fq "PC bitmap, Windows 3.x format, 640 x 480 x 24"

  echo "== $game: ASan and UBSan =="
  gcc -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -o "$scratch/$game-sanitize" \
    "$root/$game/src/platform.c" "$root/$game/src/game.c"
  ASAN_OPTIONS=detect_leaks=0 "$scratch/$game-sanitize" -s 11 -N 20 >/dev/null
  ASAN_OPTIONS=detect_leaks=0 "$scratch/$game-sanitize" -s 11 -L -N 20 >/dev/null
  ASAN_OPTIONS=detect_leaks=0 "$scratch/$game-sanitize" -s 11 -F -N 20 -t 3000 >/dev/null
done

[[ "$(sha256sum "$scratch"/*-a.bmp | cut -d' ' -f1 | sort -u | wc -l)" -eq 5 ]]

"$root/release.sh"
for manifest in "$root"/dist/*-1.0-rc1/MANIFEST.txt; do
  package="$(dirname "$manifest")"
  (cd "$package" && sha256sum -c <(tail -n 1 MANIFEST.txt))
done

git -C "$root" diff --check
echo "RC1 LOCAL AUDIT PASSED"
