# convoy

A post-apocalyptic trading roguelike that fits on a floppy disk.

**72,192 bytes — 4.90% of 1,474,560.**

You run a convoy east across eight sectors toward the Green Zone. You will not
get there without trading, and everything you trade is something that keeps you
alive.

## The design

**Every good has a market price and a survival use.**

| good | you sell it for | you need it to |
|---|---|---|
| water | high, stable | keep the crew alive each day |
| fuel | high, volatile | move at all |
| ammo | mid | survive raiders |
| meds | mid, spiky | treat sickness |
| scrap | low, bulk | repair breakdowns |

So no transaction is only about money. Selling ammo at a good price means you
cannot fight off the next raid. That is the whole game.

**Cargo is health.** There is no hit-point bar anywhere. Raiders take cargo,
storms take water and fuel, and an empty hold is death. You die broke.

**Markets remember.** Selling into a settlement permanently depresses its prices.
Routes burn out behind you, so the pressure to push into the dangerous outer
sectors is economic rather than an artificial timer.

You start with six fuel and the Green Zone is seven hops away, which means
reaching it is arithmetically impossible without trading. Measured over 60 seeds:
a bot that buys fuel wins **38%** of runs; one that ignores the economy wins
**0%**.

## Controls

Nothing on screen is written in any language — meaning is carried by icon,
colour, sign and numerals. There is no alphabetic font in the build.

| key | does |
|---|---|
| ↑ ↓ | select |
| Z | buy · accept · travel |
| X | sell · refuse |
| ↵ | depart · restart |

Keycaps appear inline next to whatever they act on, so the game needs no
instructions.

## Building

```sh
./build.sh          # windows exe + native headless harness
ONLY_WIN=1 ./build.sh   # windows exe only
```

Requires [Zig](https://ziglang.org) 0.16.0 as the C cross-compiler. Nothing else.
The build fails if the executable exceeds the floppy limit.

## Development harness

The dev machine has no display and cannot execute the Windows binary, so
`platform_headless.c` runs the game core natively with scripted input and writes
what it renders to disk.

```sh
# play a scripted run, dump a frame per step, trace the simulation
./build/convoy_headless -s 16 -i "daaaaasabsaaaaasabsaa" -e 1 -o run -v

# render the synthesiser to a wav and report levels
./build/convoy_headless -s 8 -w 10 -o out -i "."
```

Script characters are one discrete keypress each: `u d l r` arrows, `a` = Z,
`b` = X, `s` = ↵, `.` idles.

| flag | meaning |
|---|---|
| `-s N` | seed |
| `-i STR` | input script |
| `-e N` | dump a frame every N steps |
| `-o DIR` | output directory |
| `-v` | trace simulation state each step |
| `-w N` | render N seconds of audio to a wav |

`tools/mkmedia.py` recompresses frame dumps and assembles an animated GIF —
GIF/LZW is implemented there because this machine has no ffmpeg, ImageMagick or
PIL.

## Layout

| file | |
|---|---|
| `src/game.h` | the boundary: memory, input, pixels, audio. No OS calls cross it. |
| `src/game.c` | state machine and presentation |
| `src/world.c` | route generation, markets, travel, survival — pure simulation |
| `src/render.c` | rasterizer, bitmap numerals, icon vocabulary |
| `src/audio.c` | synthesiser and sequencer, integer maths only |
| `src/platform_win32.c` | submission target |
| `src/platform_headless.c` | development harness |

## Status

Mechanically complete and verified by trace: markets, travel, five encounter
types, three death conditions, win condition. Zero crashes across 200 seeds.

Not done: balance tuning, win and lose screens are placeholder, and **the binary
has never been run on Windows** — that is what CI is for.
