#include "world.h"

// Nominal value of each good before local supply and demand distort it.
static const int BASE_PRICE[GOODS_COUNT] = { 12, 20, 25, 40, 6 };

// ---------------------------------------------------------------- rng
static uint32_t rng_next(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *s = x;
}
static int rng_range(uint32_t *s, int lo, int hi) {
    return lo + (int)(rng_next(s) % (uint32_t)(hi - lo + 1));
}

static void roll_event(World *w);

// ---------------------------------------------------------------- setup
static void price_node(World *w, Node *n, int sector) {
    for (int g = 0; g < GOODS_COUNT; ++g) {
        // A wide spread is what makes arbitrage worth the cargo space.
        int pct = rng_range(&w->rng, 40, 195);
        int p = BASE_PRICE[g] * pct / 100;
        // Fuel gets dearer the further east you go, so the run gets harder to
        // afford exactly as it gets harder to survive.
        if (g == G_FUEL) p = p * (100 + sector * 4) / 100;
        n->price[g] = (int16_t)(p < 1 ? 1 : p);
    }
}

void world_init(World *w, uint32_t seed) {
    for (int i = 0; i < (int)sizeof *w; ++i) ((uint8_t *)w)[i] = 0;
    w->rng = seed ? seed : 1u;

    for (int s = 0; s < SECTORS; ++s) {
        int count;
        if (s == 0)                count = 1;              // the convoy starts alone
        else if (s == SECTORS - 1) count = 1;              // everything converges on the goal
        else                       count = rng_range(&w->rng, 2, NODES_PER);

        // Active nodes always occupy indices 0..count-1, which guarantees the
        // |n - m| <= 1 link rule below can never strand a node.
        for (int n = 0; n < count; ++n) {
            Node *nd = &w->node[s][n];
            nd->active = 1;

            if (s == 0)                     nd->type = NODE_SETTLE;
            else if (s == SECTORS - 1)      nd->type = NODE_GREEN;
            else {
                int r = rng_range(&w->rng, 0, 99);
                nd->type = (uint8_t)(r < 46 ? NODE_SETTLE :
                                     r < 76 ? NODE_EVENT  :
                                     r < 92 ? NODE_HAZARD : NODE_EMPTY);
            }
            price_node(w, nd, s);
        }
    }

    // Link each node forward to the neighbours it lines up with.
    for (int s = 0; s < SECTORS - 1; ++s) {
        for (int n = 0; n < NODES_PER; ++n) {
            if (!w->node[s][n].active) continue;
            uint8_t mask = 0;
            for (int m = 0; m < NODES_PER; ++m) {
                if (!w->node[s + 1][m].active) continue;
                int d = n - m; if (d < 0) d = -d;
                if (d <= 1) mask |= (uint8_t)(1u << m);
            }
            if (!mask) mask = 1u;   // fall back to the first node; never dead-end
            w->node[s][n].links = mask;
        }
    }

    w->sector = 0;
    w->index  = 0;
    w->node[0][0].visited = 1;
    // Deliberately lean: enough to start trading, not enough to simply buy
    // your way east without ever selling at a profit.
    w->credits = 150;
    w->held[G_WATER] = 9;
    w->held[G_FUEL]  = 6;
    w->held[G_AMMO]  = 4;
    w->held[G_MEDS]  = 1;
    w->held[G_SCRAP] = 2;
    w->day   = 1;
    w->state = ST_TRADE;   // the starting node is a settlement
}

// ---------------------------------------------------------------- helpers
int world_cargo(const World *w) {
    int t = 0;
    for (int g = 0; g < GOODS_COUNT; ++g) t += w->held[g];
    return t;
}

static void drop_random_cargo(World *w, int units) {
    for (int i = 0; i < units; ++i) {
        if (world_cargo(w) == 0) return;
        // Walk from a random start so losses spread across goods.
        int start = rng_range(&w->rng, 0, GOODS_COUNT - 1);
        for (int k = 0; k < GOODS_COUNT; ++k) {
            int g = (start + k) % GOODS_COUNT;
            if (w->held[g] > 0) { w->held[g]--; break; }
        }
    }
}

int world_reachable(const World *w, int *out) {
    int n = 0;
    if (w->sector >= SECTORS - 1) return 0;
    uint8_t links = w->node[w->sector][w->index].links;
    for (int m = 0; m < NODES_PER; ++m)
        if ((links & (1u << m)) && w->node[w->sector + 1][m].active) out[n++] = m;
    return n;
}

int world_can_travel(const World *w, int next_index) {
    if (w->sector >= SECTORS - 1) return 0;
    if (next_index < 0 || next_index >= NODES_PER) return 0;
    if (!w->node[w->sector + 1][next_index].active) return 0;
    if (!(w->node[w->sector][w->index].links & (1u << next_index))) return 0;
    return w->held[G_FUEL] >= 1;
}

