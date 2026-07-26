// convoy -- a price-aware test bot.
//
// This is NOT part of the game. It is compiled only into the headless harness,
// so it contributes zero bytes to the submitted executable.
//
// It exists because scripted key sequences can only measure the floor: a fixed
// string of presses cannot look at a price and decide. This bot plays through
// the same UI a human uses -- moving the cursor, pressing the same keys -- and
// makes decisions from the world state, which is what makes it possible to ask
// whether the game rewards playing well.
#include "bot.h"
#include "state.h"

#include <string.h>

#define SECTORS_LAST (SECTORS - 1)

// How far ahead the convoy provisions. Stocking for the entire remaining route
// is not a strategy, it is a deadlock: on a 13-hop route that is 29 units of
// fuel and water against a 30-slot hold, leaving no room to trade and no way to
// pay for anything. You provision to the next few markets and re-supply.
#define PLAN_AHEAD 5

// How much of each good the bot refuses to sell.
static void reserves(const World *w, int *keep) {
    int hops = SECTORS_LAST - w->sector;
    if (hops < 0) hops = 0;
    int span = hops < PLAN_AHEAD ? hops : PLAN_AHEAD;

    keep[G_FUEL]  = span + 1;      // +1 for a storm eating one
    // Water is per day, not per hop, and every hand aboard drinks. Missing
    // this is how a bot cheerfully hires five people and dies of thirst.
    //
    // Summed over the days ahead rather than today's rate multiplied out. The
    // burn alternates with the parity of the day -- crew drink on odd days,
    // water tanks give a dry day on even ones -- so sampling one day and
    // scaling it made the reserve swing between two very different numbers
    // depending on which day the convoy happened to arrive. With tanks fitted
    // on an even day it collapsed to 2.
    int water = 2;
    for (int i = 1; i <= span; ++i) water += world_water_burn_on(w, w->day + i);
    keep[G_WATER] = water;
    keep[G_AMMO]  = 2;             // enough to refuse one raid
    // Two, not one. Plague asks for one or two and the convoy starts with a
    // single unit, so 30% of outbreaks could not be treated at any price --
    // the same shape as the scrap reserve, in a dearer currency.
    keep[G_MEDS]  = 2;
    // Scrap is the cheapest good in the game and reads as pure trade stock,
    // which is how it came to be reserved at zero -- the bot sold every unit.
    // But it is also the repair currency, and a convoy with none cannot fix a
    // breakdown at any price. Measured: 60% of breakdowns and 52% of leaks
    // were refused because there was nothing to pay with, not because refusing
    // was the better deal. Those two are indistinguishable in a decline count,
    // and until they are separated the encounter tables cannot be tuned.
    keep[G_SCRAP] = 3;             // one repair's worth
}

// What a taken job still needs, so the convoy buys toward it instead of
// arriving at the delivery empty-handed. reserves() deliberately does not know
// about contracts -- it is about surviving -- so this is separate.
static int contract_short(const World *w, int g) {
    const Contract *j = &w->job;
    if (j->state != CONTRACT_TAKEN || j->good != g) return 0;
    int short_by = j->qty - w->held[g];
    return short_by > 0 ? short_by : 0;
}

static int avg_price(const Bot *b, int g) {
    if (b->seen[g] == 0) return 0;
    return (int)(b->sum[g] / b->seen[g]);
}

static void observe(Bot *b, const World *w) {
    const Node *nd = &w->node[w->sector][w->index];
    if (nd->type != NODE_SETTLE) return;
    for (int g = 0; g < GOODS_COUNT; ++g) {
        b->sum[g] += nd->price[g];
        b->seen[g]++;
    }
}

// Moves the cursor toward `want`, or performs `act` once it is there.
static int step_to(int sel, int want, int act) {
    if (sel > want) return BTN_UP;
    if (sel < want) return BTN_DOWN;
    return act;
}

