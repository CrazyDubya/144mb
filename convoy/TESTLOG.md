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
