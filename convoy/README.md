# convoy

A post-apocalyptic trading roguelike that fits on a floppy disk.

**94,208 bytes — 6.39% of 1,474,560.**

You run a convoy east across fourteen sectors toward the Green Zone. You will not
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

You start with five fuel and the Green Zone is nine hops away, so reaching it is
arithmetically impossible without trading. Fuel also gets dearer the further east
you go, which means the run grows harder to afford exactly as it grows harder to
survive.

### Measured difficulty

| player | wins |
|---|---|
| ignores the economy | **0%** |
| buys fuel, fixed routine | **0%** |
| plays prices (see the bot below) | **53%** |

Over 200 seeds each. The first two follow fixed key sequences and cannot react
to a price, so they die out on the road; only the third actually trades.

## Controls

| key | does |
|---|---|
| ↑ ↓ | select |
| Z | buy · accept · travel |
| X | sell · refuse |
| ↵ | depart · restart |
| H | how to play |
| Esc | quit |

Keycaps appear inline beside whatever they act on, and every one is labelled.

An earlier version shipped with no text at all, on the theory that pure
iconography reads the same in every language. It did not survive contact with
the screenshots: nothing told you the droplet was *water* rather than coolant,
that the number beside it was a *price* rather than a count, or that
`−meds×1 / −water×2` was a choice between paying and suffering. That is not
minimalism, it is a puzzle wrapped around the game. Icons still carry the
meaning at a glance; the words are there so the glance is not a guess.

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

```sh
# let the price-aware bot play a full run
./build/convoy_headless -s 16 -B
```

`tools/mkmedia.py` recompresses frame dumps and assembles an animated GIF —
GIF/LZW is implemented there because this machine has no ffmpeg, ImageMagick or
PIL. `tools/crop.py` decodes and crops arbitrary PNGs, filters and all.

## The test bot

`src/bot.c` plays the game through its own UI: it moves the cursor and presses
the same keys a player would, deciding from world state. It tracks a running
average of every price it has seen, sells above it, buys below it, keeps a fuel
and water reserve sized to the distance remaining, and scores each route branch.

It is compiled **only into the headless harness** and contributes zero bytes to
the submitted executable.

It exists because scripted key sequences measure only the floor — a fixed string
of presses cannot look at a price and decide. The first thing the bot revealed
was that the game was far too easy for anyone who knew what they were doing: it
won 90% of runs on the original eight-sector route. Lengthening the journey and
thinning out the settlements brought that to 53%.

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

Version 1. Title screen, procedural backdrop, five encounter types, three death
conditions, win condition, procedural audio, balanced against a price-aware bot.
Zero crashes across 300 bot-played runs. Verified running on Windows by CI, which launches the binary and
screenshots it on every push.

The backdrop is generated every frame and stored nowhere: a Bayer-dithered sky,
three parallax dune layers summed from sines, a dithered sun corona, and dust
motes derived from the tick counter so they carry no state.