// A job is worth taking if the cargo will fit and the reward beats what the
// same slots would earn carrying anything else.
static int contract_worth_taking(const World *w) {
    const Contract *j = &w->job;
    if (j->state != CONTRACT_OFFERED) return 0;
    if (world_cargo(w) + j->qty > world_cargo_cap(w) - 4) return 0;
    if (j->by_sector > SECTORS - 2) return 0;
    return 1;
}

// ...and worth turning down otherwise. Declining used to be indistinguishable
// from ignoring, so an offer the convoy could not carry sat on the board until
// it drove away. Refusing it there and then lets the same settlement's board
// clear and the next town post something it *can* carry.
static int contract_worth_declining(const World *w) {
    const Contract *j = &w->job;
    if (j->state != CONTRACT_OFFERED) return 0;
    return !contract_worth_taking(w);
}

// ------------------------------------------------- what a fitting is worth
//
// THE RULE THIS SECTION EXISTS TO ENFORCE: the bot takes *facts* from world.h
// -- prices, burn rates, capacities, what is reachable -- and never a
// *valuation*. It must not call world_upg_payback or world_crew_payback.
//
// Not a style preference. world_upg_price is defined as a fixed percentage of
// world_upg_payback, so a test of the form `payback(x) > price(x)` reduces to
// `p > 0.45p`, which is true for every positive p. That was literally the
// hiring test: it passed for any crew member at any price, whether the payback
// figure behind it was right or wrong by a factor of eight. A tautology cannot
// discover that a price is wrong, and the price being wrong is the thing under
// investigation.
//
// So these estimates are built from what this convoy has actually seen: the
// prices it has paid, the encounters it has met, the times it ran out of room.
// They can disagree with the asking price, and the disagreement is the
// measurement.

// A unit of a good, valued at the prices this convoy has actually seen. The
// table that used to be here had drifted from the game's own base prices --
// water 13 against 12, fuel 22 against 17 -- and nothing would ever have
// reported that, because both copies were only ever read by their own side.
static int unit_value(const Bot *b, const World *w, int g) {
    int a = avg_price(b, g);
    if (a > 0) return a;
    return w->node[w->sector][w->index].price[g];   // first stop: what is in front of us
}

// Which hand would have helped with which trouble. This is the bot's own
// belief about the crew, formed the way a player forms it -- by meeting the
// encounter and noticing who would have covered it -- and deliberately not a
// copy of the coverage logic inside roll_event.
static int role_for_event(int kind) {
    switch (kind) {
    case EV_BREAK: case EV_LEAK: case EV_WRECK:      return CREW_MECHANIC;
    case EV_RAID:  case EV_TOLL: case EV_CHECKPOINT: return CREW_GUARD;
    case EV_SICK:  case EV_PLAGUE: case EV_REFUGEE:  return CREW_MEDIC;
    case EV_BRIDGE: case EV_CACHE:                   return CREW_SCOUT;
    case EV_TRADER: case EV_SIGNAL: case EV_RIVAL:   return CREW_TRADER;
    default:                                         return -1;
    }
}

// Records an encounter as it is faced, so later purchases can be judged
// against what this run has actually thrown at the convoy rather than against
// an assumed rate. The old constants assumed five encounter kinds firing 0.8
// times each; there are fourteen.
static void note_event(Bot *b, const World *w) {
    const Event *e = &w->event;
    int cost = 0;
    if (e->pay_good >= 0)  cost += e->pay_qty  * unit_value(b, w, e->pay_good);
    if (e->lose_good >= 0) cost += e->lose_qty * unit_value(b, w, e->lose_good);
    b->enc_seen++;
    b->enc_cost += cost;
    int role = role_for_event(e->kind);
    if (role >= 0) b->role_seen[role]++;
}

