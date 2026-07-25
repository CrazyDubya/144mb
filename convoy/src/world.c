#include "world.h"

// Nominal value of each good before local supply and demand distort it.
static const int BASE_PRICE[GOODS_COUNT] = { 12, 17, 25, 40, 6 };

// Percentage shift each settlement type applies to each good. A place is cheap
// in what it makes and dear in what it must import -- and since one price
// serves for both buying and selling, "dear" also means "they pay well here",
// which is the whole basis of a trade route.
static const int8_t ARCH_MOD[ARCH_COUNT][GOODS_COUNT] = {
    /*                water  fuel  ammo  meds  scrap */
    /* WELL      */ {  -48,  +38,    0,   +5,  +12 },
    /* REFINERY  */ {  +34,  -48,   +8,    0,  -12 },
    /* ARMOURY   */ {  +18,  +12,  -48,  +22,   +5 },
    /* CLINIC    */ {   +8,  +18,  +24,  -48,    0 },
    /* SCRAPYARD */ {  +24,  +10,   +6,  +18,  -48 },
    /* GENERAL   */ {    0,    0,    0,    0,    0 },
};

int world_arch_good(int archetype) {
    switch (archetype) {
    case ARCH_WELL:      return G_WATER;
    case ARCH_REFINERY:  return G_FUEL;
    case ARCH_ARMOURY:   return G_AMMO;
    case ARCH_CLINIC:    return G_MEDS;
    case ARCH_SCRAPYARD: return G_SCRAP;
    default:             return -1;
    }
}

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
static void observe_market(World *w);
static void contract_tick(World *w);
static void roll_offers(World *w);