// ---------------------------------------------------------------- travel
void world_travel(World *w, int next_index) {
    if (!world_can_travel(w, next_index)) {
        // Out of fuel with nowhere to go: the run ends here.
        if (w->held[G_FUEL] < 1) { w->state = ST_DEAD; w->death = DEATH_STRANDED; }
        return;
    }

    w->held[G_FUEL]--;
    w->day++;

    // The crew drink whether or not there is anything to drink.
    if (w->held[G_WATER] < 1) { w->state = ST_DEAD; w->death = DEATH_THIRST; return; }
    w->held[G_WATER]--;

    w->sector++;
    w->index = next_index;
    Node *nd = &w->node[w->sector][w->index];
    nd->visited = 1;

    if (nd->type == NODE_HAZARD) {
        // A storm eats supplies on arrival.
        if (w->held[G_WATER] > 0) w->held[G_WATER]--;
        if (w->held[G_FUEL]  > 0) w->held[G_FUEL]--;
        if (w->held[G_WATER] == 0 && w->held[G_FUEL] == 0 && world_cargo(w) == 0) {
            w->state = ST_DEAD; w->death = DEATH_STRIPPED; return;
        }
    }

    switch (nd->type) {
    case NODE_GREEN:  w->state = ST_WON;   break;
    case NODE_SETTLE: w->state = ST_TRADE; break;
    case NODE_EVENT:  w->state = ST_EVENT; roll_event(w); break;
    default:          w->state = ST_MAP;   break;
    }
}

// ---------------------------------------------------------------- market
// Trades move the local price permanently. Sell into a market and it stays
// depressed for the rest of the run: routes burn out behind the player, which
// is what forces the push outward.
void world_buy(World *w, int good) {
    Node *nd = &w->node[w->sector][w->index];
    if (nd->type != NODE_SETTLE) return;
    int p = nd->price[good];
    if (w->credits < p || world_cargo(w) >= CARGO_CAP) return;

    w->credits -= p;
    w->held[good]++;
    nd->price[good] = (int16_t)(p + p / 10 + 1);
}

void world_sell(World *w, int good) {
    Node *nd = &w->node[w->sector][w->index];
    if (nd->type != NODE_SETTLE) return;
    if (w->held[good] < 1) return;

    int p = nd->price[good];
    w->credits += p;
    w->held[good]--;
    int np = p - p / 8 - 1;
    nd->price[good] = (int16_t)(np < 1 ? 1 : np);
}

// ---------------------------------------------------------------- encounters
int world_can_accept(const World *w) {
    const Event *e = &w->event;
    if (e->pay_good < 0) return 1;
    return w->held[e->pay_good] >= e->pay_qty;
}

static void end_event(World *w) {
    if (world_cargo(w) == 0) { w->state = ST_DEAD; w->death = DEATH_STRIPPED; return; }
    w->state = ST_MAP;
}

void world_accept(World *w) {
    if (w->state != ST_EVENT) return;
    if (!world_can_accept(w)) return;   // can't afford it; must decline

    Event *e = &w->event;
    if (e->pay_good >= 0) w->held[e->pay_good] -= e->pay_qty;

    if (e->gain_good >= 0) {
        int room = CARGO_CAP - world_cargo(w);
        int q = e->gain_qty < room ? e->gain_qty : room;
        if (q > 0) w->held[e->gain_good] += q;
    }
    w->credits += e->gain_credits;
    end_event(w);
}

void world_decline(World *w) {
    if (w->state != ST_EVENT) return;
    Event *e = &w->event;

    if (e->lose_qty > 0) {
        if (e->lose_good < 0) {
            drop_random_cargo(w, e->lose_qty);
        } else {
            int g = e->lose_good;
            w->held[g] -= e->lose_qty;
            if (w->held[g] < 0) w->held[g] = 0;
        }
    }
    end_event(w);
}

// Rolls a fresh encounter. Deeper sectors bite harder.
static void roll_event(World *w) {
    Event *e = &w->event;
    for (int i = 0; i < (int)sizeof *e; ++i) ((uint8_t *)e)[i] = 0;
    e->pay_good = e->gain_good = e->lose_good = -1;

    int depth = w->sector;
    int kind  = rng_range(&w->rng, 0, EV_KINDS - 1);
    e->kind = (uint8_t)kind;

    switch (kind) {
    case EV_RAID:
        e->pay_good = G_AMMO;  e->pay_qty = (int8_t)rng_range(&w->rng, 2, 3);
        e->lose_good = -1;     e->lose_qty = (int8_t)(rng_range(&w->rng, 2, 4) + depth / 3);
        break;
    case EV_WRECK:
        e->pay_good = G_FUEL;  e->pay_qty = 1;
        e->gain_good = G_SCRAP; e->gain_qty = (int8_t)rng_range(&w->rng, 3, 6);
        e->lose_qty = 0;                       // walking away is free
        break;
    case EV_SICK:
        e->pay_good = G_MEDS;  e->pay_qty = 1;
        e->lose_good = G_WATER; e->lose_qty = (int8_t)rng_range(&w->rng, 2, 3);
        break;
    case EV_BREAK:
        e->pay_good = G_SCRAP; e->pay_qty = (int8_t)rng_range(&w->rng, 2, 3);
        e->lose_good = G_FUEL;  e->lose_qty = 2;
        break;
    default: // EV_TRADER -- an opportunity rather than a threat
        e->pay_good = G_WATER; e->pay_qty = (int8_t)rng_range(&w->rng, 2, 3);
        e->gain_credits = (int16_t)(rng_range(&w->rng, 30, 60) + depth * 8);
        e->lose_qty = 0;
        break;
    }
}