static int upg_value(const Bot *b, const World *w, int u, int hops) {
    int water = unit_value(b, w, G_WATER);
    int fuel  = unit_value(b, w, G_FUEL);
    switch (u) {
    // A free hop every second day, and a dry day every second day. Both are
    // arithmetic on prices the convoy has paid.
    case UPG_ECON:   return (hops / 2) * fuel;
    case UPG_TANKS:  return (hops / 2) * water;
    // Ten more slots are worth what the convoy would have done with them.
    // Counted, not assumed: every time a buy was refused for want of room is
    // one unit it wanted and could not carry, and the margin on a unit is
    // roughly the gap between buying and selling it elsewhere.
    case UPG_HOLD: {
        // Valued from how full the hold actually runs, not from buys that were
        // refused for want of room. The refusal counter never fires: the bot
        // only speculates when six slots are already free, so the "no room"
        // branch is unreachable and the racks priced at zero forever -- 0 fitted
        // out of 552 offers, against a forced-policy value of +6 points.
        int cap  = world_cargo_cap(w);
        int full = world_cargo(w) * 100 / (cap ? cap : 1);
        if (full < 50) return 0;                       // room to spare; no value
        int typical = (unit_value(b, w, G_WATER) + unit_value(b, w, G_FUEL)) / 2;
        // Above half full, each of the ten new slots is worth a share of a
        // typical unit's margin, scaled by how tight things already are.
        return 10 * typical * (full - 50) / 100 / 3;
    }
    // Armour reduces what refusing a raid costs. Valued from the raids this
    // run has actually met and what they have actually cost.
    case UPG_ARMOUR: {
        // Priced off the road ahead rather than only off raids already met.
        // Keyed to role_seen alone it was worth nothing until a raid had
        // happened, and armour is offered early -- 13 fitted out of 1444
        // offers, against a forced-policy value of +5 points. Raids are three
        // of fourteen kinds, and world_road_ahead is a fact the bot may read.
        int storms = 0, events = 0;
        world_road_ahead(w, &storms, &events);
        int per = b->enc_seen ? b->enc_cost / b->enc_seen
                              : unit_value(b, w, G_AMMO) * 2;
        // One expression: raids come to about 0.8 over a run and dividing
        // before multiplying rounds that to zero. Exactly the fault found in
        // the crew payback in P8, reproduced here days later.
        return events * 45 * 3 * per / (100 * 14 * 2);
    }
    default: return 0;
    }
}

// A hand's worth: the trouble it covers, minus what it drinks. Both sides
// measured -- the coverage from this run's encounters, the keep from the
// game's own burn function at the prices the convoy is paying for water.
static int crew_value(const Bot *b, const World *w, int k, int hops) {
    int water = unit_value(b, w, G_WATER);

    // Keep: what one more mouth adds to the burn over the hops remaining.
    // world_water_burn_on is a fact, not a valuation, so asking it is fine.
    // Asked, not assumed. This loop used to test `day % 2` directly -- a copy
    // of the ration schedule that went stale the moment the schedule changed,
    // leaving the bot pricing hires against a burn rate the game no longer
    // used. The rule lives in world.c; the bot reads it.
    int keep = 0;
    for (int i = 1; i <= hops; ++i)
        if (world_crew_drinks_on(w, w->day + i)) keep += water;

    if (k == CREW_TRADER) {
        // The only one whose benefit is not tied to encounters: it takes a
        // tenth off the spread on every sale for the rest of the run.
        int sales = hops * 3;                       // a few units a stop
        int typical = (unit_value(b, w, G_WATER) + unit_value(b, w, G_FUEL)) / 2;
        return sales * typical / 10 - keep;
    }

    if (b->enc_seen == 0) return -keep;             // nothing seen yet: assume nothing
    int avg_cost = b->enc_cost / b->enc_seen;

    // A hand aboard offers a way through EVERY encounter, not only the three
    // kinds they specialise in. Valuing them at `role_seen` coverage was right
    // when their ability was a silent modifier on 3 of 14 kinds; it now
    // undercounts them by roughly the ratio the third branch changed --
    // measured, a free hand is worth +10 to +17 points and the old model
    // priced that at a fifth of it, so the convoy hired nobody.
    // How much road is left to meet trouble on. Extrapolating from encounters
    // already seen undercounts badly early -- one encounter three hops in
    // predicts one more over the next eight, when the map plainly shows more
    // than that. world_road_ahead is a fact about the route the player can see
    // drawn on screen, so the bot may read it.
    int storms = 0, events = 0;
    world_road_ahead(w, &storms, &events);
    int enc_left = events * 45 / 100;
    int seen_rate = b->enc_seen * hops / (b->hops_done + 1);
    if (seen_rate > enc_left) enc_left = seen_rate;
    int covered  = b->role_seen[k] * hops / (b->hops_done + 1);

    // Roughly a third of an encounter's cost is saved by taking the manoeuvre
    // instead, and the specialist's own kinds are worth more than that again.
    int v = (enc_left * avg_cost / 3) + (covered * avg_cost / 3) - keep;

    // A floor, from the forced-policy A/Bs rather than from this model.
    //
    // Granting any single hand free is worth +10 to +17 points of win rate,
    // which makes a hand among the most valuable things the convoy can buy --
    // and the estimate above, built from encounters already met, kept coming
    // out near the 10-credit price floor and declining. When a measurement and
    // a model disagree by that margin the measurement wins: a hand carried for
    // the rest of the route is worth about eight credits a hop, and the model
    // is left in place to argue for MORE than that where it can.
    int floor_v = hops * 8;
    return v > floor_v ? v : floor_v;
}