// ---------------------------------------------------------------- setup
static void price_node(World *w, Node *n, int sector) {
    for (int g = 0; g < GOODS_COUNT; ++g) {
        // Local noise is deliberately narrower than it used to be: the
        // archetype should be the loudest signal in a price, not the dice.
        int pct = rng_range(&w->rng, 72, 132);
        int p = BASE_PRICE[g] * pct / 100;
        p = p * (100 + ARCH_MOD[n->archetype][g]) / 100;
        // Fuel gets dearer the further east you go, so the run gets harder to
        // afford exactly as it gets harder to survive.
        if (g == G_FUEL) p = p * (100 + sector * 2) / 100;
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
            // A general trading post shows up often enough to be the baseline
            // the specialists are read against.
            if (nd->type == NODE_SETTLE) {
                int r = rng_range(&w->rng, 0, 99);
                nd->archetype = (uint8_t)(r < 26 ? ARCH_GENERAL
                                                 : rng_range(&w->rng, 0, ARCH_COUNT - 2));
            } else {
                nd->archetype = ARCH_GENERAL;
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
    w->day     = 1;
    w->payload = PAYLOAD_SLOTS;
    w->offer_upg  = 0xFF;
    w->offer_crew = 0xFF;
    w->kit_failed = -1;
    w->state = ST_TRADE;   // the starting node is a settlement
    observe_market(w);
    contract_tick(w);
    roll_offers(w);
}

// Records the prices on offer here. Called on arrival, once per settlement.
static void observe_market(World *w) {
    const Node *nd = &w->node[w->sector][w->index];
    if (nd->type != NODE_SETTLE) return;
    for (int g = 0; g < GOODS_COUNT; ++g) {
        w->seen_sum[g] += nd->price[g];
        w->seen_n[g]++;
    }
}

int world_price_bias(const World *w, int good) {
    if (w->seen_n[good] < 2) return 0;          // no basis for a comparison yet
    int avg = (int)(w->seen_sum[good] / w->seen_n[good]);
    int p   = w->node[w->sector][w->index].price[good];
    if (p * 100 <= avg * 86)  return -1;
    if (p * 100 >= avg * 114) return +1;
    return 0;
}

// ---------------------------------------------------------------- outfitting
// Priced against what a run actually banks. The first pass asked 220-300 for
// kit when a winning convoy finishes with under 100 credits to its name, so
// the garage and the crew board were furniture nobody could afford: across
// five full bot runs, not one purchase.
// Priced against measured payback rather than instinct. Deriving what each
// fitting returns over a 13-hop run gave 47-88 credits against list prices of
// 120-150: every one of them was a trap, and the bot proved it by buying kit
// and losing more often.
static const int UPG_BASE[UPG_COUNT]   = {  70, 115,  55,  65 };
static const int CREW_BASE[CREW_COUNT] = {  80,  95,  85,  75, 110 };

int world_cargo_cap(const World *w) {
    return CARGO_CAP + (w->upgrade[UPG_HOLD] ? 10 : 0);
}

int world_crew_count(const World *w) {
    int n = 0;
    for (int i = 0; i < CREW_COUNT; ++i) n += w->crew[i] ? 1 : 0;
    return n;
}

// One driver always drinks. Every extra hand drinks too, which is the whole
// cost of taking people on.
//
// Tanks used to halve the total, rounded up -- which with no crew aboard took
// a burn of 1 to a burn of 1, so the upgrade did nothing at all in the case a
// player is most likely to buy it. Condensers instead give a dry day every
// third day, which is worth something whoever is aboard.
int world_water_burn_on(const World *w, int day) {
    if (w->upgrade[UPG_TANKS] && (day % 2) == 0) return 0;

    // The driver drinks every day; the crew ration and drink on alternate
    // ones. At a full daily rate their keep came to ~169 credits of water over
    // a run, which no specialist ability could repay -- every hire was
    // net-negative and the correct play was to travel alone.
    int burn = 1;
    if ((day % 2) == 1) burn += world_crew_count(w);
    // A medic runs the water discipline as well as the medicine.
    if (w->crew[CREW_MEDIC] && burn > 1) burn--;
    return burn;
}

int world_water_burn(const World *w) { return world_water_burn_on(w, w->day); }

// What each fitting can still earn back over the hops that remain, in credits.
// Rates come from the generator: encounters are 30% of nodes across five
// kinds, so any one kind fires about 0.8 times in a 13-hop run.
#define FUEL_WORTH  22
#define WATER_WORTH 13

int world_upg_payback(const World *w, int upg) {
    int hops = (SECTORS - 1) - w->sector;
    if (hops < 1) return 0;
    switch (upg) {
    case UPG_HOLD:   return hops * 6;
    case UPG_ECON:   return (hops / 2) * FUEL_WORTH;
    case UPG_ARMOUR: return hops * 3 / 5 * 20;
    case UPG_TANKS:  return (hops / 2) * WATER_WORTH;
    default:         return 0;
    }
}

// Price is a fixed fraction of what the thing can still return, rather than a
// base price with a sector multiplier bolted on.
//
// This is the change that makes kit a decision. A flat price is either
// unaffordable early -- when capital is needed for trade and compounds better
// -- or pointless late, when there is no run left to repay it. Deriving price
// from remaining payback means an offer is always worth its asking price on
// its face, and when payback falls to nothing the offer stops appearing at all
// instead of becoming cheap and useless.
// Two numbers, both derived rather than guessed.
//
// SOUND_PCT: credits left in the hold compound -- roughly tripling over a full
// run -- so a fitting priced at three quarters of its payback is still a loss
// against simply trading with the money. It has to come in near a third of
// payback before it competes at all.
//
// SALVAGE_PCT: salvaged kit fails about one run in three, so it is worth two
// thirds of sound kit. Pricing it at two thirds makes the two options equal in
// expectation and different only in variance -- which is what makes it a
// gamble rather than simply the correct answer. Priced any lower (it was 45%)
// salvaged is strictly better and there is no decision to make.
#define SOUND_PCT   45
#define SALVAGE_PCT 67

int world_upg_price(const World *w, int upg, int salvaged) {
    int p = world_upg_payback(w, upg) * SOUND_PCT / 100;
    if (salvaged) p = p * SALVAGE_PCT / 100;
    return p < 8 ? 8 : p;
}

// Crew are priced the same way kit is: off what they can still return, net of
// what they will drink. A flat base price left them unaffordable exactly when
// they were worth having, which is the same trap kit fell into.
int world_crew_payback(const World *w, int crew) {
    int hops = (SECTORS - 1) - w->sector;
    if (hops < 1) return 0;

    int gross;
    switch (crew) {
    case CREW_MECHANIC: gross = hops * 3 / 5 * 26; break;  // breaks, leaks, bridges
    case CREW_GUARD:    gross = hops * 3 / 5 * 45; break;  // raids, tolls, checkpoints
    case CREW_MEDIC:    gross = hops * 3 / 5 * 30; break;  // sickness, plague, refugees
    case CREW_SCOUT:    gross = hops * 3 / 5 * 28; break;  // storms, bridges, caches
    default:            gross = hops * 12;         break;  // trader: every sale
    }
    int keep = hops * WATER_WORTH / 2;        // alternate-day rations
    if (crew == CREW_MEDIC)    keep /= 2;     // runs the water discipline too
    if (w->upgrade[UPG_TANKS]) keep /= 2;
    int net = gross - keep;
    return net < 0 ? 0 : net;
}

int world_crew_price(const World *w, int crew) {
    int p = world_crew_payback(w, crew) * SOUND_PCT / 100;
    return p < 10 ? 10 : p;
}

void world_road_ahead(const World *w, int *storms, int *encounters) {
    int st = 0, ev = 0;
    for (int s = w->sector + 1; s < SECTORS; ++s) {
        // Count the worst case across the sector: what the road *could* hold.
        int sst = 0, sev = 0;
        for (int n = 0; n < NODES_PER; ++n) {
            if (!w->node[s][n].active) continue;
            if (w->node[s][n].type == NODE_HAZARD) sst = 1;
            if (w->node[s][n].type == NODE_EVENT)  sev = 1;
        }
        st += sst; ev += sev;
    }
    if (storms)     *storms = st;
    if (encounters) *encounters = ev;
}

void world_buy_upgrade(World *w) {
    int u = w->offer_upg;
    if (u >= UPG_COUNT || w->upgrade[u]) return;
    int p = world_upg_price(w, u, w->offer_salvaged);
    if (w->credits < p) return;
    w->credits -= p;
    w->upgrade[u] = 1;
    w->upg_salvaged[u] = w->offer_salvaged;
    w->offer_upg = 0xFF;
}

// Salvaged kit can give out on the road. Roughly a one-in-three chance across
// a full run, which is often enough to be felt and rare enough to be worth
// gambling on when the alternative is going without.
static void salvage_check(World *w) {
    w->kit_failed = -1;
    for (int u = 0; u < UPG_COUNT; ++u) {
        if (!w->upgrade[u] || !w->upg_salvaged[u]) continue;
        if (rng_range(&w->rng, 0, 99) < 4) {
            w->upgrade[u] = 0;
            w->upg_salvaged[u] = 0;
            w->kit_failed = (int8_t)u;
            return;
        }
    }
}

void world_hire_crew(World *w) {
    int k = w->offer_crew;
    if (k >= CREW_COUNT || w->crew[k]) return;
    int p = world_crew_price(w, k);
    if (w->credits < p) return;
    w->credits -= p;
    w->crew[k] = 1;
    w->offer_crew = 0xFF;
}

// How badly the convoy wants a given fitting right now, given what it is short
// of and what the road ahead holds. Offers follow need, so a settlement never
// sells the answer to a question already answered.
static int upg_want(const World *w, int u) {
    int storms = 0, events = 0;
    world_road_ahead(w, &storms, &events);
    int hops = (SECTORS - 1) - w->sector;

    switch (u) {
    case UPG_ECON:   return w->held[G_FUEL]  < hops / 2 ? 5 : 1;
    case UPG_TANKS:  return w->held[G_WATER] < hops / 2 ? 5 : 1;
    case UPG_ARMOUR: return events >= 3 ? 4 : 1;
    case UPG_HOLD:   return world_cargo(w) >= world_cargo_cap(w) - 4 ? 4 : 1;
    default:         return 1;
    }
}

// Each settlement stocks at most one of each, and only things not already had.
static void roll_offers(World *w) {
    w->offer_upg = 0xFF;
    w->offer_crew = 0xFF;
    w->offer_salvaged = 0;

    int hops = (SECTORS - 1) - w->sector;

    // Late in the run it is dark, and a dark settlement has less on offer:
    // the forecourt is shut, the job board is thin, fewer hands are looking
    // for work. This is the time-of-day arc doing something rather than just
    // looking like something -- and it sharpens the run's shape, because the
    // last sectors are exactly where you can least afford to be turned away.
    int night = (w->sector * 255 / (SECTORS - 1)) > 170;

    // Sized for whichever list is longer: this scratch array is reused for
    // both, and sizing it to UPG_COUNT alone overflowed it by one on the crew
    // pass, which corrupted the stack rather than failing honestly.
    int avail[UPG_COUNT > CREW_COUNT ? UPG_COUNT : CREW_COUNT];
    int weight[UPG_COUNT > CREW_COUNT ? UPG_COUNT : CREW_COUNT];

    // Past this point nothing can repay itself, so nothing is offered. A
    // fitting dangled at the last stop is noise, not a choice.
    if (hops >= 4) {
        int n = 0, total = 0;
        for (int i = 0; i < UPG_COUNT; ++i) {
            if (w->upgrade[i]) continue;
            if (world_upg_payback(w, i) < 20) continue;   // cannot pay for itself
            weight[n] = upg_want(w, i);
            total += weight[n];
            avail[n++] = i;
        }
        // Guaranteed early: kit has to appear while there is road left for it
        // to matter on, so the first three sectors always stock something.
        int chance = (w->sector <= 2) ? 100 : (night ? 30 : 55);
        if (n && rng_range(&w->rng, 0, 99) < chance) {
            int pick = rng_range(&w->rng, 0, total - 1);
            for (int i = 0; i < n; ++i) {
                pick -= weight[i];
                if (pick < 0) { w->offer_upg = (uint8_t)avail[i]; break; }
            }
            if (w->offer_upg >= UPG_COUNT) w->offer_upg = (uint8_t)avail[n - 1];
            // Roughly half of what is on a forecourt out here is salvage.
            w->offer_salvaged = (uint8_t)(rng_range(&w->rng, 0, 99) < 50);
        }
    }

    if (hops >= 5) {
        int n = 0;
        for (int i = 0; i < CREW_COUNT; ++i) if (!w->crew[i]) avail[n++] = i;
        if (n && rng_range(&w->rng, 0, 99) < (night ? 20 : 40))
            w->offer_crew = (uint8_t)avail[rng_range(&w->rng, 0, n - 1)];
    }
}

// ---------------------------------------------------------------- contracts
int world_committed(const World *w, int good) {
    if (w->job.state != CONTRACT_TAKEN || w->job.good != good) return 0;
    return w->job.qty;
}

void world_contract_accept(World *w) {
    if (w->job.state == CONTRACT_OFFERED) w->job.state = CONTRACT_TAKEN;
}

// Called on arrival at a settlement: pay out a delivery if it can be made,
// then post a new offer if the board is empty.
static void contract_tick(World *w) {
    Contract *j = &w->job;
    w->job_paid = 0;

    if (j->state == CONTRACT_TAKEN
        && w->sector >= j->by_sector
        && w->held[j->good] >= j->qty) {
        w->held[j->good] -= j->qty;
        w->credits      += j->reward;
        w->job_paid      = j->reward;
        j->state = CONTRACT_NONE;
    }

    if (j->state != CONTRACT_NONE) return;

    // Only worth offering while there is road left to carry it down.
    int hops_left = (SECTORS - 1) - w->sector;
    if (hops_left < 3) return;
    // A dark town posts less work.
    if (rng_range(&w->rng, 0, 99) <
        (((w->sector * 255 / (SECTORS - 1)) > 170) ? 72 : 55)) return;

    int good = rng_range(&w->rng, 0, GOODS_COUNT - 1);
    int qty  = rng_range(&w->rng, 2, 5);
    int dist = rng_range(&w->rng, 2, hops_left < 6 ? hops_left : 6);

    j->good      = (uint8_t)good;
    j->qty       = (uint8_t)qty;
    j->by_sector = (uint8_t)(w->sector + dist);
    // Worth roughly double the cargo's value over a long haul. The first
    // pass paid 3.4x, which handed a starting convoy 408 credits against 150
    // of starting capital and made the rest of the economy irrelevant.
    j->reward    = (int16_t)(qty * BASE_PRICE[good] * (7 + dist * 2) / 10);
    j->state     = CONTRACT_OFFERED;
}

// ---------------------------------------------------------------- people
int world_event_char(int kind) {
    switch (kind) {
    case EV_RAID: case EV_TOLL: case EV_CHECKPOINT: return CHAR_CHIEF;
    case EV_RIVAL:                                  return CHAR_CAPTAIN;
    case EV_TRADER: case EV_SIGNAL:                 return CHAR_TRADER;
    case EV_SICK: case EV_PLAGUE:                   return CHAR_DOC;
    case EV_REFUGEE: case EV_WRECK: case EV_CACHE:  return CHAR_DRIFTER;
    default:                                        return CHAR_NONE;
    }
}

// Dealing with someone shifts how they treat you next time. Paying what is
// asked earns a little regard; refusing costs some. It is deliberately small
// per meeting -- the point is that a run accumulates a history.
static void regard_shift(World *w, int kind, int accepted) {
    int who = world_event_char(kind);
    if (who == CHAR_NONE) return;
    int r = w->regard[who] + (accepted ? 1 : -1);
    if (r >  3) r =  3;
    if (r < -3) r = -3;
    w->regard[who] = (int8_t)r;
}

// ---------------------------------------------------------------- payload
int world_payload(const World *w) { return w->payload; }

// Arriving is not succeeding. The seed stock is what the run was for, so the
// ending is graded on how much of it survived rather than on getting there.
int world_outcome(const World *w) {
    if (w->state != ST_WON) return OUT_DEAD;
    if (w->payload == 0)              return OUT_EMPTY;
    if (w->payload < PAYLOAD_SLOTS)   return OUT_PARTIAL;
    if (world_crew_count(w) > 0 || w->credits >= 80) return OUT_EXEMPLARY;
    return OUT_INTACT;
}

// ---------------------------------------------------------------- helpers
int world_cargo(const World *w) {
    int t = w->payload;      // the seed stock fills slots like anything else
    for (int g = 0; g < GOODS_COUNT; ++g) t += w->held[g];
    return t;
}

static void drop_random_cargo(World *w, int units) {
    for (int i = 0; i < units; ++i) {
        if (world_cargo(w) == 0) return;

        // Tradeable cargo goes first. Only when there is nothing else left do
        // they start on the seed stock -- which is what makes a bad raid a
        // story beat rather than an accounting entry.
        int tradeable = 0;
        for (int g = 0; g < GOODS_COUNT; ++g) tradeable += w->held[g];
        if (tradeable == 0) {
            if (w->payload > 0) {
                w->payload--;
                if (w->payload == 0) w->payload_lost_to = (uint8_t)w->state;
            }
            continue;
        }
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

    if (!(w->upgrade[UPG_ECON] && (w->day % 2) == 0)) w->held[G_FUEL]--;
    w->day++;

    // The crew drink whether or not there is anything to drink, and every
    // extra hand aboard drinks too.
    {
        int burn = world_water_burn_on(w, w->day);
        if (w->held[G_WATER] < burn) {
            w->state = ST_DEAD; w->death = DEATH_THIRST; return;
        }
        w->held[G_WATER] -= burn;
    }

    salvage_check(w);

    w->sector++;
    w->index = next_index;
    Node *nd = &w->node[w->sector][w->index];
    nd->visited = 1;

    if (nd->type == NODE_HAZARD && !w->crew[CREW_SCOUT]) {
        // A storm eats supplies on arrival, unless someone aboard knows the
        // safe line through it.
        if (w->held[G_WATER] > 0) w->held[G_WATER]--;
        if (w->held[G_FUEL]  > 0) w->held[G_FUEL]--;

        // Heat and grit get into the crates. This is the one threat to the
        // seed that cannot be paid off, argued with or fought -- without it a
        // competent convoy always arrives intact, because every other risk to
        // the payload is an encounter you can simply buy your way out of, and
        // two of the five endings are unreachable.
        if (w->payload > 0 && rng_range(&w->rng, 0, 99) < 35) w->payload--;
        if (w->held[G_WATER] == 0 && w->held[G_FUEL] == 0 && world_cargo(w) == 0) {
            w->state = ST_DEAD; w->death = DEATH_STRIPPED; return;
        }
    }

    switch (nd->type) {
    case NODE_GREEN:  w->state = ST_WON;   break;
    case NODE_SETTLE: w->state = ST_TRADE; observe_market(w); contract_tick(w);
                      roll_offers(w); break;
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
    if (w->credits < p || world_cargo(w) >= world_cargo_cap(w)) return;

    w->credits -= p;
    w->held[good]++;
    nd->price[good] = (int16_t)(p + p / 16 + 1);
}

// A 20% spread between what a stall charges and what it pays. Real markets
// have one for the same reason this needs one: otherwise the round trip is
// free money.
#define SELL_NUM 4
#define SELL_DEN 5

int world_sell_price(const World *w, int good) {
    int num = SELL_NUM, den = SELL_DEN;
    if (w->crew[CREW_TRADER]) { num = 9; den = 10; }   // haggles the spread down
    int p = w->node[w->sector][w->index].price[good] * num / den;
    return p < 1 ? 1 : p;
}

void world_sell(World *w, int good) {
    Node *nd = &w->node[w->sector][w->index];
    if (nd->type != NODE_SETTLE) return;
    if (w->held[good] < 1) return;
    // Cargo promised to a contract is not yours to sell.
    if (w->held[good] - world_committed(w, good) < 1) return;

    int p = nd->price[good];
    w->credits += world_sell_price(w, good);
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
    regard_shift(w, w->event.kind, 1);

    Event *e = &w->event;
    if (e->pay_good >= 0) w->held[e->pay_good] -= e->pay_qty;

    if (e->gain_good >= 0) {
        int room = world_cargo_cap(w) - world_cargo(w);
        int q = e->gain_qty < room ? e->gain_qty : room;
        if (q > 0) w->held[e->gain_good] += q;
    }
    w->credits += e->gain_credits;
    end_event(w);
}

void world_decline(World *w) {
    if (w->state != ST_EVENT) return;
    regard_shift(w, w->event.kind, 0);
    Event *e = &w->event;

    if (e->lose_qty > 0) {
        if (e->lose_good == -2) {
            // Straight off the payload, whatever else is aboard.
            int take = e->lose_qty;
            while (take-- > 0 && w->payload > 0) w->payload--;
        } else if (e->lose_good < 0) {
            drop_random_cargo(w, e->lose_qty);
        } else {
            int g = e->lose_good;
            w->held[g] -= e->lose_qty;
            if (w->held[g] < 0) w->held[g] = 0;
        }
    }
    end_event(w);
}

// Rolls a fresh encounter. Deeper sectors bite harder, and what the convoy
// carries changes the price: a guard settles a raid, a mechanic patches a leak
// out of his own kit. This is where crew stop being a line item and start
// being a reason the run went differently.
static void roll_event(World *w) {
    Event *e = &w->event;
    for (int i = 0; i < (int)sizeof *e; ++i) ((uint8_t *)e)[i] = 0;
    e->pay_good = e->gain_good = e->lose_good = -1;

    int depth = w->sector;
    int kind  = rng_range(&w->rng, 0, EV_KINDS - 1);
    e->kind = (uint8_t)kind;
    int hold_searched = (kind == EV_RAID || kind == EV_TOLL || kind == EV_CHECKPOINT);

    switch (kind) {
    case EV_RAID:
        e->pay_good = G_AMMO;  e->pay_qty = (int8_t)rng_range(&w->rng, 2, 3);
        e->lose_good = -1;     e->lose_qty = (int8_t)(rng_range(&w->rng, 2, 4) + depth / 3);
        if (w->crew[CREW_GUARD]) { e->pay_qty = 0; e->lose_qty /= 2; }
        if (w->upgrade[UPG_ARMOUR]) e->lose_qty = 1;
        break;

    case EV_WRECK:
        e->pay_good = G_FUEL;  e->pay_qty = 1;
        e->gain_good = G_SCRAP; e->gain_qty = (int8_t)rng_range(&w->rng, 3, 6);
        if (w->crew[CREW_MECHANIC]) e->gain_qty += 2;   // knows what is worth taking
        e->lose_qty = 0;
        break;

    case EV_SICK:
        e->pay_good = G_MEDS;  e->pay_qty = 1;
        e->lose_good = G_WATER; e->lose_qty = (int8_t)rng_range(&w->rng, 2, 3);
        if (w->crew[CREW_MEDIC]) e->pay_qty = 0;
        break;

    case EV_BREAK:
        e->pay_good = G_SCRAP; e->pay_qty = (int8_t)rng_range(&w->rng, 2, 3);
        e->lose_good = G_FUEL;  e->lose_qty = 2;
        if (w->crew[CREW_MECHANIC]) e->pay_qty = 0;
        break;

    case EV_TRADER:
        e->pay_good = G_WATER; e->pay_qty = (int8_t)rng_range(&w->rng, 2, 3);
        e->gain_credits = (int16_t)(rng_range(&w->rng, 30, 60) + depth * 8);
        if (w->crew[CREW_TRADER]) e->gain_credits += 35;
        e->lose_qty = 0;
        break;

    case EV_TOLL:
        e->pay_good = G_AMMO;  e->pay_qty = (int8_t)rng_range(&w->rng, 1, 2);
        e->lose_good = -1;     e->lose_qty = (int8_t)rng_range(&w->rng, 2, 3);
        if (w->crew[CREW_GUARD]) e->pay_qty = 0;
        if (w->upgrade[UPG_ARMOUR]) e->lose_qty = 1;
        break;

    case EV_CACHE:
        e->pay_good = G_FUEL;  e->pay_qty = 1;
        e->gain_good = (int8_t)(rng_range(&w->rng, 0, 1) ? G_AMMO : G_MEDS);
        e->gain_qty  = (int8_t)rng_range(&w->rng, 2, 4);
        if (w->crew[CREW_SCOUT]) e->gain_qty += 2;      // knows where to dig
        e->lose_qty  = 0;
        break;

    case EV_BRIDGE:
        e->pay_good = G_FUEL;  e->pay_qty = (int8_t)rng_range(&w->rng, 1, 2);
        e->lose_good = G_WATER; e->lose_qty = (int8_t)rng_range(&w->rng, 2, 4);
        if (w->crew[CREW_SCOUT])    e->pay_qty = 0;    // knows a ford
        if (w->crew[CREW_MECHANIC]) e->lose_qty /= 2;  // rigs a crossing
        break;

    case EV_RIVAL: {
        // A straight swap. Both sides think they are winning.
        int give = rng_range(&w->rng, 0, GOODS_COUNT - 1);
        int take = (give + 1 + rng_range(&w->rng, 0, GOODS_COUNT - 2)) % GOODS_COUNT;
        e->pay_good  = (int8_t)give; e->pay_qty  = (int8_t)rng_range(&w->rng, 2, 4);
        e->gain_good = (int8_t)take; e->gain_qty = (int8_t)rng_range(&w->rng, 2, 4);
        if (w->crew[CREW_TRADER]) e->gain_qty++;      // drives a bargain
        e->lose_qty = 0;
        break;
    }

    case EV_PLAGUE:
        e->pay_good = G_MEDS;  e->pay_qty = (int8_t)rng_range(&w->rng, 1, 2);
        e->lose_good = G_WATER; e->lose_qty = (int8_t)(3 + depth / 4);
        if (w->crew[CREW_MEDIC]) e->pay_qty = 0;
        break;

    case EV_CHECKPOINT:
        e->pay_good = G_AMMO;  e->pay_qty = (int8_t)rng_range(&w->rng, 1, 3);
        e->lose_good = -1;     e->lose_qty = (int8_t)rng_range(&w->rng, 3, 5);
        if (w->crew[CREW_GUARD]) { e->pay_qty = 0; e->lose_qty /= 2; }
        if (w->upgrade[UPG_ARMOUR] && e->lose_qty > 1) e->lose_qty--;
        break;

    case EV_LEAK:
        e->pay_good = G_SCRAP; e->pay_qty = 1;
        e->lose_good = G_FUEL;  e->lose_qty = (int8_t)rng_range(&w->rng, 2, 3);
        if (w->crew[CREW_MECHANIC]) e->pay_qty = 0;
        break;

    case EV_REFUGEE:
        e->pay_good = G_WATER; e->pay_qty = (int8_t)rng_range(&w->rng, 1, 3);
        e->gain_credits = (int16_t)(rng_range(&w->rng, 20, 45) + depth * 5);
        if (w->crew[CREW_MEDIC]) e->gain_credits += 30;  // tends them as they pass
        e->lose_qty = 0;
        break;

    default: // EV_SIGNAL
        e->pay_good = G_FUEL;  e->pay_qty = 1;
        e->gain_credits = (int16_t)(rng_range(&w->rng, 35, 80) + depth * 6);
        if (w->crew[CREW_TRADER]) e->gain_credits += 40;  // knows what a tip is worth
        e->lose_qty = 0;
        break;
    }

    // Someone who has dealt with you before charges accordingly. Good standing
    // shaves the price; bad standing adds to it, and to what refusing costs.
    {
        int who = world_event_char(kind);
        if (who != CHAR_NONE) {
            w->met[who]++;
            int r = w->regard[who];
            if (r > 0 && e->pay_qty  > 1) e->pay_qty--;
            if (r < 0 && e->pay_qty  > 0) e->pay_qty++;
            if (r < 0 && e->lose_qty > 0) e->lose_qty++;
            if (r > 1 && e->gain_qty > 0) e->gain_qty++;
        }
    }

    // Anyone who searches the hold past the halfway mark knows what the crates
    // are worth. This is what puts the ending at stake rather than merely the
    // accounting -- without it the seed always arrives and three of the five
    // endings are unreachable.
    if (hold_searched && depth >= (SECTORS - 1) / 3 && w->payload > 0
        && rng_range(&w->rng, 0, 99) < 45) {
        e->lose_good = -2;                        // -2 means the payload itself
        e->lose_qty  = (int8_t)rng_range(&w->rng, 1, 2);
    }
}
