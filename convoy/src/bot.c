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

#include <string.h>

#define SECTORS_LAST (SECTORS - 1)

// How much of each good the bot refuses to sell, given how far is left to go.
static void reserves(const World *w, int *keep) {
    int hops = SECTORS_LAST - w->sector;
    if (hops < 0) hops = 0;

    keep[G_FUEL]  = hops + 1;      // +1 for a storm eating one
    keep[G_WATER] = hops + 2;
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

// ---------------------------------------------------------------- trade
static int decide_trade(Bot *b, const World *w, int sel) {
    const Node *nd = &w->node[w->sector][w->index];
    int keep[GOODS_COUNT];
    reserves(w, keep);

    int cargo = world_cargo(w);
    int hops  = SECTORS_LAST - w->sector;

    // 1. Sell surplus into a market that is paying above what we have seen
    //    elsewhere. Never sell below the survival reserve.
    for (int g = 0; g < GOODS_COUNT; ++g) {
        int surplus = w->held[g] - keep[g];
        if (surplus <= 0) continue;
        int avg = avg_price(b, g);
        int good_price = avg == 0 || nd->price[g] * 100 >= avg * 108;
        // Scrap is dead weight; dump it wherever it is not actively insulting.
        if (g == G_SCRAP) good_price = avg == 0 || nd->price[g] * 100 >= avg * 85;
        if (good_price) return step_to(sel, g, BTN_B);
    }

    // 2. Top up fuel, which is the resource that ends runs. Buy it even at a
    //    poor price -- being stranded costs more than being overcharged.
    if (w->held[G_FUEL] < keep[G_FUEL] && w->credits >= nd->price[G_FUEL])
        return step_to(sel, G_FUEL, BTN_A);

    // 3. Then water.
    if (w->held[G_WATER] < keep[G_WATER] && w->credits >= nd->price[G_WATER])
        return step_to(sel, G_WATER, BTN_A);

    // 4. Speculate: buy anything unusually cheap, if there is room and money to
    //    spare, to sell further east. This is the part a fixed script cannot do.
    if (cargo < CARGO_CAP - 2 && hops > 1) {
        for (int g = 0; g < GOODS_COUNT; ++g) {
            int avg = avg_price(b, g);
            if (avg == 0 || b->seen[g] < 2) continue;
            if (g == G_FUEL || g == G_WATER) continue;   // survival stock, handled above
            int cheap = nd->price[g] * 100 <= avg * 72;
            int affordable = w->credits - nd->price[g] >= b->float_credits;
            if (cheap && affordable) return step_to(sel, g, BTN_A);
        }
    }

    return BTN_START;   // nothing left worth doing here
}

// ---------------------------------------------------------------- map
static int score_node(const World *w, const Node *nd) {
    switch (nd->type) {
    case NODE_GREEN:  return 1000;
    case NODE_SETTLE: return 30;
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
}

int bot_step(Bot *b, const World *w, int sel, int map_sel, int title) {
    if (title) return BTN_START;

    observe(b, w);

    switch (w->state) {
    case ST_TRADE: return decide_trade(b, w, sel);
    case ST_MAP:   return decide_map(w, map_sel);
    case ST_EVENT: return decide_event(w);
    default:       return -1;    // run is over
    }
}