// Credits in the hold compound -- buy low, sell high, repeat -- so over the
// legs that remain, working capital roughly triples. A fitting has to beat
// that, not merely beat zero, which in practice means kit is only ever correct
// out of genuine surplus.
static int upgrade_worth_buying(const Bot *b, const World *w) {
    int u = w->offer_upg;
    if (u >= UPG_COUNT || w->upgrade[u]) return 0;
    int hops = SECTORS_LAST - w->sector;
    if (hops < 5) return 0;

    int price = world_upg_price(w, u, w->offer_salvaged);
    // Working capital to leave after a purchase. This was 120 for sound kit on
    // a convoy that typically holds 100-150 credits, so it blocked nearly every
    // fitting -- while forced-policy A/Bs put the economiser at +42 points and
    // the water tanks at +36. The gate was written when kit was overpriced and
    // capital compounded faster than any fitting returned; neither is true now.
    // Swept: 120 -> 43%, 80 -> 49%, 50 -> 53%, 30 -> 55%, 15 -> 55%.
    int float_needed = w->offer_salvaged ? 18 : 30;
    if (w->credits - price < float_needed) return 0;

    // Salvaged kit may fail partway, so it is worth less than sound kit by
    // roughly the share of the run it is expected to survive.
    int value = upg_value(b, w, u, hops);
    if (w->offer_salvaged) value = value * 4 / 5;
    return value > price;
}

static int crew_worth_hiring(const Bot *b, const World *w) {
    int k = w->offer_crew;
    if (k >= CREW_COUNT || w->crew[k]) return 0;
    int hops = SECTORS_LAST - w->sector;
    // Match the board. The game offers hands while three hops remain; the bot
    // refused anything past hops<5, so every late offer was declined before it
    // was priced -- two gates on the same question with different answers,
    // which is the fault this project keeps rediscovering.
    if (hops < 3) return 0;
    int price = world_crew_price(w, k);
    // Working capital to leave after a hire. This was 100 on a convoy that
    // typically holds 100-150 credits, so it refused almost every hand -- the
    // identical fault the kit gate had at 120, found the same way, by an
    // option that was offered constantly and never taken.
    if (w->credits - price < 30) return 0;
    if (w->held[G_WATER] < 5) return 0;          // cannot feed them yet
    return crew_value(b, w, k, hops) > price;
}

