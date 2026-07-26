# Test log

Every measurement taken against convoy, with the change that prompted it and
the decision it led to. Append to this; do not rewrite history. A number with
no record of what produced it is worth very little six weeks later.

## How to reproduce any row

```sh
./build.sh                                     # both targets, hard size check

# Win rate. This is the primary instrument.
w=0; for s in $(seq 1 150); do
  r=$(./build/convoy_headless -s $s -B -e 0 -o /tmp)
  case "$r" in *WON*) w=$((w+1));; esac
done; echo "$((w*100/150))%"

# How runs end, which matters as much as how often they end well.
for s in $(seq 1 50); do
  ./build/convoy_headless -s $s -B -e 0 -o /tmp -v \
  | grep -E "^ *[0-9]+ .*DEAD" | tail -1 | awk '{print $NF}'
done | sort | uniq -c | sort -rn

./tools/asan.sh                                # memory and UB check
```

`-B` runs the price-aware bot in `src/bot.c`. It plays through the real UI, so
it measures the game rather than the simulation.

## Reading these numbers

- **Skilled** is the bot. It is the ceiling a thinking player approaches, not a
  typical outcome.
- **Careless** is a fixed key sequence that ignores prices. It should always be
  0%: if it is not, the economy is optional.
- **Target band** is 40-50% skilled. Below 30% the game reads as unfair; above
  60% the decisions stop mattering.
- **STALLED** means the bot made no progress for 4,000 steps. It is always a
  bug, never a balance result.
- A sweep that straddles a rebuild is **invalid** — each seed re-invokes the
  binary, so half the sample measures the old build.

---

## Baseline: v1

| config | skilled | careless | notes |
|---|---:|---:|---|
| 10 sectors, 30-slot hold | **53%** | 0% | shipped as v1, tagged |

## Phase 0 — longer route, scrolling map

| change | skilled | notes |
|---|---:|---|
| 14 sectors, resources unchanged | **0%** | bot deadlocked at sector 0 |
| cr 170 / fuel 6 / water 10 / fuel-scale 3 | 13% | still starving |
| settlements 34%→46%, cr 200 / f 7 / w 11 | 22% | resupply density matters more than starting stock |
| ~~cr 260 / f 9 / w 13 / scale 2~~ | ~~63%~~ | **invalid** — sweep straddled a rebuild |
| cr 150 / f 6 / w 9 / scale 4, **provisioning fix** | **44%** | careless 0%. Accepted. |

**Finding.** Stocking fuel and water for the whole remaining route is 29 units
against a 30-slot hold on a 13-hop journey. The hold filled with consumables,
leaving no room to trade and no way to earn; the bot then pressed BUY at a full
hold forever. **Hold size and route length are the same number.** Provisioning
now plans 5 hops ahead.

## Phase 1a — archetypes and price context

| change | skilled | notes |
|---|---:|---|
| archetypes + price context, bot unchanged | 34% | bot had no concept of either |
| + bot archetype awareness, **+ bid-ask spread** | 26% | spread cut trading income 20% |
| + fuel base 20→17, scale 4→2, buy bump 10%→6% | **46%** | accepted |

Death causes at 46%: **22 STRANDED / 7 THIRST / 1 STRIPPED**. Monotone — fuel
was effectively the only failure mode.

**Finding.** Market memory had an infinite money loop that shipped in v1.
Buying nudged a price up ~10% and you could sell into your own nudge: buy 20,
price becomes 22, sell for 22, repeat. The bot found it accidentally and
reported 4,141 credits at sector 0 on day 1. Fixed with a 20% bid-ask spread.
**Any economy where the player's trades move prices needs this check.**

## Phase 1b — contracts

| change | skilled | notes |
|---|---:|---|
| contracts, reward 3.4× cargo value | 54% | 408 credits vs 150 starting capital |
| reward ~2×, work at 45% of stalls | **50%** | accepted |

## Phase 2 — upgrades and crew

| change | skilled | notes |
|---|---:|---|
| upgrades + crew, kit priced 220-300 | **48%** | see finding — nothing was ever bought |
| kit repriced 130-175, crew 85-120 | **39%** | kit now bought (8 buys / 30 runs) and win rate *fell 9 points* |
| upgrade effects fixed, repriced 120-150 | **40%** | still below the 48% the bot got by ignoring the garage |
| payback-aware purchasing | ~~35%~~ | **invalid** — edit never reached bot.c |
| ~~surplus-only buying~~ | ~~30%~~ | **invalid** — same, identical results gave it away |
| payback + surplus, *verified in source* | *pending* | grep before build, not after |

Death causes at 48%: **16 STRANDED / 7 THIRST / 1 STRIPPED**. Thirst rose from
13% to 29% of deaths once crew started drinking — the intended effect.

**Finding: two upgrades were no-ops.** Water tanks halved the daily water burn
rounded up, so with no crew aboard a burn of 1 became a burn of 1 -- no effect
whatsoever, in exactly the situation a player is most likely to buy them. The
scout was sold as "see two sectors ahead" when the entire route is already drawn
on the map: a purchasable nothing. Both now do something concrete (a dry day
every third day; storms cost nothing).

Neither showed up as a crash, a stall or a bad win rate. They were found by
working out by hand what each upgrade was worth in credits and noticing two of
them were worth zero. **Write the payback arithmetic down; do not assume an
effect exists because you wrote the code for it.**

**Finding.** Kit at 220-300 credits was unaffordable: a winning run banks under
100. Across five complete bot runs, **not one purchase was ever made**. Content
that exists and cannot be reached is worse than content that does not exist,
and only an agent that plays every run to completion will tell you.

### RESOLVED in phase 2A: kit is now a genuine choice

| price (% of remaining payback) | skilled win | n |
|---:|---:|---:|
| 75% | 44% | 150 |
| 60% | 39% | 150 |
| 35% | 59% | 150 |
| **45%** | **51%** | **250** |

Parity is 45%: buying wins 51%, ignoring the garage won 49%. Both are
defensible, which is the definition of a decision. Kit is bought in 39 of 40
runs versus 2 of 30 before.

Two notes on reading this. The 75/60/35 rows were 150-seed samples where the
standard error is about +/-4 points, so 44% and 39% are the same number and
only the 35% row was a real signal -- the sequence looks non-monotonic but is
not. The final figure was re-measured at n=250 for that reason.

Salvaged kit is priced at 67% of sound, which is exactly its reliability
(it fails about one run in three). That makes the two options equal in
expectation and different only in variance -- a real gamble. Priced any lower,
as it was at 45%, salvaged is strictly better and there is no choice to make.

### ORIGINAL PROBLEM: kit lost to working capital

With payback-aware, surplus-only purchasing the bot buys kit **twice in thirty
runs**. It is not being stupid: credits in the hold compound. Buy low, sell
high, repeat over six legs and 100 credits becomes ~300. No fitting returns
that, so the "decision" is not a decision -- it is a dominated option that the
game politely offers.

The target is a toss-up, or failing that a gamble worth taking. Three angles,
in rough order of how well they fit what is already here:

**1. Price kit against the back half, not the front.** Kit currently gets
*dearer* by sector (`world_upg_price` scales +5%/sector) while its value goes
*up* late, because there are fewer legs left for capital to compound through
and because market memory has already burned the easy routes. That escalation
is backwards. Removing or inverting it makes kit the natural thing to do with
late-run money.

**2. Value it in survival, not credits.** The payback model prices fuel saved
at a flat 22 credits. But fuel saved *when the tank is empty* is worth the
entire rest of the run, and fuel saved when rich is worth 22. Roughly half of
all runs end in death; kit that removes a specific death -- econ against
stranded, tanks against thirst, armour against stripped -- is worth far more
than its expected credit value. Pricing it by expectation systematically
undervalues it.

**3. Make it an actual gamble.** Cheap kit with a failure chance, or kit paid
for in cargo rather than credits, turns a dominated option into a bet. A bet
that is occasionally wrong is more interesting than a purchase that is reliably
mediocre.

Angle 1 is a two-line change and should be tried first. Angle 2 needs the
payback model rewritten in terms of probability of ruin, which is the more
correct model and a bigger job.

### OPEN: every crew member is net-negative

Worked out in credits at 13 hops remaining, before their keep is deducted:

| crew | saves | keep (water) | net |
|---|---:|---:|---:|
| mechanic | 90 | 169 | **-79** |
| guard | 150 | 169 | **-19** |
| medic | 80 | 169 | **-89** |
| scout | 105 | 169 | **-64** |
| trader | 117 | 169 | **-52** |

There is no correct hire, only a less-wrong one. The cause is structural rather
than a price being off: each crew member helps with exactly **one** encounter
type, and encounters are only ~30% of nodes, so any given hire pays out once or
twice a run while drinking every single day.

Deliberately **not** fixed by inventing numbers here. Phase 3 takes encounters
from 5 kinds to ~14 and makes their outcomes depend on crew, which changes this
arithmetic materially. Re-derive the table above after Phase 3 and price crew
against the result.

The general rule this is an instance of: **an ability that triggers on a rare
event cannot pay for a cost that recurs every day.** Either broaden what it
applies to, or charge for it per use rather than per day.

## Phase 3 — fourteen encounter kinds

| change | skilled | notes |
|---|---:|---|
| 5 kinds -> 14, generic bot evaluation | **53%** | n=200, 0 crashes, 0 stalls |

