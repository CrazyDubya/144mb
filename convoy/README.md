# convoy

A post-apocalyptic trading roguelike that fits on a floppy disk.

**110,592 bytes — 7.50% of 1,474,560.**

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

**Markets remember, and they run out.** Selling into a settlement permanently
depresses its prices, so routes burn out behind you and the pressure to push
into the dangerous outer sectors is economic rather than an artificial timer.
Stock is finite and set by what a place produces — a well has water to spare
and almost no ammunition — so a cargo is assembled across several towns rather
than bought at one.

**A town is a place, not a price row.** It has a name, and about half of them
are going through something: under siege, sick, booming, half-abandoned, run by
a cartel, or gone dry. That bends what they have and what they charge, and each
one offers a choice you can walk into or walk past. You cannot see any of it
from the map — the kind of place is drawn, its condition and prices are found by
arriving.

**Every archetype does one thing no other will.** A well fills your tanks
whatever the shelf says; a scrapyard puts a broken fitting right, or pays list
for metal; a clinic squares you with whoever is closest to walking out.

You start with five or six fuel and the Green Zone is thirteen hops away, so
reaching it is arithmetically impossible without trading. Fuel also gets dearer the further east
you go, which means the run grows harder to afford exactly as it grows harder to
survive.

### Measured difficulty

| player | FORGIVING | THE ROAD | UNFORGIVING |
|---|---|---|---|
| ignores the economy | **0%** | **0%** | **0%** |
| buys fuel, fixed routine | **0%** | **0%** | **0%** |
| plays prices (see the bot below) | **61%** | **47%** | **27%** |

Over 1,000 seeds each, zero stalls. The first two follow fixed key sequences and
cannot react to a price, so they die out on the road; only the third trades.

These numbers are **not** comparable to anything published before v4. The
generator was re-seeded into three independent streams, so every seed is a
different run — and, more importantly, the bot that produced the old figures
sampled prices by loitering and thirsted itself to death on a reserve that
collapsed whenever the day parity was wrong. The difficulty it reported was
substantially its own handicap. See `TESTLOG.md`, phase P3.

## Controls

| key | does |
|---|---|
| ↑ ↓ | select |
| Z | buy · accept · travel |
| X | sell · refuse |
| ← → | walk between places in town |
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
| `-B` | play with the price-aware bot |
| `-N n` | run seeds 1..n in one process, then print a SWEEP line |
| `-D n` | difficulty: 0 forgiving, 1 the road, 2 unforgiving |
| `-A ref` | play with the frozen v4-entry agent instead of the current one |
| `-Q` | shrink the drawn area — ~50x faster, identical results |
| `-Z` | replay every seed and compare a state hash |
| `-X` | hunt for a profitable buy-then-sell round trip |
| `-K` | per-encounter-kind accept/refuse/forced report |
| `-S n` | photograph the first frame showing tab n |
| `-Y` | fail on any frame where two pieces of text overlap |
| `-U n` | fit upgrade n regardless of what the bot chooses |
| `-C n` | put crew member n aboard regardless |
| `-V n` | grant archetype n's service free at every stop |
| `-I n` | force every rumour true (1) or false (2) |
| `-M n` | which human pressures the bot feels |
| `-J n` | photograph the watchlist at bot step n |
| `-R` | refuse every encounter (reachability probe) |
| `-E` | dump the end screen |
| `--daily` | play today's fixed map |

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
average of every price it has seen — once per market, not once per keypress —
sells above it, buys below it, keeps fuel and water reserves summed over the
days ahead, and scores the road two hops out.

It takes **facts** from `world.h` and never a **valuation**. That rule is not
stylistic: `world_crew_price` is a fixed percentage of `world_crew_payback`, so
the old hiring test `payback > price` reduced to `p > 0.45p` — true for every
positive p, at any price, however wrong the payback figure was. A tautology
cannot discover that a price is wrong.

It is compiled **only into the headless harness** and contributes zero bytes to
the submitted executable.

It exists because scripted key sequences measure only the floor — a fixed string
of presses cannot look at a price and decide. The first thing the bot revealed
was that the game was far too easy for anyone who knew what they were doing: it
won 90% of runs on the original ten-sector route.

The more useful thing it revealed came later, in v4: once its own faults were
fixed it played twenty points better on a game that had not changed, which
meant three releases of recorded difficulty had been measuring the instrument
rather than the game. Every number in `TESTLOG.md` above the v4 epoch marker is
kept as a record of what was tried, and none of it is a current measurement.

The three difficulties move every field — starting capital, water, fuel, the
fuel price curve, storm severity, storm frequency and settlement density — not
one multiplier.

## Layout

| file | |
|---|---|
| `src/game.h` | the boundary: memory, input, pixels, audio. No OS calls cross it. |
| `src/game.c` | state machine and input |
| `src/state.h` | the run's complete state, shared by game.c and ui.c |
| `src/world.{c,h}` | route generation, markets, travel, survival — pure simulation |
| `src/ui.c` | every screen; reads state, writes pixels, decides nothing |
| `src/render.{c,h}` | rasterizer, 5x7 font, icon vocabulary, procedural portraits |
| `src/scene.{c,h}` | procedural backdrop: sky, dunes, weather, time of day |
| `src/cutscene.{c,h}` | letterboxed panels, typewriter text, endings and vignettes |
| `src/audio.{c,h}` | synthesiser and sequencer, integer maths only |
| `src/text.h` | every string in the game |
| `src/bot.{c,h}` | the price-aware test agent — harness only |
| `src/bot_ref.{c,h}` | a frozen copy of it, so the game can be held fixed |
| `src/platform_win32.c` | submission target |
| `src/platform_headless.c` | development harness |

## Status

**Version 6**, 123,904 bytes — 8.4% of the floppy.

A payload of seed stock that cannot be sold and decides the ending; fourteen
encounter kinds, all measurably real decisions; five recurring characters who
can be recruited, carry personal errands, offer a third way through any
encounter, and desert you after a warning; towns with names, finite stock, a
situation to walk into and a trade only they will do; word of the road from
people whose reliability depends on how you have treated them; contracts, kit,
three difficulties and a daily map; procedural backdrop with weather and time of
day; cut scenes, five endings and procedural audio.

    65 / 48 / 29 win rate across 3,000 bot-played runs at the v6 gate
    zero crashes, zero stalls, zero text collisions
    determinism, market-exploit and layout probes on every phase

Verified running on Windows by CI, which launches the binary and screenshots it
on every push.

### Recorded, not fixed

Two things measured badly and are documented rather than quietly rebalanced —
see `TESTLOG.md` for the numbers:

- **Word of the road carries no mechanical advantage.** A perfect oracle is
  worth zero win-rate points, because a hop offers two or three
  near-interchangeable places and knowing more about one cannot pay. Fixing it
  means making route generation produce sectors that differ.
- **The mechanic fell from +11 to +3** when granted free. The new shops do what
  the mechanic does, and made the role largely redundant without anyone deciding
  to.

The backdrop is generated every frame and stored nowhere: a Bayer-dithered sky,
three parallax dune layers summed from sines, a dithered sun corona, and dust
motes derived from the tick counter so they carry no state.