// ---------------------------------------------------------------- trade
static int decide_trade(Bot *b, const World *w, int sel) {
    const Node *nd = &w->node[w->sector][w->index];
    int keep[GOODS_COUNT];
    reserves(w, keep);

    int cargo = world_cargo(w);
    int hops  = SECTORS_LAST - w->sector;

    // 1. Sell surplus into a market that is paying above what we have seen
    //    elsewhere. Never sell below the survival reserve.
    int local_spec = world_arch_good(nd->archetype);

    for (int g = 0; g < GOODS_COUNT; ++g) {
        int surplus = w->held[g] - keep[g];
        if (surplus <= 0) continue;

        // Cargo under contract is not surplus, whatever the price says.
        surplus -= world_committed(w, g);
        surplus -= world_errand_committed(w, g);
        if (surplus <= 0) continue;

        // Never sell a thing where it is made. They have plenty, they pay
        // badly, and selling into the same stall that just sold it to you is
        // how the bot ended up oscillating a market forever.
        if (g == local_spec) continue;
        if (b->bought_here[g]) continue;   // no round-tripping our own purchase

        // Judge a sale on what the stall actually pays, not on what it asks.
        int pays = world_sell_price(w, g);
        int good_price;

        if (b->seen[g] >= 2) {
            int avg = avg_price(b, g);
            good_price = pays * 100 >= avg * 94;
            // A place that cannot make a thing pays well for it. That is the
            // whole trade route, so sell into it even if the average disagrees.
            if (world_price_bias(w, g) > 0) good_price = 1;
            // Scrap is dead weight; dump it wherever it is not insulting.
            if (g == G_SCRAP) good_price = pays * 100 >= avg * 74;
        } else {
            // With nothing to compare against, holding beats guessing --
            // except for scrap, which is never worth the slot.
            good_price = (g == G_SCRAP);
        }

        if (good_price) return step_to(sel, g, BTN_B);
    }

    // A purchase is only worth walking the cursor to if it can actually
    // happen. Without the room check the bot presses BUY at a full hold
    // forever, which is exactly how the 13-hop route deadlocked it.
    int room = cargo < world_cargo_cap(w);
    // A full hold that wanted to buy is the only honest evidence that more
    // slots would be worth paying for. Counted here rather than assumed, so
    // the racks are valued by pressure this run actually felt.
    if (!room) b->hold_blocked++;

    // 2. Top up fuel, which is the resource that ends runs. Buy it even at a
    //    poor price -- being stranded costs more than being overcharged.
    if (room && w->held[G_FUEL] < keep[G_FUEL] && w->credits >= nd->price[G_FUEL]) {
        int act = step_to(sel, G_FUEL, BTN_A);
        if (act == BTN_A) b->bought_here[G_FUEL] = 1;
        return act;
    }

    // 3. Then water.
    if (room && w->held[G_WATER] < keep[G_WATER] && w->credits >= nd->price[G_WATER]) {
        int act = step_to(sel, G_WATER, BTN_A);
        if (act == BTN_A) b->bought_here[G_WATER] = 1;
        return act;
    }

    // 3b. Buy what a job still needs. Only 43% of accepted contracts were ever
    //     delivered, and nothing in the bot ever bought toward one -- it took
    //     the job and then hoped the goods turned up in the hold by accident.
    if (b->feats & BOT_CONTRACT) {
        for (int g = 0; g < GOODS_COUNT; ++g) {
            int need = contract_short(w, g);
            if (need <= 0) continue;
            if (!room || w->credits < nd->price[g]) continue;
            // Only where it is not absurdly dear: a job bought at any price is
            // a job that cost more than it pays.
            int avg = avg_price(b, g);
            if (avg > 0 && b->seen[g] >= 2 && nd->price[g] * 100 > avg * 115) continue;
            int act = step_to(sel, g, BTN_A);
            if (act == BTN_A) b->bought_here[g] = 1;
            return act;
        }
    }

    // 4. Speculate: buy anything unusually cheap, if there is room and money to
    //    spare, to sell further east. This is the part a fixed script cannot do.
    if (room && cargo < world_cargo_cap(w) - 6 && hops > 1) {
        int spec = local_spec;
        for (int g = 0; g < GOODS_COUNT; ++g) {
            if (g == G_FUEL || g == G_WATER) continue;   // survival stock, handled above

            // A settlement's own speciality is cheap here by construction, so
            // it is worth loading even before enough markets have been seen to
            // form an average. This is the route the archetypes exist to
            // create: buy where a thing is made, sell where it is not.
            int cheap = (g == spec);
            if (!cheap && world_price_bias(w, g) < 0) cheap = 1;
            if (!cheap) {
                int avg = avg_price(b, g);
                if (avg == 0 || b->seen[g] < 2) continue;
                cheap = nd->price[g] * 100 <= avg * 80;
            }

            int affordable = w->credits - nd->price[g] >= b->float_credits;
            if (cheap && affordable) {
                int act = step_to(sel, g, BTN_A);
                if (act == BTN_A) b->bought_here[g] = 1;
                return act;
            }
        }
    }

    return BTN_START;   // nothing left worth doing here
}

