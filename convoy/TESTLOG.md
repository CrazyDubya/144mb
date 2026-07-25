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
