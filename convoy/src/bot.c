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
    keep[G_WATER] = span + 2;
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
    if (world_cargo(w) + j->qty > CARGO_CAP - 4) return 0;
    if (j->by_sector > SECTORS - 1) return 0;
    return 1;
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
    int room = cargo < CARGO_CAP;

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
    if (room && cargo < CARGO_CAP - 6 && hops > 1) {
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
static int decide_event(const World *w) {
    const Event *e = &w->event;
    if (!world_can_accept(w)) return BTN_B;

    int keep[GOODS_COUNT];
    reserves(w, keep);

    // Refuse if paying would cut into what is needed to finish the route.
    if (e->pay_good >= 0) {
        int after = w->held[e->pay_good] - e->pay_qty;
        if ((e->pay_good == G_FUEL || e->pay_good == G_WATER)
            && after < keep[e->pay_good])
            return BTN_B;
    }

    switch (e->kind) {
    case EV_RAID:
        // Losing cargo is losing health. Fight while there is ammo to spare.
        return (w->held[G_AMMO] >= e->pay_qty) ? BTN_A : BTN_B;
    case EV_WRECK:
        return (w->held[G_FUEL] > keep[G_FUEL]) ? BTN_A : BTN_B;
    case EV_TRADER:
        return (w->held[G_WATER] - e->pay_qty >= keep[G_WATER]) ? BTN_A : BTN_B;
    case EV_SICK:
    case EV_BREAK:
    default:
        return BTN_A;   // paying the stated price beats the stated consequence
    }
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
        if (tab != TAB_MARKET) return BTN_RIGHT;
        return decide_trade(b, w, sel);
    case ST_MAP:   return decide_map(w, map_sel);
    case ST_EVENT: return decide_event(w);
    default:       return -1;    // run is over
    }
}