Death causes: **17 STRANDED / 11 THIRST**, i.e. 61/39. The monotone 87/13 the
game started with is gone; runs now end in more than one way.

**The bot no longer knows what an encounter is.** It reads pay/gain/lose and
their quantities, values them against what the convoy currently needs, and
takes the better side. All fourteen kinds resolve through one function, so a
fifteenth needs no bot change -- which matters, because a mechanic the bot
cannot judge is a mechanic nobody can measure.

One deliberate distortion in that valuation: fuel and water are worth **3x**
their price when stock is below the reserve. A flat price model will happily
trade away the thing that is about to kill you.

### Correction: more encounter kinds did NOT rescue crew

Phase 2 logged the expectation that tripling encounter variety would make
specialist crew worth hiring. **That was wrong, and backwards.** Encounters are
still ~30% of nodes, so spreading them across 14 kinds instead of 5 means any
one kind fires *less* often, not more. Each crew member covers about two kinds
-- roughly 0.6 triggers per run against a keep of ~169 credits of water.

Crew are still net-negative. The fix has to be broader coverage or a lower
keep, not more content. Carried into phase 4.

## Phase 4 — crew economics, final balance

| change | skilled | engagement |
|---|---:|---|
| crew broadened + alternate-day rations | 53% | crew still never hired |
| crew priced off payback, as kit already was | **42%** | 201 fittings, 47 crew / 200 runs |

Final: **42% skilled, 0 crashes, 0 stalls, sanitizers clean, n=200.**
Deaths **19 stranded / 15 thirst** — a 56/44 split against the 87/13 monotone
this work started from.

### What actually fixed crew

Two changes, and the second was the one that mattered.

Broadening coverage (each hand now handles a category of trouble -- raids,
tolls and checkpoints for a guard -- rather than one encounter kind) and
halving their keep to alternate-day rations took every crew member from
net-negative to net-positive on paper: the table that read -79 to -19 now reads
+72 to +231.

The bot still never hired one. Crew were priced off a flat base while kit was
priced off payback, so they were unaffordable exactly when they were worth
having -- the identical trap kit had already been pulled out of in phase 2A.
Pricing them the same way fixed it in one change.

**A fix that works on paper and changes nothing in play means the constraint is
somewhere else.** Look at what the agent can afford, not only at what the thing
is worth.

### The instrumentation that should have existed from the start

The harness now reports what a run ended up owning:

    BOT seed=16 WON sector=13 day=14 credits=18 cargo=6 upg=2 crew=1 steps=134

Two extra integers. Four separate problems this session -- unaffordable kit,
two no-op upgrades, unaffordable crew, and kit that lost runs when bought --
all hid behind clean sweeps and a healthy win rate. An option nobody takes and
an option that loses are indistinguishable in an outcome metric.

# v3

## Phase A — the payload, cut scenes, endings

| change | skilled | endings reached |
|---|---:|---|
| payload + cut scenes, seed only lost when hold is empty | 40% | DEAD 119, INTACT 81 |
| seed targeted by anyone who searches the hold | 40% | + PARTIAL 1, EXEMPLARY 9 |
| storms spoil seed (unavoidable attrition) | **41%** | DEAD 118, PARTIAL 5, INTACT 68, EXEMPLARY 9 |

n=200, 0 crashes, 0 stalls, sanitizers clean.

**Finding: a threat you can pay off is not a threat.** The first attempt let
raiders take the seed only once the tradeable hold was empty, then let them
target it directly. Neither moved the numbers, because the bot values the seed
at 90 credits a slot and simply *pays* every time -- which is correct play.
Seed loss only ever happened when the convoy could not afford the price.

Two of five endings stayed unreachable until storms were given a 35% chance to
spoil a crate. That threat cannot be paid off, argued with or fought, and it is
the only reason attrition exists at all.

The general shape: **if every risk to a thing is purchasable, a competent player
never loses it.** Something has to erode it unconditionally or the failure
states are decoration.

### OPEN: the EMPTY ending is still unreached

Arriving with none of the seed has not occurred in 200 bot runs. It requires
losing all six crates, and an optimal convoy avoids storms and pays off raids.
It is reachable in principle -- a careless human will find it -- but by the
standard applied to unaffordable kit and no-op upgrades, unverified content is
unverified. Either force reachability in phase F or cut the ending.

**RESOLVED in phase F.** It was not reachable in principle either: the
arithmetic came to 0.29 crates lost per run against six needed. Payload demands
now escalate with depth, and the ending is demonstrated with a refuse-every-
encounter probe. See the phase F section below.

## Phase B — the audio engine

| metric | before | after |
|---|---:|---:|
| engine | one fixed 16-step pattern, 3 voices | 4-channel sequencer, 6 instruments, 6 songs |
| lines | 117 | 268 |
| peak | -2.1 dBFS | -3.1 dBFS |
| DC offset | -13 | **-6** |
| clipped samples | 0 | 0 |

Win rate 39% (n=150), unchanged within noise: audio is presentation only.

**Finding: a duty-cycle square has a DC offset.** The first render measured a
-1290 DC and 298 clipped samples. A plain +/- full-scale square at 37% duty
spends more time low than high, so its mean sits at -8520 -- audible as a thump
on every note, and it eats the headroom the music needs. Scaling each half by
the other's share puts the mean at zero for any duty:

    hi = 32767 - duty * 128;   lo = -duty * 128;

**Debugging note.** Two moods reported byte-identical audio and the first
suspicion was another silent edit failure. It was not: the test scripts never
dismissed the three-panel opening cut scene, and while a cut scene owns the
screen `game_update` returns before it reaches the music. A packed probe --
`cut*1000 + title*100 + mood*10 + mood_next` -- gave the answer in one run
where single-value prints had failed three times. When a guess has been wrong
twice, return the whole state path as one number.

## Phase C — day/night, weather, transitions

Presentation only; win rate unchanged within noise.

**Finding: a physically correct night was the wrong night.** The first pass
took the palette to cold blue-grey after dusk and blended the land 150/255
toward the sky. Every panel, icon and goods colour in the game is warm, so the
backdrop fought all of them and dark sectors read as muddy rather than
atmospheric. The fix was to keep night inside the game's own range -- a dusty,
lamp-lit night -- and to cut the land tint to 70/255 so the desert keeps its
own colour. Night now also has mechanical weight: dark settlements put fewer
offers on the board, so it is a condition rather than a filter.

**Finding: a full-screen transition is a large share of what a player sees.**
A dithered wipe fired on every state change. It looked good in motion and
awful in a still, and because it covered the whole frame it turned up in two
screenshots taken at random and made the game look broken. It was cut to a
brief scrim dip, and then measured: at its first setting the dip peaked at 62%
ink over 8 frames.

    trans = 8, level = trans > 4 ? (8 - trans) * 2 + 4 : trans * 2   -> peak 10/16

The bot spends exactly one frame per step, and an encounter lasts about three
steps, so the transition outlived the screen it was introducing -- the harness
could not photograph a clean encounter at all. Cut to 4 frames with level =
trans (peak 4/16, ~66ms). **If a screenshot keeps catching an effect by
accident, that is a measurement of how much of the game the effect is.**

---

## Phase D — characters, dialogue and journal

Five recurring characters (`met[]`, `regard[]` in `World`), portraits generated
from a seed, dialogue that differs on first meeting / warm return / cold
return, and a PEOPLE tab recording who was met, how often, and where you stand.

| metric | phase C | phase D |
|---|---:|---:|
| size | 106,496 B (7.22%) | 107,008 B (**7.26%**) |
| win rate (n=200) | 42% | **36%** |
| stalls / crashes | 0 | **0** |
| encounters per run | 1.76 | **2.75** |
| runs meeting nobody | 43 / 200 | **7 / 200** |
| runs meeting 3+ | 19 / 200 | **53 / 200** |

New harness counters: `met=`, `regard=`, `enc=`.

**Finding: the story was competing with the profitable route, and losing.**
The generator makes 30% of nodes encounter nodes, which should give ~3.6 per
run. Measured: **1.76**, and 43 of 200 runs -- including 19 of 84 *winning*
runs -- met nobody at all. The cause was structural, not statistical: money is
made in settlements, so the route that pays is the route that skips the story.
Every portrait, dialogue line and journal entry was content a good player was
paid to avoid.

The fix was to stop making the two compete. A settlement arrival now has a 14%
chance of also carrying an encounter, returning to the market afterwards
(`after_event`). Meeting people where the people are costs nothing to opt into.

**Finding: adding encounters costs win rate, which means encounters are a tax.**
Raising encounter frequency dropped the win rate 42% -> 30%. A genuinely
even-money encounter table would have added *variance* without moving the mean.
That it moved the mean by twelve points says the fourteen kinds are net
negative EV against a competent player. Two partial fixes were applied --
market meetings re-roll once if they come up a threat (raids happen on the
road, deals happen in town), worth +3 points, and the rate was cut from 20% to
14%, worth another +3 -- landing at 36%. **The underlying EV imbalance is left
for phase F**, which is the rebalance phase; it should be fixed in the payoff
tables, not by hiding the content.

**Bug: the journal could never be opened.** `cycle_tab` skipped any tab whose
`ui_tab_rows` was zero, and the journal deliberately returns zero because it is
a record rather than a menu. It was therefore skipped on every press. The tab
strip already had the right predicate (`tab_live`); the fix was to export it as
`ui_tab_live` and have both ask the same question. Found by reading the code,
not by any sweep -- **no bot test would ever have caught it, because the bot
never opens the journal.** The screenshot flag added for it (`-J`) drives real
tab presses rather than reaching into `GameState`, so the shot proves the tab
is reachable.

