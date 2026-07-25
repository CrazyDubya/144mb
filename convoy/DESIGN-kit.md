# Phase 2A — rebuilding kit as a decision

## Why the current design cannot be tuned into working

Kit loses to working capital, and it loses structurally rather than by a
margin. Credits in the hold **compound**: buy low, sell high, repeat, and over
six legs 100 credits becomes roughly 300. A fitting is a **flat** one-time
saving. Compounding beats flat whenever there is road left to compound over.

That produces a squeeze with no middle:

| when | price | payback | verdict |
|---|---|---|---|
| early | high | high | worth it, but unaffordable — capital is needed for trade |
| late | low | low | affordable, but there is no run left to repay it |

Measured, at the two ends of the float:

| bot float | purchases / 30 runs | win rate |
|---|---:|---:|
| 170 credits (buys almost never) | 2 | **49%** |
| 90 credits (buys readily) | 15 | **39%** |

Buying makes the bot worse. The only correct play is to ignore the garage, so
the garage is not a decision — it is a dominated option the game politely
offers. No price fixes that, because the problem is the *shape* of the payoff,
not its size.

## What it has to become

Two failure modes to design against, both about timing:

1. **A lever nobody needs.** If a problem is already solved — you are rich,
   fuel is handled — the fitting that solves it is worthless however cheap.
2. **A lever revealed too late.** An engine offered at sector 12 cannot repay
   itself in one hop, so dangling it is noise.

And two properties to add: a real gamble, and forward-looking depth.

## The design

### 1. Value-linked pricing

Price a fitting at a fixed fraction of what it can **still** return, computed
from hops remaining, rather than from a base price with a sector multiplier
bolted on.

    price = payback(fitting, hops_remaining) * 3 / 4

This makes every offer a positive-expectation purchase on its face, and makes
the price fall naturally as the run shortens — without ever drifting into
"cheap but pointless", because when payback goes to zero, so does the price
and the offer stops appearing at all.

### 2. Need-linked offers

A settlement stocks what the convoy actually lacks. Weight the roll by current
shortfall: a refinery offers the tuned engine when fuel is thin, a well offers
condensers when water is short, an armoury offers plate after a mauling.

This kills failure mode 1 directly — you are never sold the answer to a
question you have already answered.

### 3. Guaranteed early window

Force at least one garage offer within the first three sectors. Kit needs to
appear while there is still route for it to pay back over. After hops < 4,
stop offering fittings entirely rather than showing an option that cannot
mathematically repay itself.

This kills failure mode 2.

### 4. Salvaged or sound — the gamble

Every fitting is offered in two conditions:

| condition | price | risk |
|---|---|---|
| **sound** | full value-linked price | works for the rest of the run |
| **salvaged** | ~45% of it | 1-in-3 chance it fails at a random hop and is gone |

Salvaged plate for a third of the price is a genuine bet: usually a bargain,
sometimes a hole in your budget with nothing to show. It also gives the poor
convoy — the one that most needs help — a way in, which is precisely when a
gamble is rational.

Failure is announced when it happens, so the risk is felt rather than
silently accounted.

### 5. Forward-looking information

The route ahead is already drawn on the map and already known to the
simulation. Surface it in the garage: **how many storms and how many
encounters remain on the road east**. Then plate armour is not a coin flip, it
is a response to four raid markers between here and the Green Zone.

This is what makes the purchase strategic rather than merely random, and it
costs nothing to compute — the data is sitting in `w->node[][]`.

## What success looks like

Not "the bot buys more". The target is that buying and not buying are both
defensible:

- Bot win rate stays in the **40-50%** band whether or not it engages.
- Purchases happen in roughly **a third to a half of runs**, not 2 in 30 and
  not every stop.
- Salvaged kit fails often enough to be felt (~1 in 3) and pays off often
  enough to be worth taking.
- No offer ever appears that cannot repay itself.

If buying still loses runs after this, the honest answer is to cut the system
rather than keep tuning it — a game is better without a mechanic than with one
that punishes engagement.


---

## Outcome

Shipped and measured. Kit is bought in **39 of 40 runs** (was 2 of 30) and the
skilled win rate is **51% over 250 seeds**, against 49% for ignoring the garage
entirely. Engaging and not engaging are both defensible, which was the target.

The price multiplier had to be found empirically rather than derived: the
compounding model said a third of payback, but measurement put parity at 45%.
The model was directionally right and quantitatively wrong, which is the usual
outcome and the reason the sweep exists.
