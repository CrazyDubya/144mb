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
    keep[G_WATER] = span * world_water_burn(w) + 2;
    keep[G_AMMO]  = 2;             // enough to refuse one raid
    keep[G_MEDS]  = 1;
    keep[G_SCRAP] = 0;             // pure trade good
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
    if (j->by_sector > SECTORS - 1) return 0;
    return 1;
}

// What a fitting is worth over the hops that remain, in credits.
//
// Derived from the generator's own rates, not from instinct: encounters are
// 30% of nodes and there are five kinds, so any one kind fires about 0.8 times
// in a 13-hop run. An earlier version of this assumed a raid every four hops
// and overvalued armour by more than 3x.
#define FUEL_WORTH  22
#define WATER_WORTH 13

static int upgrade_payback(int u, int hops) {
    switch (u) {
    case UPG_HOLD:   return hops * 6;                     // more to trade with
    case UPG_ECON:   return (hops / 2) * FUEL_WORTH;      // a free hop every second
    case UPG_ARMOUR: return hops * 3 / 5 * 20;            // ~0.8 raids, ~4 cargo each
    case UPG_TANKS:  return (hops / 2) * WATER_WORTH;     // a dry day every second
    default:         return 0;
    }
}

// Crew drink every day they are aboard, so their keep comes straight out of
// whatever they save.
// Each hand now covers a category of trouble rather than one encounter kind,
// which is roughly three of the fourteen, and drinks on alternate days rather
// than every day. Both halves of that were needed: at one kind and a full
// ration, every hire was net-negative and travelling alone was correct.
static int crew_payback(const World *w, int k, int hops) {
    int gross;
    switch (k) {
    case CREW_MECHANIC: gross = hops * 3 / 5 * 26; break;  // breaks, leaks, bridges
    case CREW_GUARD:    gross = hops * 3 / 5 * 45; break;  // raids, tolls, checkpoints
    case CREW_MEDIC:    gross = hops * 3 / 5 * 30; break;  // sickness, plague, refugees
    case CREW_SCOUT:    gross = hops * 3 / 5 * 28; break;  // storms, bridges, caches
    case CREW_TRADER:   gross = hops * 12;         break;  // every sale, plus tip-offs
    default:            gross = 0;
    }
    // Alternate-day rations, and a medic pays part of their own keep.
    int keep = hops * WATER_WORTH / 2;
    if (k == CREW_MEDIC)       keep /= 2;
    if (w->upgrade[UPG_TANKS]) keep /= 2;
    return gross - keep;
}

// Credits in the hold compound -- buy low, sell high, repeat -- so over the
// legs that remain, working capital roughly triples. A fitting has to beat
// that, not merely beat zero, which in practice means kit is only ever correct
// out of genuine surplus.
static int upgrade_worth_buying(const World *w) {
    int u = w->offer_upg;
    if (u >= UPG_COUNT || w->upgrade[u]) return 0;
    int hops = SECTORS_LAST - w->sector;
    if (hops < 5) return 0;
    // Price is now derived from remaining payback, so the question is no
    // longer "does this pay for itself" -- it always does on paper -- but
    // "can the convoy spare the capital, and is the gamble worth it".
    int price = world_upg_price(w, u, w->offer_salvaged);
    int float_needed = w->offer_salvaged ? 70 : 120;
    if (w->credits - price < float_needed) return 0;
    (void)hops;
    return 1;
}