**Bug: three of the five faces were near-identical.** `draw_portrait` reads
skin, cloth, hat, beard and scar from separate bit fields of one seed, and
nothing forces those to differ between characters. Evenly spaced seeds
(`who * 977 + 13`) collided; replacing them with a hash collided differently.
Both attempts were *hoping* rather than checking. Fixed by searching for five
seeds that yield deliberately chosen combinations and hard-coding them. **With
five of something and 1.3MB spare, pick them; do not roll for them.**

**Repeat offence: a scripted `.replace()` silently did nothing.** A header
declaration failed to land because the file had `int  world_event_char` with
two spaces and the pattern had one. This is the same failure already recorded
under phase 2, and it cost a build cycle -- during which `asan.sh` reported
"sanitizers clean" against a stale binary. **A green result from a build that
failed is worse than a red one.** Use `Edit`, and grep for the new text before
believing any number that follows.

**Still open.** `crew=0` in 159 of 200 runs: the crew board is bought from far
less often than the garage (`upg>=1` in 148 of 200). Combined with the
encounter-EV finding above, phase F has two known balance targets rather than
one.

---

## Phase E — difficulty modes and the daily seed

Three settings chosen at the title, plus a daily run: one map a day, the same
one for everybody, with a score to compare.

| difficulty | skilled win (n=200) | careless (n=40) | stalls |
|---|---:|---:|---:|
| FORGIVING | **64%** | 0% | 0 |
| THE ROAD  | **46%** | 0% | 0 |
| UNFORGIVING | **25%** | 0% | 0 |

Target bands were ~60 / 40-45 / ~25. All three land inside or within one
standard error (n=200 gives about +/-3.5 points).

**Difficulty leans on a different death, not on a bigger number.** The design
claim was that the modes should fail *differently*. Measured, over 120 seeds
each:

| difficulty | THIRST | STRANDED |
|---|---:|---:|
| FORGIVING | 23 (61%) | 15 |
| THE ROAD | 32 (54%) | 27 |
| UNFORGIVING | 34 | **54 (61%)** |

Easy and normal are thirst-led; hard inverts to stranded-led, because the
settlement density drop and the steeper fuel curve bite before the water does.
The claim holds and is now a measurement rather than an intention.

The table is six numbers per row -- starting credits, water, fuel, the fuel
price curve, storm spoilage and settlement density -- and settlement density is
by far the strongest lever. It is resupply: 52/48/44 percent moves the win rate
more than every other field combined.

**Bug: the opening cut scene played before the title, and then again.**
`game_init` calls `restart()` to build a world for the title screen to sit in
front of, and `restart()` begins the opening. So the opening ran first, the
title appeared after it, and pressing start replayed the whole thing. It had
been there since phase A and nobody noticed because nobody had played by hand
and the bot presses through everything. It surfaced only when the new title
menu stopped responding -- the cut scene was eating the keys. **A modal screen
that swallows input hides itself; the symptom shows up in whatever was supposed
to receive that input.**

**Bug: paying for goods there was no room for.** `world_can_accept` checked
whether the player could afford an encounter but not whether the hold could
take what was offered, and `world_accept` clamped the gain to the space
available. With a full hold the player paid the price in full and received
nothing, while the panel advertised the goods. Worth only about one point of
win rate -- the bot rarely runs full -- but it is the one thing an encounter
must never do, since the whole system asks the player to take the offer at its
word.

## Phase F — rebalance, and closing the last open item

**RESOLVED: the empty-handed ending, open since phase A.** `OUT_EMPTY` had
never occurred in 200 runs, then 800, across every difficulty. The standing
decision was "force reachability or cut it".

Cutting it was nearly the right answer, because the arithmetic said it was
decoration. Raiders, tolls and checkpoints past sector 4 demanded the seed with
a flat 45% chance for 1-2 crates. Encounters average 2.75 per run; roughly two
fall past sector 4; three of the fourteen kinds search the hold. That is
2 x (3/14) x 0.45 = **0.19 demands per run**, at 1.5 crates each: **0.29 crates
lost per run against six needed.** No sample size would ever have reached it.

Instead both the chance and the price now climb with depth -- `45 + depth * 3`
percent, for `1..2 + depth/5` crates. Late raiders know exactly what is in the
crates. Verified with a new probe (`-R`) that refuses every encounter, which is
the only player who can lose the seed at all:

| difficulty | EMPTY (refusing, n=200) | EMPTY (paying, n=200) |
|---|---:|---:|
| FORGIVING | 3 | 0 |
| THE ROAD | 4 | 0 |
| UNFORGIVING | 2 | 0 |

All five endings are now reachable and demonstrated. Skilled win rates were
unchanged by the change (64/46/25 before and after), because a player who pays
raiders never loses a crate to them -- which is exactly the intended shape: the
ending belongs to the player who refuses, and the cost is real.

**A probe is not a strategy.** `-R` exists to prove content is reachable, not
to play well; it wins less often than the ordinary bot. Keeping the two
separate is what stops "reachable" and "sensible" being confused.

**Teaching arc, re-verified now that a cut scene and a title menu exist.**
The opening is 301 characters at 2 ticks each: **10 seconds** if never skipped,
and any key advances it. Title -> opening -> first market is immediate (the
starting node is a settlement), and the first encounter arrives at bot step 31.
Well inside five minutes with room to spare.

**Win-screen wash.** Arrival tinted the whole frame green at a quarter
coverage, which swallowed the sky, the sand and the convoy -- the same mistake
as the cold night in phase C, in the opposite colour. Cut to an eighth.

---

# v4 — the refinement cycle

**Everything above this line is void as a current measurement.** It stays as a
record of what was tried and what was learned, but no number from v1 to v3 can
be compared with anything below it, for three independent reasons:

1. The generator was one RNG stream. Map layout, market offers, contracts,
   encounters and salvage failure all drew from it, so editing any table
   reshuffled every later roll and a seed stopped being the same run. Paired
   before/after comparison was never valid. Split in P1 into `rng_map`,
   `rng_offer` and `rng_event`.
2. The bot sampled prices once per *keypress* rather than once per arrival, so
   its running average was weighted by how long it stood in each shop — and
   the weighting was a function of how much it traded. Fixed in P3.
3. The bot's hire test was `crew_payback(...) > price` where `price` is defined
   as `payback * 45 / 100`. That is `p > 0.45p` — true for any positive
   payback. There was no economic filter on crew at all. Fixed in P3.

The v3 headline of 64% / 46% / 25% is not recoverable and is not a target to
return to.

## P0 — make the build able to fail

CI built every game with `ONLY_WIN=1`, which skips the harness. **`src/bot.c`
and `src/platform_headless.c` were never compiled by CI at any point in three
releases.** The entire measurement layer could have been deleted and every run
would still have gone green. A Linux job now builds them, runs the sanitizers,
and plays 20 seeds on each difficulty, failing on a stall or on a difficulty
that never wins.

`-Werror` added. Warnings had been on since the first commit and accumulated
anyway: an unused duplicate of `world_upg_payback` in `bot.c`, two dead price
tables (`UPG_BASE`, `CREW_BASE`) that nothing had read since pricing became a
function of payback — **and that two separate audits mistook for live data** —
a dead audio pattern, a misleading indentation, and an enum-compare in an array
bound.

The gate was verified by introducing a warning and confirming the build failed.
A gate that has never been seen to fire is not known to work.

`tools/sync.sh` added. The development tree and the shipped tree are separate
copies with no link; the only thing keeping them in step was remembering to
copy by hand, and nothing would have reported a drift, because every sweep runs
against the tree that does not ship.

Exit check: **BOT lines for seeds 1..50 byte-identical before and after.**

## P1 — the harness becomes an instrument

No game rules changed. Everything here is measurement.

### The RNG split, and the proof it works

The invariant to establish was: *editing an encounter table must leave every
map exactly where it was.* Tested by inserting one extra `rng_range` draw into
the `EV_RAID` case, rebuilding, and comparing map hashes across 100 seeds on
each difficulty:

| difficulty | maps after an encounter-table edit |
|---|---|
| FORGIVING | unchanged |
| THE ROAD | unchanged |
| UNFORGIVING | unchanged |

while the win rate moved (75 → 74 of 200), which is the encounter stream doing
its job. Before the split, the same edit would have moved every map.

**The first attempt at this test proved nothing.** The scripted insert missed
its anchor, the assertion fired, and the comparison ran against an unmodified
binary — reporting "maps unchanged" for a probe that was never there. This is
the same silent-edit failure already recorded twice in this log. The rule holds
and needs restating: *grep for the probe before believing the result of the
probe.*

### New harness capabilities

| flag | question it answers |
|---|---|
| `-N n` | run seeds 1..n **in one process** |
| `-A ref\|v4` | play with the frozen v4-entry agent, or the working one |
| `-Z` | replay every seed and compare a step-by-step state hash |
| `-X` | is there a profitable buy-then-sell round trip anywhere? |
| `--daily` | drive the daily map through the title menu |

`-N` removes a whole class of invalid result structurally: every sweep in this
log until now was a shell loop re-invoking the binary per seed, which is
exactly how a sweep straddles a rebuild and reports half of one build and half
of another. A sweep is now one process and cannot.