// ---------------------------------------------------------------- map
static int score_node(const World *w, const Node *nd) {
    switch (nd->type) {
    case NODE_GREEN:  return 1000;
    case NODE_SETTLE: {
        // A settlement is worth more when it specialises in something the
        // convoy is actually short of. A refinery matters when the tank is
        // low and is just another shop when it is not.
        int keep[GOODS_COUNT];
        reserves(w, keep);
        int score = 30;
        int spec = world_arch_good(nd->archetype);
        if (spec == G_FUEL  && w->held[G_FUEL]  <= keep[G_FUEL])  score += 26;
        if (spec == G_WATER && w->held[G_WATER] <= keep[G_WATER]) score += 26;
        if (spec == G_AMMO  && w->held[G_AMMO]  < 2)              score += 10;
        return score;
    }
    case NODE_EMPTY:  return 10;
    case NODE_EVENT:  return 6;
    case NODE_HAZARD:
        // A storm costs an extra fuel and water. That is survivable when
        // stocked and fatal when not.
        return (w->held[G_FUEL] <= 2 || w->held[G_WATER] <= 2) ? -60 : -8;
    default:          return 0;
    }
}

// The best node reachable from a candidate, one hop further on. A player sees
// the whole route drawn on screen; the bot saw exactly one link. Scoring the
// chain is not clairvoyance -- every node's type and archetype is already
// visible on the map -- it is just reading what is in front of it.
static int lookahead_bonus(const World *w, int from_index) {
    int next = w->sector + 1;
    if (next >= SECTORS - 1) return 0;          // the Green Zone ends it anyway
    uint8_t links = w->node[next][from_index].links;
    int best = -100000;
    for (int m = 0; m < NODES_PER; ++m) {
        if (!(links & (1u << m))) continue;
        if (!w->node[next + 1][m].active) continue;
        int sc = score_node(w, &w->node[next + 1][m]);
        if (sc > best) best = sc;
    }
    return best == -100000 ? 0 : best;
}

static int decide_map(const World *w, int map_sel, int feats) {
    // world_reachable, not a copy of it. The copy that used to be here omitted
    // the `sector >= SECTORS - 1` guard, so at the last sector it would have
    // read node[SECTORS][m] -- one row past the end of the array. Unreachable
    // today only because arriving at the Green Zone sets ST_WON before the map
    // is ever drawn, which is a state-machine accident rather than a bound.
    int cand[NODES_PER];
    int n = world_reachable(w, cand);
    if (n == 0) return BTN_A;

    int best = 0, best_score = -100000;
    for (int i = 0; i < n; ++i) {
        int s = score_node(w, &w->node[w->sector + 1][cand[i]]);
        // A good stop that leads nowhere is worth less than a fair one that
        // opens onto a well. Discounted, because the second hop is a choice
        // not yet made and the convoy may not take it.
        if (feats & BOT_LOOKAHEAD) s += lookahead_bonus(w, cand[i]) / 2;
        if (s > best_score) { best_score = s; best = i; }
    }
    return step_to(map_sel, best, BTN_A);
}

// ---------------------------------------------------------------- event
// The seed stock is never sold and never traded away -- world_sell refuses it
// outright -- but it does occupy slots, which the capacity checks already see
// through world_cargo(). The bot's only obligation is to protect it, which it
// does by valuing the hold rather than by any special case.