static int crew_worth_hiring(const World *w) {
    int k = w->offer_crew;
    if (k >= CREW_COUNT || w->crew[k]) return 0;
    int hops = SECTORS_LAST - w->sector;
    if (hops < 5) return 0;
    int price = world_crew_price(w, k);
    if (w->credits - price < 100) return 0;
    if (w->held[G_WATER] < 6) return 0;          // cannot feed them yet
    return crew_payback(w, k, hops) > price;
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

static int decide_map(const World *w, int map_sel) {
    int cand[NODES_PER], n = 0;
    uint8_t links = w->node[w->sector][w->index].links;
    for (int m = 0; m < NODES_PER; ++m)
        if ((links & (1u << m)) && w->node[w->sector + 1][m].active) cand[n++] = m;
    if (n == 0) return BTN_A;

    int best = 0, best_score = -100000;
    for (int i = 0; i < n; ++i) {
        int s = score_node(w, &w->node[w->sector + 1][cand[i]]);
        if (s > best_score) { best_score = s; best = i; }
    }
    return step_to(map_sel, best, BTN_A);
}

// ---------------------------------------------------------------- event
// Roughly what a unit of each good is worth to the convoy right now. Survival
// stock is worth more than its price when the tank or the tanks are low, which
// is what stops the bot trading away the thing that is about to kill it.
static int good_value(const World *w, int g) {
    static const int BASE[GOODS_COUNT] = { 13, 22, 25, 40, 6 };
    int v = BASE[g];
    int keep[GOODS_COUNT];
    reserves(w, keep);
    if ((g == G_FUEL || g == G_WATER) && w->held[g] <= keep[g]) v *= 3;
    return v;
}

// Encounters are evaluated generically, from the numbers in the Event rather
// than from its kind. Fourteen kinds and counting all resolve through this, so
// adding a fifteenth needs no change here at all -- which is the whole point,
// since a mechanic the bot cannot judge is a mechanic nobody can measure.
static int decide_event(const World *w) {
    const Event *e = &w->event;
    if (!world_can_accept(w)) return BTN_B;

    int keep[GOODS_COUNT];
    reserves(w, keep);

    // Refuse anything that would eat into what is needed to finish the route,
    // however good the deal looks on paper.
    if (e->pay_good >= 0 && e->pay_qty > 0) {
        int after = w->held[e->pay_good] - e->pay_qty;
        if ((e->pay_good == G_FUEL || e->pay_good == G_WATER)
            && after < keep[e->pay_good])
            return BTN_B;
    }

    int cost = 0;
    if (e->pay_good >= 0) cost = e->pay_qty * good_value(w, e->pay_good);

    int benefit = e->gain_credits;
    if (e->gain_good >= 0) benefit += e->gain_qty * good_value(w, e->gain_good);

    // Refusing has a price too: either a named good or a bite out of the hold.
    if (e->lose_qty > 0) {
        if (e->lose_good >= 0) benefit += e->lose_qty * good_value(w, e->lose_good);
        else                   benefit += e->lose_qty * 18;   // average cargo
    }

    // Losing the last of the hold ends the run, so treat that as unaffordable
    // rather than merely expensive.
    if (e->lose_good < 0 && e->lose_qty >= world_cargo(w)) return BTN_A;

    return benefit > cost ? BTN_A : BTN_B;
}

// ---------------------------------------------------------------- driver
void bot_init(Bot *b, int float_credits) {
    memset(b, 0, sizeof *b);
    b->float_credits = float_credits;
    b->at_sector = -1;
    b->at_index  = -1;
}

int bot_step(Bot *b, const World *w, int sel, int map_sel, int tab, int title) {
    if (title) return BTN_START;

    // New stop: forget what was bought at the last one.
    if (w->sector != b->at_sector || w->index != b->at_index) {
        b->at_sector = w->sector;
        b->at_index  = w->index;
        for (int g = 0; g < GOODS_COUNT; ++g) b->bought_here[g] = 0;
    }

    observe(b, w);

    switch (w->state) {
    case ST_TRADE:
        // Deal with the job board first: a delivery pays better than anything
        // the same slots would earn on speculation.
        if (contract_worth_taking(w)) {
            if (tab != TAB_CONTRACTS) return BTN_RIGHT;   // tabs cycle forward
            return BTN_A;
        }
        if (upgrade_worth_buying(w)) {
            if (tab != TAB_GARAGE) return BTN_RIGHT;
            return BTN_A;
        }
        if (crew_worth_hiring(w)) {
            if (tab != TAB_CREW) return BTN_RIGHT;
            return BTN_A;
        }
        if (tab != TAB_MARKET) return BTN_RIGHT;
        return decide_trade(b, w, sel);
    case ST_MAP:   return decide_map(w, map_sel);
    case ST_EVENT: return decide_event(w);
    default:       return -1;    // run is over
    }
}