Acceptance test for `-N`: 50 seeds in-process must be **byte-identical** to 50
shell-loop invocations. They are. `game_init` assigns fields individually and
never clears `GameState`, so the arena is zeroed per seed — without that the
transition timer, cut-scene state and audio phase carry over.

`-Z` clean on 100 seeds × 3 difficulties. `-X` clean: the best round trip nets
**+0** at a list price of 1, where the sell clamp floors the take — you end
with the same credits and the same goods and the buy nudges the price up, so
nothing is farmed. Strictly positive is the failure condition.

**Bug found while adding `--daily`: the map hash was being taken from the wrong
world.** `game_init` builds a world for the title screen to sit in front of,
and pressing start builds the real one. Hashing on the first step captured the
title's placeholder. For an ordinary run the two are generated from the same
seed and are identical, so it looked correct — but every `-D 0` and `-D 2` hash
was reporting the *normal-difficulty* map, and a daily run reported the
non-daily one. Found only because the daily map came back identical to the
standard map when it had no reason to.

### Engagement counters — first readings (n=150, THE ROAD)

Compiled into the harness only, via `-DCONVOY_INSTRUMENT`. The Windows binary
is **byte-identical md5 with and without them**: the contest target does not
carry its own test rig.

| measurement | reading |
|---|---:|
| encounters per run | 2.76 |
| accepted | 0.85 |
| **forced declines** | **0.57** |
| contracts offered | 1.48 |
| contracts accepted | 1.47 |
| **contracts delivered** | **0.63** |
| crates lost — storm / demand / random | 0.08 / 0.15 / 0.00 |
| realised ÷ headline on sales | **78.7%** |
| biggest stack sold at one market | 2.6 units |
| hold occupancy — mean / peak | **69% / 94%** |
| min water / min fuel reached | 1.3 / 1.2 |
| deaths — thirst / stranded | 48 / 44 |

### Epoch zero — the baseline every later phase is measured against

n=400 per difficulty, in one process, both agents. The reference agent is a
byte-for-byte copy of the working bot today, so identical columns are the
expected result and a divergence would mean the copy was not faithful.

| difficulty | frozen `-A ref` | working `-A v4` | stalls |
|---|---:|---:|---:|
| FORGIVING | 68% (275/400) | 68% (275/400) | 0 |
| THE ROAD | 41% (167/400) | 41% (167/400) | 0 |
| UNFORGIVING | 25% (102/400) | 25% (102/400) | 0 |

At n=400 the standard error is 2.5 points, so differences below ~5 points
between independent sweeps are not differences. Paired comparison on the same
seed set resolves considerably finer, and is the default from here.

These are **not** comparable to v3's 64/46/25: the generator was re-seeded into
three streams, so every seed is a different run. The game was not made harder.

### Three findings that revise the plan

**Forced declines are 30% of all refusals.** Of 1.91 declines per run, 0.57 are
the game refusing on the player's behalf because the price could not be paid.
Every previous statement about players "choosing" to refuse an encounter was
measuring a mix of choice and inability, with no way to tell them apart. This
is the counter that had to exist before P7 can tune anything.

**Sales realise 78.7% of headline, not 41%.** The 41% figure was arithmetic for
a 10-unit stack. Measured, the largest stack sold at any one market is **2.6
units** — the decay barely engages, because nobody sells ten of anything. The
asymmetry between buy impact (`p/16 + 1`) and sell impact (`p/8 + 1`) is still
real and still wrong, but **its measured cost is a fifth of what the arithmetic
implied.** P4 stands, with its justification corrected: it is a fairness fix and
a guard against future stack-selling, not a recovery of 60% of the economy.

**The hold runs at 69% mean and 94% peak occupancy, not 30-45%.** The earlier
figure was *final* cargo, read at the end of a run after selling down — not
occupancy during it. `UPG_HOLD` is therefore not untested content, and P5's
premise that the bot never feels hold pressure is wrong. The pressure is
already there; what is missing is the bot *valuing* the space.

All three came from counters, none from a win rate, and all three corrected a
conclusion that had been reached by arithmetic alone. That is the argument for
this phase.


## P2 — correctness bugs with no design content

Seven fixes, all cases where the current behaviour was impossible to defend
rather than merely unbalanced. Measured against the frozen `-A ref` control,
which is why this phase comes before any work on the bot: it needs no competent
agent to show that a run ended for a reason the rules do not support.

| difficulty | P1 epoch zero | P2 exit | change |
|---|---:|---:|---:|
| FORGIVING | 68% | **72%** | +4 |
| THE ROAD | 41% | **46%** | +5 |
| UNFORGIVING | 25% | **27%** | +2 |

n=400 each, zero stalls, `ref` and `v4` identical (the bot is untouched in this
phase). The win rate rose because three of these bugs killed runs the rules say
should have continued. **Recorded, not tuned back** — the balance phases come
later and will be measured from here.

### The fixes

**Armour was overwritten by the thing it protects against.** The clamp on what
refusing costs was applied inside each of the three raid-family cases, and the
payload demand at the end of `roll_event` then overwrote `lose_good` and
`lose_qty` wholesale. So the one fitting sold as protection gave exactly none
from sector 4 onward — the half of the run a player buys it for. Armour and the
bad-standing surcharge now both apply *after* the demand, and armour covers
crates as well as cargo: one crate back is worth more than a full hold.

Measured with a forced-policy A/B — same seeds, armour fitted or not,
refuse-every-encounter on UNFORGIVING so that demands always bite, n=300:

| | crates lost to demands |
|---|---:|
| armour off | 177 |
| armour on | **151** |

A 15% reduction, against **107 versus 107** — bit-for-bit identical — before
the fix. Note this is deliberately not a win-rate measurement: armour protects
the *ending*, and a crate is 500 score against a win's 1000, so its value never
had to show up in a won/lost column and did not.

**Standing was asymmetric and then discarded.** A discount required
`pay_qty > 1` and a gain bonus required `regard > 1`, while both penalties
required only `regard < 0`. Half the encounter table has `pay_qty` of exactly
1, so on those kinds good standing bought nothing at all while bad standing
always cost. And the surcharge was applied *before* the payload override that
discards it — on precisely the three kinds the raider chief appears in, which
are the only ones his standing affects. Gates are symmetric now, and goodwill
can take a price to zero, the same shape as having the right crew aboard.

**The economiser killed runs.** `UPG_ECON` makes every second hop cost no fuel,
but `world_can_travel` demanded a unit in the hold regardless, and the failure
path declared `DEATH_STRANDED`. A convoy with the economiser fitted, no fuel,
on a free day, was killed for a hop that would have cost nothing — on the one
fitting sold as insurance against exactly that. Both now ask
`world_hop_costs_fuel()`.

**Accepting could kill you.** `end_event` ended the run with `DEATH_STRIPPED`
whenever the hold reached zero, including on the accept path. Several kinds pay
in credits and cost goods, so taking the money for your last unit of cargo
ended the run on the screen that had just shown a gain. Only a refusal can
strip you now.

**Contracts jammed permanently, and some were undeliverable.** `contract_tick`
early-returns while a job is on the board and nothing ever cleared it, so one
ignored offer disabled the contract system for the rest of the run. Offers now
lapse when the convoy leaves. Separately, `by_sector` could be `SECTORS-1` —
the Green Zone, which has no market, so `contract_tick` never runs there and
the job could not be paid out at all. Clamped to `SECTORS-2`.

Measured, normal difficulty, n=200:

| | P1 | P2 |
|---|---:|---:|
| contracts offered per run | 1.48 | **1.89** |
| accepted | 1.47 | 1.31 |
| delivered | 0.63 | 0.56 |

Offers up 28%, which is the expiry working. **Acceptances went down**, which is
the finding: there are now more jobs than the bot can carry, because
`contract_worth_taking` refuses anything that would leave under four free
slots. Previously the board was jammed early and the question never arose. The
delivery rate is unchanged at 43% of accepted. Both belong to P6.

**Vignettes replayed for the rest of the run.** The caller tracked which
*sector* had shown a beat, but two of the five are conditions rather than
places: the seed being gone stays true, and storms recur. So the loss beat
fired again at every remaining settlement — and because it is tested first, it
suppressed the halfway and last-hop beats entirely. A convoy that lost its
cargo saw the same three lines four times and nothing else again. Keyed by kind
now.

The loss beat was also guarded by `payload_lost_to != 0xFE`, and **`0xFE` is
never assigned anywhere in the program**, so that test was always true.

**`payload_lost_to` deleted.** Documented as "what took the last of it, for the
ending", it was written on one of the three paths that can take a crate, stored
a `WorldState` rather than a cause, and had exactly one reader — the dead
sentinel above. With that gone it had none. The harness counts crate losses by
cause properly.

**Arriving through a vignette skipped the arrival.** The vignette branch
returned before the tab reset and the travel sound, so a beat left the cursor
on whatever tab was last used and made no noise — the two signals that say you
have arrived somewhere. It falls through now.

### Process

Three mistakes worth recording, all caught rather than shipped.

**I edited the frozen reference agent.** A one-line contract bound was applied
to `bot.c` and `bot_ref.c` together by a careless `sed` over both files. The
reference agent exists precisely so that it does not change; reverted. The
change was harmless in this instance, which is exactly why the rule has to be
mechanical rather than judged case by case.