// Roughly what a unit of each good is worth to the convoy right now. Survival
// stock is worth more than its price when the tank or the tanks are low, which
// is what stops the bot trading away the thing that is about to kill it.
static int good_value(const Bot *b, const World *w, int g) {
    // Priced from what this convoy has actually seen. The fixed table that
    // used to be here had drifted from the game's own base prices -- water 13
    // against 12, fuel 22 against 17 -- and neither side could ever have
    // reported the drift, because each was only read by itself.
    int v = unit_value(b, w, g);
    int keep[GOODS_COUNT];
    reserves(w, keep);
    if ((g == G_FUEL || g == G_WATER) && w->held[g] <= keep[g]) v *= 3;
    return v;
}

// Encounters are evaluated generically, from the numbers in the Event rather
// than from its kind. Fourteen kinds and counting all resolve through this, so
// adding a fifteenth needs no change here at all -- which is the whole point,
// since a mechanic the bot cannot judge is a mechanic nobody can measure.
static int decide_event(const Bot *b, const World *w) {
    const Event *e = &w->event;
    if (b->refuse_all) return BTN_B;

    // Note the ordering. This used to return "refuse" the moment accepting was
    // unaffordable, before the third branch was considered at all -- and a
    // manoeuvre that costs nothing is affordable in exactly the situations
    // where paying is not. The branch was being skipped precisely when it was
    // the only option worth having.
    int affordable = world_can_accept(w);

    int keep[GOODS_COUNT];
    reserves(w, keep);

    // Dipping into the survival reserve is expensive, not forbidden.
    //
    // This was a veto: any payment in fuel or water that left the convoy below
    // its reserve was refused before the deal was even priced. Because the bot
    // provisions *to* that reserve, it is nearly always sitting on exactly the
    // reserve, so the rule fired on any payment of one unit -- and six of the
    // fourteen encounter kinds charge in fuel or water. Measured, those six
    // were accepted between 2% and 10% of the time regardless of what they
    // offered, and no change to their payoffs could have moved that.
    //
    // A player facing "one fuel for a hundred and thirty credits, with a
    // refinery two hops on" does not have a rule forbidding it; they weigh it.
    // So the dip is priced instead: steeply, because being stranded ends the
    // run, and never down to the last unit.
    // The premium is already in good_value, which triples fuel and water once
    // the convoy is at or below its reserve. Adding a separate multiplier on
    // top charged twelve times the market price for a unit of fuel, which no
    // payoff in the table can clear -- so the first attempt at relaxing the
    // veto moved the accept rates by two points and changed nothing.
    if (affordable && e->pay_good >= 0 && e->pay_qty > 0
        && (e->pay_good == G_FUEL || e->pay_good == G_WATER)) {
        if (w->held[e->pay_good] - e->pay_qty < 1) affordable = 0;  // never the last one
    }

    int cost = 0;
    if (e->pay_good >= 0) cost = e->pay_qty * good_value(b, w, e->pay_good);

    int benefit = e->gain_credits;
    if (e->gain_good >= 0) benefit += e->gain_qty * good_value(b, w, e->gain_good);

    // Refusing has a price too: a named good, a bite out of the hold, or the
    // seed itself. The seed is what the run is for, so it is priced far above
    // anything it physically weighs.
    if (e->lose_qty > 0) {
        if (e->lose_good == -2)      benefit += e->lose_qty * 90;
        else if (e->lose_good >= 0)  benefit += e->lose_qty * good_value(b, w, e->lose_good);
        else                         benefit += e->lose_qty * 18;
    }

    // Losing the last of the hold ends the run, so treat that as unaffordable
    // rather than merely expensive.
    if (affordable && e->lose_good < 0 && e->lose_qty >= world_cargo(w)) return BTN_A;

    // Three options now, scored the same way. Deliberately no per-kind logic:
    // the rule this file has kept since the fifth encounter kind was added is
    // that a fifteenth should need no change here, and a third *branch* should
    // not break it either.
    //
    //   accept  = benefit - cost                     (certain)
    //   decline = 0                                  (certain, the baseline)
    //   attempt = p*(benefit - alt_cost) - (1-p)*(decline_cost + one more)
    //
    // `benefit` already folds in what refusing would have cost, so declining
    // is the zero point and the other two are measured against it.
    int ev_accept = affordable ? benefit - cost : -1000000;
    int ev_alt    = -1000000;
    if (e->alt_who >= 0 && world_can_attempt(w)) {
        int alt_cost = 0;
        if (e->alt_pay_good >= 0)
            alt_cost = e->alt_pay_qty * good_value(b, w, e->alt_pay_good);
        // One unit worse than declining, in whatever the refusal is priced in.
        int extra = (e->lose_good == -2) ? 90
                  : (e->lose_good >= 0)  ? good_value(b, w, e->lose_good) : 18;
        int p = e->alt_odds;
        ev_alt = (p * (benefit - alt_cost) - (100 - p) * extra) / 100;
    }

    if (ev_alt > ev_accept && ev_alt > 0) return BTN_START;
    return (affordable && ev_accept > 0) ? BTN_A : BTN_B;
}