**I rebuilt while a sweep was running.** The sweep re-invokes the binary per
difficulty, so it would have reported some difficulties from one build and some
from another. The reasoning that the change was inert was almost certainly
correct and was discarded anyway: sweeps now finish with an `md5sum -c` on the
harness, so a straddled sweep reports itself instead of relying on memory.

**A forced-policy flag that did nothing.** `-U n` fits an upgrade regardless of
what the bot chooses, so armour can be measured with and without on the same
seeds. The first A/B returned **107 crates against 107** — identical. The flag
was setting the upgrade on the world `game_init` builds for the title screen to
sit in front of, and pressing start builds the real world from scratch, zeroing
it. This is the same shape as the map-hash bug found in P1, from the same
cause, three days apart. **Anything the harness pokes into the world has to be
poked after the title, not before.**


## P3 — the bot stops lying

Game frozen; `bot.c` only. This is the first trustworthy baseline of the
release, and it is nothing like the numbers that preceded it.

| difficulty | frozen `ref` | honest `v4` | change |
|---|---:|---:|---:|
| FORGIVING | 72% | **92%** | +20 |
| THE ROAD | 46% | **67%** | +21 |
| UNFORGIVING | 27% | **38%** | +11 |

n=400 each, zero stalls. `ref` is byte-identical to its P2 figures, which is
the proof that the game did not move: **every point of that gain is the
observer, not the observed.**

### The game was never as hard as three releases of this log reported

The difficulty numbers were measuring the instrument's handicap. Two faults did
most of it:

**The water reserve sampled one day's parity and multiplied it out.** The daily
burn alternates — crew drink on odd days, water tanks give a dry day on even
ones — so `span * world_water_burn(w)` swung between two very different
answers depending on which day the convoy happened to arrive at a market. With
tanks fitted on an even day the burn reads 0 and the whole reserve collapsed to
2 units. The bot then sold water down to that and died of thirst a day later.
It now sums `world_water_burn_on` over the days ahead.

**The price average was weighted by loitering.** `observe()` ran on every step,
i.e. every keypress, so a market was counted five to twenty times depending on
how long the bot stood in it — and because `world_buy` and `world_sell` move
the local price permanently, what accumulated was the *moved* price, over and
over. The error was therefore a function of how much the bot traded, which is
precisely what the average is used to decide.

`world.h` carried a comment for three releases saying the bot "keeps the same
running average for itself, so both reason from identical information". It was
not true. It is now, and the harness checks it rather than asserting it: with
`-Z`, at every arrival, the bot's per-good sample count and mean must equal the
world's exactly. 100 seeds × 3 difficulties, clean.

*(The first version of that check failed immediately — bot n=1 against world
n=2. The check was wrong, not the code: the world observes inside
`game_update` and the bot observes at its next `bot_step`, so comparing after
the update straddles a phase boundary. Moved to between the two.)*

### Breaking the circularity

The old hiring test, in full:

```c
return crew_payback(w, k, hops) > price;      // price = world_crew_price(w, k)
```

and `world_crew_price` is `world_crew_payback * 45 / 100`. So the test is
`p > 0.45p` — **true for every positive p**, at any price, whether the payback
figure behind it was right or wrong by a factor of eight. There was no economic
filter on crew at all; the bot hired whoever it could afford. The upgrade test
was worse: it made no value judgement whatsoever and contained a literal
`(void)hops;` discarding the number it had just computed.

**A tautology cannot discover that a price is wrong, and the price being wrong
is the thing under investigation.** So the rule is now written into `bot.c` as
the section it governs:

> The bot takes *facts* from `world.h` — prices, burn rates, capacities, what
> is reachable — and never a *valuation*. It must not call
> `world_upg_payback` or `world_crew_payback`.

Its estimates are built from what the run has actually produced: prices it has
paid, encounters it has met, which hand would have covered each, and how often
a purchase was refused for want of room. All four counters are new; none of
them existed to be consulted before.

### And immediately, the measurement the audit could only assert

| | P2 | P3 |
|---|---:|---:|
| upgrades bought per run | — | 0.69 |
| **crew hired per run** | — | **0.00** |

With an independent valuation, the bot hires **nobody, ever**, across 400 runs
per difficulty. The static analysis had claimed crew were priced three to eight
times their worth; that was arithmetic. This is a reading, and it is the first
one possible, because until the tautology was removed the answer was fixed at
"buy" regardless of the price. Every crew role is now a dominated option and
P8 has to reprice all five from measured coverage.

### Duplicates removed

Four copies of game logic lived in `bot.c`, none of which any test could have
reported as drifted, because each was only ever read by its own side:

- `upgrade_payback` — dead; nothing called it.
- `crew_payback` — had already diverged, missing the `net < 0` clamp.
- `FUEL_WORTH` / `WATER_WORTH` — hand-synchronised constants.
- a `BASE` price table — **already drifted**: water 13 against the game's 12,
  fuel 22 against 17.
- the `world_reachable` loop, re-implemented inside `decide_map` **without its
  `sector >= SECTORS - 1` guard**, so it would read `node[SECTORS][m]`, one row
  past the end of the array. Unreachable today only because arriving at the
  Green Zone sets `ST_WON` before the map is drawn — a state-machine accident
  rather than a bound.

### Dead weight out of the contest binary

`game_world`, `audio_mood_of` and `game_ui` are harness-only accessors, but
"never called" is not "not present" without link-time garbage collection, and
all three were being carried in the shipped executable. Now behind
`CONVOY_INSTRUMENT`. Verified by comparing object symbol tables rather than by
reading the size, which did not move — they are smaller than the 512-byte
section granularity the size report rounds to:

```
windows object:  game_audio  game_daily  game_init  game_update
harness object:  game_audio  game_daily  game_init  game_update
                 audio_mood_of  game_ui  game_world
```

`game_daily` briefly went out with them and the Windows link failed on an
undefined symbol — which is the linker doing the job `-Werror` did earlier in
the phase, and an argument for building both targets on every change rather
than only the one being worked on.

### Where this leaves the band

67% on THE ROAD is far outside the 40-50% band this log has used since v1. That
band described a game measured by an agent that thirsted itself to death; it
has no standing now and is not something to restore by making the game harder.
It is re-derived in P5, after the bot also learns the pressures a player feels,
and P4, P7 and P8 all move the economy underneath it first.


## P4 — market spread symmetry

One line. `world_sell` moved the local price by `p/8 + 1` per unit while
`world_buy` moved it by `p/16 + 1` — selling walked the price down twice as
fast as buying walked it up, for no stated reason. Now symmetric. The 20%
bid-ask spread, which is the thing that actually prevents a free round trip,
is untouched.

### The arithmetic, before the change (rule 6)

The round-trip margin depends on the **buy** nudge and the spread, not on how
fast selling walks the price down, so symmetry cannot open the exploit. Worth
proving rather than asserting, because the margin is thinner than it looks:

    with the trader aboard the spread is 90%, so a round trip nets
    0.9 * (p + p/16 + 1) - p  >  0   for p < 20.6 in exact arithmetic

Only integer truncation keeps that negative — `p/16` is 0 below 16 — and water
at base 12 and scrap at base 6 both sit inside that window. `-X` sweeps every
good at every price 1..200 with and without the trader, before and after:
best round trip **+0**, at a list price of 1 where the sell clamp floors the
take. Unchanged by this phase, as predicted.

### What it was worth

| | P3 | P4 |
|---|---:|---:|
| realised ÷ headline | 77.7% | 77.8% |
| biggest stack sold at one market | 2.23 | 2.35 |
| units sold per run | 6.0 | **6.5** |
| credits banked per run | 83 | **91** |

| difficulty | P3 | P4 |
|---|---:|---:|
| FORGIVING | 92% | 92% |
| THE ROAD | 67% | 67% |
| UNFORGIVING | 38% | 39% |

n=400, zero stalls. Win rate unchanged within noise (SE 2.5 points).

**The headline ratio barely moved, and that is the finding.** 77.8% is
dominated by the 20% bid-ask spread, not by the decay: at a typical stack of
about two units the decay was only ever worth one or two points of realised
value. The "a ten-unit stack realises 41% of headline" figure that motivated
this phase was arithmetic about a sale nobody makes — the largest stack sold
at any one market is 2.3 units.

What did move is volume: 8% more units sold and 10% more credits banked,
because a market that recovers at the same rate it is depleted is worth
returning to. That is the real effect, and it is a tenth the size of the one
the static analysis predicted.

**Recorded as a fairness fix, not an economy recovery.** It is still right —
an asymmetry with no reason behind it is a trap for anyone who does sell a
large stack, and P5 is about to teach the bot to value hold space, which is
exactly the behaviour that would have walked into it. Doing it before P5 was
the correct order for that reason and not for the reason originally given.

### Note on the control

The frozen agent moved too: 72/46/27 → 71/44/27. Expected and worth stating —
`bot_ref` is a control for *agent* changes, and this phase changes the game.
Its drift here is the size of the game change as seen by a fixed observer, and
it is small, which corroborates the measurement above.


## P5 — the bot learns human pressure

Game frozen; `bot.c` only. Three changes were proposed; one was measured and
deleted, two kept, and a fourth was found while measuring.

| difficulty | frozen `ref` | P4 `v4` | P5 `v4` |
|---|---:|---:|---:|
| FORGIVING | 71% | 92% | **94%** |
| THE ROAD | 44% | 67% | **73%** |
| UNFORGIVING | 27% | 39% | **40%** |