// ---------------------------------------------------------------- driver
void bot_init(Bot *b, int float_credits) {
    memset(b, 0, sizeof *b);
    b->float_credits = float_credits;
    b->feats = BOT_ALL;
    b->at_sector = -1;
    b->at_index  = -1;
}

int bot_step(Bot *b, const World *w, int sel, int map_sel, int tab, int title) {
    if (title) return BTN_START;

    // New stop: forget what was bought at the last one, and take one price
    // reading. observe() used to run on every step, i.e. every keypress, so a
    // market was counted five to twenty times depending on how long the bot
    // stood in it -- and since world_buy and world_sell move the local price
    // permanently, what accumulated was the moved price, repeatedly.
    //
    // The error was therefore a function of how much the bot traded, which is
    // the thing the average is used to decide. The game's own running average
    // (observe_market) has always been once per arrival; these now agree.
    if (w->sector != b->at_sector || w->index != b->at_index) {
        if (w->sector != b->at_sector && b->at_sector >= 0) b->hops_done++;
        b->at_sector = w->sector;
        b->at_index  = w->index;
        for (int g = 0; g < GOODS_COUNT; ++g) b->bought_here[g] = 0;
        observe(b, w);
    }

    // Record an encounter once, as it is met. What the convoy has actually
    // been asked for is the only basis it has for pricing a hand or a plate
    // of armour, and it is a better one than a constant.
    if (w->state == ST_EVENT) {
        if (!b->in_event) { b->in_event = 1; note_event(b, w); }
    } else {
        b->in_event = 0;
    }

    switch (w->state) {
    case ST_TRADE:
        // Deal with the job board first: a delivery pays better than anything
        // the same slots would earn on speculation.
        if (contract_worth_taking(w)) {
            if (tab != TAB_CONTRACTS) return BTN_RIGHT;   // tabs cycle forward
            return BTN_A;
        }
        // A favour from someone aboard. Worth taking when it can be kept: the
        // reward is standing, which is what the third branch runs on, and
        // failing one costs two steps of it and eventually the hand.
        if (w->errand.state == ERR_OFFERED) {
            if (tab != TAB_CREW) return BTN_RIGHT;
            int keepable = (w->errand.qty == 0)
                        || (world_cargo(w) + w->errand.qty < world_cargo_cap(w) - 4);
            return keepable ? BTN_A : BTN_B;
        }

        if ((b->feats & BOT_CONTRACT) && contract_worth_declining(w)) {
            if (tab != TAB_CONTRACTS) return BTN_RIGHT;
            return BTN_B;
        }
        if (upgrade_worth_buying(b, w)) {
            if (tab != TAB_GARAGE) return BTN_RIGHT;
            return BTN_A;
        }
        if (crew_worth_hiring(b, w)) {
            if (tab != TAB_CREW) return BTN_RIGHT;
            return BTN_A;
        }
        if (tab != TAB_MARKET) return BTN_RIGHT;
        return decide_trade(b, w, sel);
    case ST_MAP:   return decide_map(w, map_sel, b->feats);
    case ST_EVENT: return decide_event(b, w);
    default:       return -1;    // run is over
    }
}