n=400 each, zero stalls, careless play 0% on all three. `ref` unchanged from
P4, confirming the game did not move.

### Attribution, measured one at a time

Three changes that all move the win rate produce one number and no attribution,
so each was run alone over the same 400 seeds before all were enabled.

| arm | THE ROAD | verdict |
|---|---:|---|
| baseline | 67% | — |
| hold pressure | 67% | **byte-identical — deleted** |
| route lookahead | **72%** | kept |
| contract provisioning | 65% | kept, for a reason other than win rate |

**Hold pressure was doubly redundant and was removed.** The plan asked for it
on the premise that the bot never approaches capacity and so never values the
racks. P1 had already disproved the premise — occupancy is 69% mean and 94%
peak; the earlier "30-45%" figure was *end-of-run* cargo, read after selling
down. Implemented anyway to follow the phase description, it then turned out to
be structurally unreachable: speculation already requires six free slots, so
the "is the hold tight" test could never be true. Its arm produced 268 wins
against the baseline's 268, run for run identical. The constraint was already
there; what was missing was the bot *valuing* the space, which P3's
`hold_blocked` counter already does.

**Route lookahead is worth five points.** `decide_map` scored one link ahead;
a player sees the whole route drawn on screen. Scoring the chain two hops out
is not clairvoyance, it is reading what is already displayed.

**Contract provisioning costs two points and is kept anyway.** Nothing in the
bot ever bought toward a job it had accepted — it took the contract and hoped
the goods turned up. Delivery rate **50% → 61%**, at about three credits a run.
A shipped mechanic going from half-working to two-thirds-working is worth more
than a difference inside the noise band.

### The finding that would have corrupted P7

Scrap is the cheapest good in the game at base 6, so the bot reserved none of
it — `keep[G_SCRAP] = 0`, commented "pure trade good" — and sold every unit.
Scrap is also the **repair currency**. A convoy carrying none cannot fix a
breakdown at any price.

Measured with the new per-kind report: **60% of breakdowns and 52% of leaks
were refused because there was nothing to pay with**, not because refusing was
the better deal. Those two are the same keypress and mean opposite things.

Reserving three units — one repair's worth — with no change to the game at all:

| | accept before | accept after | forced before | forced after |
|---|---:|---:|---:|---:|
| BREAK | 3% | **29%** | 60% | 31% |
| LEAK | 17% | **58%** | 52% | 6% |

Had this gone unfixed into P7, the obvious reading of "BREAK is accepted 3% of
the time" is "the deal is bad, make it cheaper" — and the tables would have
been tuned against the observer's shopping habits. **The forced-decline column
paid for itself on its first use.**

It is also a real design tension, not only a bot one: making the cheapest and
most-dumped good the repair currency means repairs are structurally hard to
afford, for a player as much as for a bot. Noted for P7.

### The state of the encounter table, as the brief for P7

n=400. Target: every kind chosen both ways at least 15% of the time.

**7 of 14 are real decisions:** RAID 43/56, SICK 53/47, BREAK 29/70, TOLL
51/48, PLAGUE 29/70, CHECKPOINT 47/52, LEAK 58/41.

**7 of 14 are non-decisions, and every one is "never accepted":**

| kind | accept | costs |
|---|---:|---|
| WRECK | 2% | 1 fuel |
| CACHE | 4% | 1 fuel |
| SIGNAL | 4% | 1 fuel |
| BRIDGE | 5% | 1-2 fuel |
| TRADER | 9% | 2-3 water |
| REFUGEE | 10% | 1-3 water |
| RIVAL | 12% | 2-4 of a random good (48% forced) |

**Every single one charges in fuel or water — the two things that end runs.**
Four of them are the "free money" kinds the static analysis identified as pure
gains; they are refused nineteen times in twenty, because a competent convoy
will not spend survival margin to obtain credits. This is the structural thesis
of P7 confirmed from the engagement side rather than derived from arithmetic,
and it is the argument for letting some kinds pay in water and fuel rather than
only charging in them.

RIVAL is a different fault: 48% forced. It demands 2-4 units of a *random*
good, and a convoy reserves most goods for survival, so the cost frequently
cannot be met at all. That is a table problem, not a shopping problem.

### The band, derived

The 40-50% band this log has used since v1 described a game measured by an
agent that sampled prices by loitering and thirsted itself to death. It has no
standing. The reasoning behind it does: *below about 30% skilled the game reads
as unfair, above about 60% the decisions stop mattering.* That is a statement
about play, not about the observer, and it survives.

Against an honest, competent agent, v4 targets:

| difficulty | band | now |
|---|---|---:|
| FORGIVING | 60-70% | 94% |
| THE ROAD | 42-52% | 73% |
| UNFORGIVING | 22-32% | 40% |

All three are far above. **The game is not too hard, it is much too easy for a
competent player** — the opposite of what three releases of this log reported,
because the reported difficulty was the instrument's handicap.

The retune is *not* done here. P7 changes the encounter tables and P8 reprices
kit and crew — which the bot currently refuses to buy at all, so making them
worth buying will raise these numbers further still. Tuning the difficulty
table now would be tuning twice and measuring once. It is P8b, after the
economy underneath it stops moving.


## P6 — contracts as a real mechanic

P2 stopped the board jamming on an *offered* job. This phase deals with the
second jam and gives refusing a name.

| difficulty | P5 | P6 |
|---|---:|---:|
| FORGIVING | 94% | 95% |
| THE ROAD | 73% | 72% |
| UNFORGIVING | 40% | 40% |

n=400, zero stalls. Unchanged within noise, which is the expected shape: this
phase makes a mechanic work rather than making the run easier.

### The second jam

`by_sector` is an *earliest* delivery point, not a deadline, so a taken job
that never found its cargo simply stayed taken -- and since the board only
posts when it is clear, one such job disabled contracts for the rest of the
run. A taken job now lapses three sectors past the earliest place it could have
been handed over.

Measured, it fires **0.09** times per run: smaller than a 63% delivery rate
suggests, because most undelivered jobs are still in hand when the run ends
rather than sitting on the board blocking it. Worth fixing anyway -- the failure
it prevents is total for the rest of a run, not marginal.

### Refusing is now a thing you can do

`X` on the contracts tab declines. The help screen has advertised
"X SELL / REFUSE" since v1 while three of the five tabs silently ignored it.
The panel says so too: a binding with no prompt is a binding nobody presses.

### Every offer now has a known fate

n=200, THE ROAD:

| fate | per run |
|---|---:|
| offered | 2.54 |
| — accepted | 1.03 |
| — — delivered | 0.65 |
| — — forfeited | 0.09 |
| — declined at the board | 1.50 |
| — lapsed on departure | 0.00 |

Residual: **+0.00**.

The counter was one bucket called `expired` until it was split three ways, and
the split is the point: a job the player refused, one they walked away from,
and one they took and could not deliver mean three different things about the
mechanic and were indistinguishable in a single number. This log has now been
caught by that shape four times -- forced versus chosen declines, offered versus
taken jams, and twice on tools reporting success while doing nothing.

**The decline did not raise throughput**, and that is worth stating plainly:
offers went 2.50 -> 2.54, inside noise, because P2's lapse-on-departure
had already unjammed the board. What moved is `lapsed` -> **0.00**: refusals
are now deliberate rather than accidental. Its value is agency and consistency,
not volume.

Offers per run stand at 2.54 against **1.48** at the v4 epoch — a 72% rise
across P2 and P6 — while the delivery rate is 63% of accepted.

## P7 — encounters: retune, then return survival margin

**14 of 14 encounter kinds are now real decisions**, against 7 at the start of
the phase. The plan's target was that every kind be chosen both ways at least
15% of the time; it is met with no kind outside 25/75.

| difficulty | P6 | P7 |
|---|---:|---:|
| FORGIVING | 95% | 97% |
| THE ROAD | 72% | 82% |
| UNFORGIVING | 40% | 56% |

n=400, zero stalls, `-X` clean, determinism clean.

### The biggest fix was in the observer, not the game

`decide_event` carried a hard veto: any payment in fuel or water that left the
convoy below its reserve was refused **before the deal was priced at all**.
Because the bot provisions exactly to its reserve, that fired on any payment of
one unit -- and six of the fourteen kinds charge in fuel or water.

Removing the veto and letting the existing survival multiplier price the dip
instead, with no change to any table:

| kind | before | after |
|---|---:|---:|
| SIGNAL | 8% | 87% |
| CACHE | 5% | 76% |
| TRADER | 12% | 66% |
| REFUGEE | 11% | 51% |
| BRIDGE | 5% | 43% |

Five kinds went from dead content to genuine decisions **without touching the
game**. Had the phase started by retuning payoffs as planned, it would have
been adjusting numbers on kinds that were being refused categorically whatever
they offered.

**The first attempt at this failed and nearly produced the wrong conclusion.**
Relaxing the veto to a "risk premium" moved the accept rates by two points,
which reads as "the tables really are the problem". The premium was
double-counted: `good_value` already triples fuel and water below reserve, and
the new code multiplied by three again -- twelve times market price for a unit
of fuel, which no payoff in the table can clear.

### Three genuine table faults

**SIGNAL** at 87% accept was a non-decision in the other direction: a unit of
fuel for ninety-odd credits is a formality. Trimmed to 25-50 + depth*4 -> 63/36.

**RIVAL** was 43% *forced*. It named a good at random, and a convoy reserves
most goods for survival, so the commonest outcome was not refusal but
inability -- which reads the same on screen and means the opposite. A rival now
eyes what is actually aboard, and never asks for more than is carried:
22/77 with 43% forced -> 57/42 with 3% forced.

**WRECK** was the one kind that was genuinely a bad trade: a unit of fuel for
three to six scrap, about 17 credits for 27, and 51 for 27 once fuel mattered.

Trying to fix it by making it *more generous* made it worse. Raising the
salvage to 5-9 left acceptance at 11% while forced refusals went from 5% to
21%: a bigger reward needs more free slots than a hold at 69% occupancy has,
and P2's room check refuses it outright. **More generous and less attainable.**

### 7b — the table can now return survival margin

Every encounter charged in the two resources that end runs and paid in credits
worth about 3% of the final score. WRECK is reversed: parts in, fuel out. You
spend scrap stripping the wreck and come away with what is in its tank. It is a
real decision precisely because the answer changes -- scrap is worth more as
trade goods when the convoy is flush and worth nothing against fuel when it is
not. 12% -> 78% accepted.

CACHE can now hold water as well as ammo or meds, for the same reason.

The first version asked 2-3 scrap and was unaffordable 55% of the time, because
breakdowns draw on the same small stock. Reduced to 1-2.

### Two contended currencies

BREAK at 38% forced and PLAGUE at 30% were the same shape as the scrap finding
in P5: the currency is too scarce for the ask. BREAK reduced to 1-2 scrap
(38% -> 11% forced); the bot's medicine reserve raised to 2, since plague asks
for one or two and the convoy starts with one (30% -> 25%).

### Final state, n=400, THE ROAD

| kind | accept | refuse | forced |
|---|---:|---:|---:|
| WRECK | 78% | 21% | 21% |
| CACHE | 78% | 21% | 0% |
| SIGNAL | 63% | 36% | 0% |
| TRADER | 61% | 38% | 6% |
| LEAK | 59% | 40% | 8% |
| RIVAL | 55% | 44% | 4% |
| REFUGEE | 54% | 45% | 4% |
| SICK | 51% | 48% | 4% |
| BREAK | 51% | 48% | 11% |
| TOLL | 48% | 51% | 2% |
| BRIDGE | 47% | 52% | 0% |
| CHECKPOINT | 45% | 54% | 15% |
| RAID | 41% | 58% | 14% |
| PLAGUE | 34% | 65% | 25% |

### A 50x faster harness, and what it proves

A balance sweep never looks at a pixel, but the core drew a full 640x480 frame
twice per step regardless -- about 25 billion pixel writes for a 400-seed arm,
which was nearly all the wall clock. `-Q` shrinks the *logical* framebuffer
while keeping the full allocation, so every primitive clips almost everything
away and nothing can write out of bounds. It is only sound because no game
logic reads the framebuffer dimensions: `game.c` touches `fb->w` exactly once,
in a draw call.

A six-arm phase gate went from about fifteen minutes to **sixteen seconds**.

The acceptance test is that `-Q` produces byte-identical BOT lines to a
full-size run, which it does -- and that is worth having for its own sake: it
proves the render path cannot influence a balance number.

## P8 — kit and crew, derived from readings

| difficulty | P7 | P8 |
|---|---:|---:|
| FORGIVING | 97% | 97% |
| THE ROAD | 82% | 80% |
| UNFORGIVING | 56% | 56% |

n=400, zero stalls, `-X` and `-Z` clean.

### The rate was wrong by an order of magnitude

`world_crew_payback` used `hops * 3 / 5` fires per role, from a comment stating
encounters were 30% of nodes "across five kinds". There are **fourteen**. At 13
hops the formula claims 7.8 fires per role; measured over 400 runs:

| role | formula | measured | overstated by |
|---|---:|---:|---:|
| MECHANIC | 7.8 | 0.81 | 9.6x |
| GUARD | 7.8 | 0.79 | 9.9x |
| MEDIC | 7.8 | 0.80 | 9.8x |
| SCOUT | 7.8 | 0.44 | 17.7x |

Crew are now priced against the road actually ahead, using `world_road_ahead`,
which already existed and was already counting the storms and encounters left.

### But the pricing was never the binding constraint

With the rate corrected, every role was still **net-negative before its fee**:
23 to 38 credits of coverage against **84 credits of water** over a run. No
price could fix that — the floor is 10, so even free crew lost money. An honest
bot hiring nobody in 400 runs per difficulty was the correct answer to the
question as posed.

The ration is the finding. Crew now drink every third day rather than every
second: `keep` falls from 84 to 56, which is the smallest change that makes the
trade defensible rather than arithmetically impossible. They remain mouths that
drink; they are no longer mouths that cost more than they can ever save.

### Two bugs found by writing the arithmetic down

**Integer truncation zeroed the whole calculation.** Written as
`events * 45 / 100 * 3 / 14`, a rate of about 0.8 fires per role rounds to
**zero** mid-expression, so every role priced at the floor regardless of the
road ahead. Computed in one expression instead.

**The bot kept its own copy of the ration schedule.** `crew_value` tested
`day % 2` directly; when the game moved to `day % 3` the bot went on costing
hires against a burn rate that no longer existed. Exported
`world_crew_drinks_on` — a fact, which the P3 invariant permits the bot to read
— and deleted the copy. Hiring went 4% of runs to **12%** on that fix alone.

### Two documented falsehoods removed

**The medic's water saving was counted twice.** `world_water_burn_on` cancels
exactly the medic's own thirst and nobody else's, and `world_crew_payback` then
halved its keep again for "runs the water discipline too".

**Fitting water tanks made every hand more expensive for nothing.** Crew keep
was halved whenever tanks were fitted, but tanks zero the burn on *even* days
and crew ration on *odd* ones. The synergy priced there does not exist.

### Where it landed

Runs hiring at least one hand: **0% → 12%**. Short of the 15% bar
`DESIGN-kit.md` set, and recorded as such rather than tuned to hit it: the
remaining gap is that the bot's own valuation needs to have *seen* the trouble
a role covers before it will pay for it, and crew are offered from sector 5 on,
by which point a run has met one or two encounters. That is defensible
behaviour, not a fault, and the number is honest.

## P8b — the difficulty table, retuned against a settled economy

| difficulty | band | P8 | P8b |
|---|---|---:|---:|
| FORGIVING | 60-70% | 97% | **70%** |
| THE ROAD | 42-52% | 80% | **43%** |
| UNFORGIVING | 22-32% | 56% | **27%** |

n=400, zero stalls, careless play 0% on all three.

Held until last on purpose: P7 changed the encounter tables and P8 the crew
economy, and retuning difficulty before those settled would have been tuning
twice and measuring once.

| | cr | water | fuel | fuel scale | spoil | storm | settle |
|---|---:|---:|---:|---:|---:|---:|---:|
| FORGIVING | 140 | 9 | 6 | 1 | 28 | 13 | 47 |
| THE ROAD | 130 | 8 | 5 | 2 | 35 | 15 | 44 |
| UNFORGIVING | 120 | 7 | 5 | 3 | 42 | 20 | 40 |

**Settlement density is no longer the strongest lever.** P1 measured it as the
single biggest one; sweeping it alone now moves THE ROAD only from 80% to 73%
across 48 down to 32. Starting stock and the fuel price curve do most of the
work, because a competent agent that provisions correctly is limited by what it
can carry and afford rather than by how often it can stop.

### A design property that turned out to be an artifact

The table's comment claimed each setting leaned on a different failure: easy
and normal thirst-led, hard fuel-led. That was measured and true in v3.

It is not true now, and cannot be made true by skewing the starting stock. With
water tight and fuel plentiful, or the reverse, or the fuel curve doubled, the
death mix stays within a few points of 50/50 on every setting — only the win
rate moves.

The old asymmetry was substantially an artifact of the bot's water reserve,
which sampled a single day's parity and collapsed to two units when the parity
was wrong (see P3). Against an agent that provisions both resources honestly,
the two failures balance.

**Recorded rather than recreated.** Contorting the table to reproduce a split
that existed because the observer was broken would be tuning to an artifact —
the exact failure this release has spent nine phases removing.

### Process

A probe script edited the table with `sed`, produced a trailing-comma syntax
error, built into `/dev/null`, and reported **73% for five very different
configurations** — every one of them measuring the previous binary. Caught
because five identical numbers from five different inputs is not a result.
Rewritten with `set -e`, an unsilenced build, and an assertion that the edit
changed the file. **Fourth instance this release of a tool reporting success
while doing nothing.**

## P9 — presentation

Win rates 70 / 43 / 27, unchanged from P8b at n=400. A presentation phase that
moves the win rate has leaked behaviour, so that equality is the gate.

### The payload is drawn

It was rendered **nowhere**. `T_PAYLOAD` and `T_PAYLOAD_SAFE` were defined and
referenced by nothing; `world_payload()` was called only by the harness. A
player could cross all fourteen sectors and reach the Green Zone having never
seen the thing the entire run is about.

Worse, the two numbers that did exist disagreed by exactly that amount: the HUD
counts `world_cargo`, which **includes** the payload, while the cargo grid
iterated `held[]`, which does not. The gap between them *was* the six crates.

Six green cells now head the hold, labelled once. HOLD 25/30 and the grid now
agree.

The grid also sizes to `world_cargo_cap()` rather than the `CARGO_CAP`
constant. With racks fitted it drew thirty cells for a forty-slot hold, so ten
slots of paid-for cargo were invisible.

### A demand for the seed no longer looks like scrap

`ui_event` tested `lose_good >= 0` and let `-2` — the payload — fall into the
generic random-cargo branch. The highest-stakes decision in the game, where a
crate is 500 score against a win's 1000, was drawn identically to losing three
units of junk. It now renders as crates, in the payload's own colour.

### Things the screen said that were not true

- **"ESC TO SKIP"** on every cut-scene panel. ESC quits the game; it has never
  skipped anything. The opening told players to press the one key that ends the
  run. Now "ANY KEY TO CONTINUE", which is what actually happens.
- **"YOU CANNOT PAY THIS"** appeared when the real reason was a full hold.
  `world_can_accept` refuses for two unrelated reasons and said the same thing
  for both, so a convoy with a full purse was told it was broke. Split via
  `world_accept_block`.
- **The water burn was for the wrong day**, and hidden entirely from solo
  drivers. It showed `world_water_burn(w)` — the day already paid — while the
  hop about to be taken charges for `day + 1`; with crew aboard or tanks fitted
  those differ half the time. And the whole readout only appeared with crew, so
  a lone driver was never told water is spent per day, while thirst is one of
  the two things that end a run.
- **A free encounter drew as "− icon × 0"**, which reads as a cost of nothing
  rather than as no cost — and it is the only on-screen evidence that a hire is
  earning its keep. Now "FREE".
- **Replaying a daily run gave a different map.** `restart` took a fresh seed
  regardless of daily mode.

### Collisions

The garage printed "FITTED TUNED ENGINE" straight over "SHOULD RETURN 60": the
owned list runs from `y+86` at a 15px pitch while the payback and road-ahead
lines were fixed at `y+92` and `y+118`. `draw_outfit` now returns where its
list ended and the extras draw beneath it.

Found by looking, not by reading coordinates — the `-S <tab>` flag added in P6
exists because hunting the right frame by hand does not scale to a whole
presentation pass.

### Every string is now drawn

`T_MARKET` and `T_END_AGAIN` were exact duplicates of `T_TAB_MARKET` and
`T_AGAIN` that nothing referenced; removed. `T_CHEAP_HERE` and `T_DEAR_HERE`
were the legend for the price-trend arrow — the signal the entire trade route
is built from, drawn bare with no explanation anywhere outside the help screen.
They are now shown on the line that already describes the selected good.

A grep for unreferenced `T_*` symbols returns nothing.

## P10 — documentation, and the shipped state of v4

### Final gate

| difficulty | band | v4 final (n=1000) |
|---|---|---:|
| FORGIVING | 60-70% | **69%** |
| THE ROAD | 42-52% | **43%** |
| UNFORGIVING | 22-32% | **25%** |

Zero stalls. Careless play 0%. Sanitizers clean, determinism clean over 200
seeds on each difficulty, market-exploit probe clean across every good at every
price with and without the trader aboard.

**110,592 bytes — 7.50% of the floppy.**

### What the documents claimed

`convoy/README.md` described **Version 1**: "five encounter types" against
fourteen, "five fuel and nine hops" against six and thirteen, a seven-file
layout for a twenty-two file tree, six of twenty harness flags, and **no row
for the left and right keys** — the ones that reach the garage, the crew board,
the contracts and the journal. A player following that table could not open
four of the game's five screens.

It also carried the v3 difficulty figures as though they were current. They are
now published with the caveat that matters: they are not comparable to anything
before v4, because the generator was re-seeded into three streams *and* the
agent that produced them was measuring its own handicap.

`DESIGN-kit.md` prescribed two values its own log records as wrong — a price at
3/4 of payback where the shipped figure is 45%, and salvage at 45% where
TESTLOG explicitly records that value as discredited. A correction block was
added rather than editing the body: the document's worth is the record of a
model being tested, and quietly fixing its numbers would destroy exactly that.
The block also notes that the payback model underneath it was wrong by an order
of magnitude until P8 — the *method* was sound, the rate it was fed was not.

`NOTES.md` opened with "make it language-free — the strongest version ships no
alphabetic font at all", and reversed that sixty lines later without a marker.
A reader working top to bottom would have acted on the retracted advice. It now
carries a superseded note pointing forward.

`tools/mkpage.py` hard-coded the byte count, so the published page misreported
the size after every build that changed it — which is every build worth
publishing. It measures the binary now.

### New entries in NOTES

Five lessons that generalise past this game: split the generator before tuning
anything; a price derived from a payback makes every test of it a tautology;
freeze a reference agent before improving the current one; a win-rate sweep
cannot resolve a rare mechanic; and four tools reporting success while doing
nothing, with the rules that came out of it — identical is a red flag, never
silence a build, never pipe a check whose exit status you depend on, and when a
guard has been defeated twice, move it somewhere it cannot be bypassed.

## P11 — the bot optimised, and kit priced from A/Bs

Final v4 gate, n=1000: **61 / 47 / 27**, inside bands of 60-70 / 42-52 / 22-32.
Zero stalls, careless 0%, sanitizers/determinism/exploit clean.

### Forced-policy A/Bs: what each option is actually worth

Every fitting and hand granted free at the start, n=600 on THE ROAD against a
44% baseline. This is the measurement the whole release was building toward,
and it could not have been made before P3 removed the tautology.

| option | win rate | delta |
|---|---:|---:|
| kit ECON | 86% | **+42** |
| kit TANKS | 80% | **+36** |
| kit HOLD | 50% | +6 |
| kit ARMOUR | 49% | +5 |
| crew MEDIC | 48% | +4 |
| crew TRADER | 35% | −9 |
| crew SCOUT | 25% | −19 |
| crew MECHANIC | 24% | −20 |
| crew GUARD | 21% | **−23** |

### The bot was leaving 12 points on the table

`upgrade_worth_buying` demanded **120 credits of working capital left over**
after a purchase, on a convoy that typically holds 100-150. It blocked almost
every fitting -- including the economiser, worth +42. The gate dated from when
kit was overpriced and capital compounded faster than any fitting returned;
neither had been true for several phases.

Swept: 120 → 43%, 80 → 49%, 50 → 53%, 30 → **55%**, 15 → 55%. Set to 30.

### Kit was priced by instinct, and two of four were backwards

`world_upg_payback` still carried the original guesses, including armour at
`hops * 3/5 * 20` -- the same discredited five-kinds rate that broke the crew
pricing in P8. It made armour the **dearest** fitting in the game while the
A/B puts it at the **least valuable**. Repriced against the measured deltas.

Two valuation bugs in the bot, both found by a take rate stuck at zero:

- `UPG_HOLD` was valued from buys refused for want of room. That counter can
  never fire: the bot only speculates when six slots are already free, so the
  branch is unreachable and the racks were worth zero forever -- 0 fitted from
  552 offers. Valued from measured occupancy instead.
- `UPG_ARMOUR` divided before multiplying, rounding a 0.8-per-run rate to
  zero. **Exactly the fault fixed in the crew payback one phase earlier**,
  reproduced days later in the same shape.

Result -- every fitting is now a live choice, meeting the `DESIGN-kit.md` bar
of at least 15% taken and no more than 85%:

| | offered | taken |
|---|---:|---:|
| HOLD | 615 | 42% |
| ECON | 803 | 57% |
| ARMOUR | 530 | 65% |
| TANKS | 858 | 34% |

### Crew are the one system v4 could not fix

Every role is net-negative **even when granted free**, and the reason is
structural rather than a matter of price:

| ration | free GUARD | free SCOUT |
|---|---:|---:|
| every 3 days | −22 | −21 |
| every 6 days | −13 | −8 |
| never drinks | **+5** | **+9** |

At zero water cost a specialist is worth +5 to +9, because its ability fires
**0.8 times per run** -- three of fourteen kinds at 3.7 encounters. Any ration
that preserves "mouths that drink" outweighs that by three to five times.

The role that came closest to viable is the trader, and the reason points at
the fix: its benefit is **always on**. The scout is the sharpest illustration
of the opposite -- it negates storms entirely, which sounds strong and measures
at −19, because a competent convoy already routes around storms. It guards
against something good play avoids.

**Left as measured rather than papered over.** Crew are a presence problem, not
a pricing problem, and the fix is content: passives, a third branch in
encounters, personal errands, and a voice. That is v5.

### AddressSanitizer caught a stray write on its first run

The per-role counter added this phase indexed `crew_offered[w->offer_crew]`
outside the branch that sets it. `offer_crew` is `0xFF` when nobody is looking
for work, so a five-element array was being written at index 255.

---

## Bugs found, and what found them

| bug | symptom | found by |
|---|---|---|
| Provisioning deadlock | 0% win, bot pressed BUY forever | win rate collapse + STALLED |
| Market round-trip exploit | 4,141 credits at sector 0 | bot doing what no human would |
| Buy/sell threshold overlap | oscillated one good forever | STALLED with a step budget |
| `roll_offers` stack overflow | segfault, no line number | **AddressSanitizer** — `tools/asan.sh` |
| Unaffordable kit | no crash, no bad number | inspecting *what the bot spent* |

The last row is the one to remember. Four of these announced themselves. The
unaffordable garage did not: the win rate was a healthy 48% and every sweep was
green. It surfaced only from asking a different question — not "did it win?" but
"what did it actually do?"
