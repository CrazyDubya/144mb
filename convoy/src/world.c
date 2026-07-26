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

// What can be known about a node from here.
//
// What survives the fog is what the map draws: the kind of place it is, where
// it connects, and its name. What does not is everything you would only learn
// by standing in it -- what it charges, what it has left, and what it is going
// through. The archetype tells you a well is a well; only arriving tells you
// the well has run dry.
//
// A name survives on purpose, and it is not an oversight that it is the one
// piece of "content" here. A name is not information -- it is what a rumour
// points at. "SALT CROSSING IS DRY" is a sentence someone can act on; "the node
// at 7/2 is dry" is a spreadsheet.
//
// This writes into the caller's view and never into the stored Node, so the
// world itself is unchanged and the determinism hash does not move. The fog is
// the player's, not the simulation's: the world always knew.
void world_node_known(const World *w, int s, int n, NodeView *out) {
    const Node *nd = &w->node[s][n];
    out->known     = nd->visited;
    out->type      = nd->type;
    out->archetype = nd->archetype;
    out->links     = nd->links;
    out->name      = nd->name;

    if (!nd->visited) {
        out->cond = 0;
        for (int g = 0; g < GOODS_COUNT; ++g) { out->stock[g] = 0; out->price[g] = 0; }
        return;
    }
    out->cond = nd->cond;
    for (int g = 0; g < GOODS_COUNT; ++g) {
        out->stock[g] = nd->stock[g];
        out->price[g] = nd->price[g];
    }
}

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

// Scatters a seed so that the three streams started from one number are not
// visibly related to each other, and neither are consecutive seeds. An
// xorshift state of 0 is a fixed point, so never hand one out.
static uint32_t mix32(uint32_t x) {
    x ^= x >> 16; x *= 0x7FEB352Du;
    x ^= x >> 15; x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x ? x : 1u;
}

static void roll_event(World *w);
static void observe_market(World *w);
static void contract_tick(World *w);
static void roll_offers(World *w);

// ---------------------------------------------------------------- difficulty
// Three settings. Easy is not "the same game with more credits": every field
// moves, and hard makes the payload genuinely hard to bring in whole, which is
// the difference between arriving and succeeding.
//
// This used to claim each setting leaned on a different failure -- easy and
// normal thirst-led, hard fuel-led -- and that was measured and true at the
// time. It is no longer, and the reason is worth keeping: the old asymmetry
// was substantially an artifact of the test bot's water reserve, which sampled
// a single day's parity and collapsed to two units whenever the parity was
// wrong. Against an agent that provisions both resources honestly, deaths sit
// near 50/50 on all three settings however the starting stock is skewed --
// starving water or fuel moves the win rate without moving the mix.
//
// Recorded rather than recreated. Contorting the table to reproduce a split
// that only ever existed because the observer was broken would be tuning to an
// artifact.
typedef struct {
    int16_t credits;        // starting capital
    int8_t  water, fuel;    // starting stock
    int8_t  fuel_scale;     // fuel price growth per sector, in percent
    int8_t  spoil_pct;      // chance a storm takes a crate of seed
    int8_t  storm_pct;      // share of non-settlement nodes that are storms
    int8_t  settle_pct;     // share of nodes that are settlements
} DiffRule;

static const DiffRule DIFF[DIFF_COUNT] = {
    /*                 cr   w   f  fscale  spoil  storm  settle */
    /* EASY   */ {    131, 8,  6,      1,    26,     12,     47 },
    /* NORMAL */ {    123, 8,  5,      2,    35,     16,     43 },
    /* HARD   */ {    107, 7,  5,      3,    47,     23,     36 },
};

int world_score(const World *w) {
    // Seed brought in is the point of the run, so it dominates. Credits are a
    // tiebreak, and distance is what a losing run has to show for itself.
    return w->payload * 500 + w->sector * 40 + w->credits
         + (w->state == ST_WON ? 1000 : 0);
}

// ---------------------------------------------------------------- conditions
//
// What each condition does to a market, as percentages applied to stock and to
// price. Rolled at world-gen and hidden until arrival: the world always knew.
//
// The rule that matters is the ORDER. These are applied inside stock_node,
// before the water/fuel floor -- get it backwards and COND_DRY takes a town's
// water to zero underneath the clamp, which starves runs to death while
// reading as a difficulty result. COND_DRY therefore moves price and not
// stock, for the same reason.
typedef struct {
    int8_t stock_pct[GOODS_COUNT];   // percent shift on what they will part with
    int8_t price_pct[GOODS_COUNT];
} CondRule;

static const CondRule COND[COND_COUNT] = {
/*                    stock: w    f    a    m    s        price: w    f    a    m    s   */
/* NONE     */ { {    0,   0,   0,   0,   0 }, {   0,   0,   0,   0,   0 } },
/* SIEGE    */ { {  +20, +30, -70,   0, +10 }, { -15, -20, +60, +10, -10 } },
/* SICK     */ { {    0,   0,   0, -60,   0 }, { +10,   0,   0, +70,   0 } },
/* BOOM     */ { {  +40, +40, +30, +30, +40 }, { +25, +20, +15, +15, +20 } },
/* EMPTY    */ { {  -50, -50, -40, -50, +30 }, { +20, +20,   0, +20, -30 } },
/* CARTEL   */ { {  +30, +30, +20, +20, +20 }, { +30, +30, +30, +30, +30 } },
/* DRY      */ { {    0,   0,   0,   0,   0 }, { +90,   0,   0,   0,   0 } },
};

// How often a town is going through something. Around half: much more and it
// is wallpaper, much less and most towns are still price rows.
static void cond_node(World *w, Node *n, int sector) {
    if (n->type != NODE_SETTLE) { n->cond = COND_NONE; return; }
    int r = rng_range(&w->rng_town, 0, 99);
    if (r >= 50) { n->cond = COND_NONE; return; }

    // Weighted by archetype and by how far east you are. A well going dry is a
    // story about a well; the road getting emptier the further out you go is
    // the shape of the whole run.
    int pick = rng_range(&w->rng_town, 0, 99);
    int late = sector * 100 / (SECTORS - 1);
    if (n->archetype == ARCH_WELL && pick < 30)          n->cond = COND_DRY;
    else if (n->archetype == ARCH_CLINIC && pick < 35)   n->cond = COND_SICK;
    else if (n->archetype == ARCH_ARMOURY && pick < 35)  n->cond = COND_SIEGE;
    else if (pick < 20 + late / 4)                       n->cond = COND_EMPTY;
    else if (pick < 45)                                  n->cond = COND_SIEGE;
    else if (pick < 62)                                  n->cond = COND_CARTEL;
    else if (pick < 80)                                  n->cond = COND_BOOM;
    else                                                 n->cond = COND_SICK;
}

// ---------------------------------------------------------------- setup
static void price_node(World *w, Node *n, int sector) {
    for (int g = 0; g < GOODS_COUNT; ++g) {
        // Local noise is deliberately narrower than it used to be: the
        // archetype should be the loudest signal in a price, not the dice.
        int pct = rng_range(&w->rng_map, 72, 132);
        int p = BASE_PRICE[g] * pct / 100;
        p = p * (100 + ARCH_MOD[n->archetype][g]) / 100;
        // Fuel gets dearer the further east you go, so the run gets harder to
        // afford exactly as it gets harder to survive.
        if (g == G_FUEL) p = p * (100 + sector * DIFF[w->diff].fuel_scale) / 100;
        p = p * (100 + COND[n->cond].price_pct[g]) / 100;
        n->price[g] = (int16_t)(p < 1 ? 1 : p);
    }
}

// How much a settlement will actually part with.
//
// Until now every market was an infinite tap: a well would sell you thirty
// water if you could carry it, so an archetype was a price and nothing else,
// and one stop could assemble a whole cargo. Stock makes the archetype a
// quantity too, which is what makes a route a supply chain rather than a
// shopping trip.
//
// DERIVED FROM ARCH_MOD, NOT FROM A SECOND TABLE. This project's most repeated
// bug is two tables that have to agree while only one gets edited -- the two
// price tables removed in v4, world_reachable against world_can_travel, the
// water ration copied into the bot. A place that is cheap in a thing is cheap
// because it has the thing; one number should say both.
//
//   -48 (what it makes)  -> 13 units      +24 (what it must import) -> 4
//     0 (general post)   ->  7 units      +38 (the dearest import)  -> 3
//
// The divisor is 8 and not something gentler because at /12 the mechanic did
// not bind: shelves ran dry in 6% of runs and no buy was ever refused. Measured
// per good, every exhaustion that did happen was water or fuel at a place that
// does not make them -- ammo and meds ran dry in zero runs out of 400, because
// the bot buys in bulk only through speculation, speculation targets the local
// speciality, and the speciality is the deepest shelf here by construction.
// The lever is therefore the import end of the gradient, not the whole curve.
static void stock_node(World *w, Node *n) {
    for (int g = 0; g < GOODS_COUNT; ++g) {
        int base = 7 - ARCH_MOD[n->archetype][g] / 8;
        int s = base * rng_range(&w->rng_town, 60, 140) / 100;

        // The condition, BEFORE the floor below. See the note on COND.
        s = s * (100 + COND[n->cond].stock_pct[g]) / 100;

        // The survival floor, and it must stay the LAST thing applied here --
        // conditions will bend these numbers in a later phase, and if a dry
        // town could take water to zero underneath this clamp it would starve
        // runs to death while reading as a difficulty result. No settlement on
        // any route can fail to sell a convoy its next few hops.
        //
        // Safe only because travel is forward-only, so scarcity never
        // compounds: a thin town is always followed by a fresh one. That is
        // the load-bearing reason backtracking stays out of scope.
        // Value lowered 4 -> 2; POSITION unchanged, and it must stay last.
        // At 4 this clamp was the binding number for every import shelf -- the
        // worst import base was 7-38/12 = 4, so the noise band ran 2..5 and the
        // clamp lifted nearly all of it straight back to 4. The archetype
        // gradient was being erased at exactly the end where it needed to bite,
        // which is why the mechanic shipped reading as decoration. At 1 the
        // guarantee stops being one, so 2: a thin town still covers a hop.
        if (g == G_WATER || g == G_FUEL) { if (s < 2) s = 2; }
        else if (s < 1) s = 1;

        n->stock[g] = (uint8_t)(s > 30 ? 30 : s);
    }
}

void world_init(World *w, uint32_t seed, int diff) {
    for (int i = 0; i < (int)sizeof *w; ++i) ((uint8_t *)w)[i] = 0;
    w->seed = seed ? seed : 1u;
    // Decorrelated rather than offset: consecutive seeds differ by one, and an
    // xorshift started one apart produces visibly related first draws, so
    // seeds 1 and 2 would open with similar maps and similar first encounters.
    w->rng_map   = mix32(w->seed ^ 0x9E3779B9u);
    w->rng_offer = mix32(w->seed ^ 0x85EBCA6Bu);
    w->rng_event = mix32(w->seed ^ 0xC2B2AE35u);
    w->rng_people= mix32(w->seed ^ 0x27D4EB2Fu);
    w->rng_town  = mix32(w->seed ^ 0x165667B1u);
    w->diff = (uint8_t)(diff < 0 ? 0 : (diff >= DIFF_COUNT ? DIFF_COUNT - 1 : diff));
    const DiffRule *D = &DIFF[w->diff];

    for (int s = 0; s < SECTORS; ++s) {
        int count;
        if (s == 0)                count = 1;              // the convoy starts alone
        else if (s == SECTORS - 1) count = 1;              // everything converges on the goal
        else                       count = rng_range(&w->rng_map, 2, NODES_PER);

        // Active nodes always occupy indices 0..count-1, which guarantees the
        // |n - m| <= 1 link rule below can never strand a node.
        for (int n = 0; n < count; ++n) {
            Node *nd = &w->node[s][n];
            nd->active = 1;

            if (s == 0)                     nd->type = NODE_SETTLE;
            else if (s == SECTORS - 1)      nd->type = NODE_GREEN;
            else {
                // Settlements are resupply, so their density is the single
                // biggest lever on survival; storms are the only threat to the
                // seed that cannot be bought off.
                int r = rng_range(&w->rng_map, 0, 99);
                int settle = D->settle_pct, storm = D->storm_pct;
                nd->type = (uint8_t)(r < settle              ? NODE_SETTLE :
                                     r < settle + 30         ? NODE_EVENT  :
                                     r < settle + 30 + storm ? NODE_HAZARD
                                                             : NODE_EMPTY);
            }
            // A general trading post shows up often enough to be the baseline
            // the specialists are read against.
            if (nd->type == NODE_SETTLE) {
                int r = rng_range(&w->rng_map, 0, 99);
                nd->archetype = (uint8_t)(r < 26 ? ARCH_GENERAL
                                                 : rng_range(&w->rng_map, 0, ARCH_COUNT - 2));
            } else {
                nd->archetype = ARCH_GENERAL;
            }
            // Before both, because both read it.
            cond_node(w, nd, s);
            price_node(w, nd, s);
            stock_node(w, nd);
            // A name for every node, drawn whether or not anything ever shows
            // it: rolling it unconditionally keeps the rng_town draw count the
            // same on every path, and a stream whose draw count depends on the
            // map is a stream that reshuffles itself when the map changes.
            nd->name = (uint8_t)rng_range(&w->rng_town, 0, 255);
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
    w->credits = D->credits;
    w->held[G_WATER] = D->water;
    w->held[G_FUEL]  = D->fuel;
    w->held[G_AMMO]  = 4;
    w->held[G_MEDS]  = 1;
    w->held[G_SCRAP] = 2;
    w->day     = 1;
    w->payload = PAYLOAD_SLOTS;
    w->offer_upg  = 0xFF;
    w->offer_crew = 0xFF;
    w->kit_failed = -1;
    // Minima track downwards, so they have to start above anything reachable;
    // zeroed by the memset they would report "0 water" for every run.
    INSTR(w->in.min_water = 255; w->in.min_fuel = 255);
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
//
// The fixed list prices that used to live here (upgrades 70/115/55/65, crew
// 80/95/85/75/110) are gone: nothing has read them since pricing became a
// function of remaining payback in world_upg_price / world_crew_price. They
// were left behind as unreferenced tables that read like the real prices, and
// two audits mistook them for live data.

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
    // The first hand takes a shift rather than adding a mouth.
    //
    // Measured: with crew drinking nothing at all a free guard is worth +5 and
    // a free scout +9, while a three-day ration costs about 27 points. The
    // drinking outweighed everything a specialist could do by three to five
    // times, at every ration from every-second-day to every-sixth. That is not
    // a tuning problem: a convoy carries eight to ten water for thirteen hops,
    // so one extra mouth is a 40-50% rise in water demand against a benefit
    // that fires 0.8 times a run.
    //
    // So the first hire is water-neutral -- they drive a shift, they do not
    // simply consume -- and every hand after that drinks. The first hand is a
    // real decision; a full crew is a luxury you pay for. "Hands that help and
    // mouths that drink" survives, on the second hire rather than the first.
    // Every third day, not every second.
    //
    // At alternate-day rations a hand drank about 84 credits of water over a
    // thirteen-hop run while covering roughly 0.8 encounters -- worth 23 to 38
    // credits depending on the role. Every crew member was net-negative before
    // their fee was even considered, which is why an honest bot hired nobody
    // in 400 runs per difficulty. No price could fix that: the floor is 10, so
    // even free crew lost money.
    //
    // This is the smallest change that makes the trade defensible rather than
    // arithmetically impossible. They are still mouths that drink -- just not
    // ones that cost more than they can ever save.
    // The first hand takes a shift rather than adding a mouth; every hand after
    // that drinks. One hire is a real decision, a full crew is a luxury you pay
    // for -- "hands that help and mouths that drink" lands on the second hire.
    int extra = world_crew_count(w) - 1;
    if (extra > 0 && (day % 3) == 1) burn += extra;
    // A medic runs the water discipline as well as the medicine.
    if (w->crew[CREW_MEDIC] && burn > 1) burn--;
    return burn;
}

int world_water_burn(const World *w) { return world_water_burn_on(w, w->day); }

// Whether the crew take their ration on a given day. A fact about the rules,
// exported so the bot can cost a hire without keeping its own copy of the
// schedule -- it had one, still set to alternate days, and went on valuing crew
// against a ration that had been changed underneath it.
int world_crew_drinks_on(const World *w, int day) {
    if (w->upgrade[UPG_TANKS] && (day % 2) == 0) return 0;
    // Must agree with world_water_burn_on exactly, including its first-hand
    // rule: that function charges `crew_count - 1`, so the first hand aboard
    // takes a shift rather than adding a mouth and only the second onward
    // drink. Asking this question in two places with two answers is how the
    // bot came to price hires against a ration the game does not use, which is
    // why P8 exported this function rather than leaving the schedule copied.
    if (world_crew_count(w) < 1) return 0;
    return (day % 3) == 1;
}

// What each fitting can still earn back over the hops that remain, in credits.
// Rates come from the generator: encounters are 30% of nodes across five
// kinds, so any one kind fires about 0.8 times in a 13-hop run.
#define FUEL_WORTH  22
#define WATER_WORTH 13

int world_upg_payback(const World *w, int upg) {
    int hops = (SECTORS - 1) - w->sector;
    if (hops < 1) return 0;
    // Rates from forced-policy A/Bs at n=600 on THE ROAD, where each fitting
    // was granted free and the win rate compared against a baseline of 44%:
    //
    //   ECON   +42    TANKS  +36    HOLD   +6    ARMOUR +5
    //
    // The old numbers had armour the *dearest* fitting in the game at
    // `hops * 3/5 * 20` -- the same discredited five-kinds rate that broke the
    // crew pricing -- while being worth the least of the four. Nothing had
    // ever measured them; they were instinct, and two of them were backwards.
    switch (upg) {
    case UPG_ECON:   return (hops / 2) * FUEL_WORTH;      // by far the best
    case UPG_TANKS:  return (hops / 2) * FUEL_WORTH * 4 / 5;
    case UPG_HOLD:   return hops * 3;
    case UPG_ARMOUR: return hops * 2;
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
// What a hand is worth over the road that is actually left, in credits.
//
// The rate here used to be `hops * 3 / 5`, from a comment stating that
// encounters were 30% of nodes "across five kinds". There are fourteen kinds.
// At 13 hops that formula claims 7.8 fires per role; measured over 400 runs it
// is 0.79 -- so every crew payback, and therefore every crew price, was
// overstated by roughly ten times, and by nearly eighteen for the scout. That,
// not player judgement, is why an honest bot hired nobody in 400 runs per
// difficulty.
//
// Priced against the road ahead instead of a flat rate: world_road_ahead
// counts the sectors that could hold a storm or an encounter, and since a
// convoy takes one node per sector only about 45% of them are actually met.
// A hand covers three of the fourteen kinds.
int world_crew_payback(const World *w, int crew) {
    int hops = (SECTORS - 1) - w->sector;
    if (hops < 1) return 0;

    int storms = 0, events = 0;
    world_road_ahead(w, &storms, &events);

    // Computed in one expression on purpose. Written as
    // `events * 45 / 100 * 3 / 14` it truncates to zero: a role covers about
    // 0.8 encounters over a whole run, and integer division rounds a sub-unit
    // rate away entirely, so every crew member priced at the floor regardless
    // of the road ahead.
    #define COVER(v) (events * 45 * 3 * (v) / (100 * 14))

    int gross;
    switch (crew) {
    default: gross = hops * 4; break;
    }
    (void)storms; (void)events;

    // What the extra mouth drinks over the hops remaining. Crew ration on
    // alternate days, so it is half a unit of water per hop.
    //
    // The medic's keep used to be halved here for "running the water
    // discipline too" -- but world_water_burn_on already cancels exactly the
    // medic's own thirst and nobody else's, so the same saving was counted
    // twice. And the keep was halved again whenever water tanks were fitted,
    // although tanks zero the burn on even days while crew drink on odd ones:
    // fitting tanks made every hand more expensive for a synergy that does not
    // exist. Both are gone.
    int keep = hops * WATER_WORTH / 3;   // every third day, matching the burn
    int net = gross - keep;
    return net < 0 ? 0 : net;
}

int world_crew_price(const World *w, int crew) {
    int p = world_crew_payback(w, crew) * SOUND_PCT / 100;
    if (p < 10) p = 10;
    // Turning someone who has been robbing you costs more than hiring someone
    // who already liked you.
    if (world_char_is_enemy(CHAR_OF_ROLE[crew])) p = p * 3 / 2;
    return p;
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
    INSTR(w->in.upg_bought[u]++);
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
        if (w->crew[CREW_MECHANIC]) return;   // keeps the salvage running
    if (rng_range(&w->rng_offer, 0, 99) < 4) {
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
    INSTR(w->in.crew_hired[k]++);
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
    //
    // The two counts are members of different anonymous enums, so comparing
    // them directly is a -Wenum-compare warning. Widening to int says the same
    // thing without asking the compiler to relate two unrelated types.
    #define OFFER_SLOTS ((int)UPG_COUNT > (int)CREW_COUNT \
                         ? (int)UPG_COUNT : (int)CREW_COUNT)
    int avail[OFFER_SLOTS];
    int weight[OFFER_SLOTS];

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
        if (n && rng_range(&w->rng_offer, 0, 99) < chance) {
            int pick = rng_range(&w->rng_offer, 0, total - 1);
            for (int i = 0; i < n; ++i) {
                pick -= weight[i];
                if (pick < 0) { w->offer_upg = (uint8_t)avail[i]; break; }
            }
            if (w->offer_upg >= UPG_COUNT) w->offer_upg = (uint8_t)avail[n - 1];
            INSTR(w->in.upg_offered[w->offer_upg]++);
            // Roughly half of what is on a forecourt out here is salvage.
            w->offer_salvaged = (uint8_t)(rng_range(&w->rng_offer, 0, 99) < 50);
        }
    }

    if (hops >= 3) {
        int n = 0;
        // Only people you have met and left on good terms. A board that offers
        // strangers is a board with no story on it.
        for (int i = 0; i < CREW_COUNT; ++i)
            if (!w->crew[i] && world_can_recruit(w, CHAR_OF_ROLE[i])) avail[n++] = i;
        // Draw both values unconditionally whatever the pool looks like. The
        // crew board now filters on who the convoy has actually met, so the
        // pool size varies per run -- and if the number of rng_offer draws
        // varied with it, every seed's later market offers and contracts would
        // shift and every earlier baseline would silently stop comparing.
        int roll = rng_range(&w->rng_offer, 0, 99);
        int pick = rng_range(&w->rng_offer, 0, CREW_COUNT - 1);
        if (n && roll < (night ? 40 : 70)) {
            w->offer_crew = (uint8_t)avail[pick % n];
            // Inside the branch: offer_crew is 0xFF when nobody is looking for
            // work, and indexing a five-element array at 255 is a stray write
            // into whatever follows it. AddressSanitizer caught it on the
            // first run after the counter was added.
            INSTR(w->in.crew_offered[w->offer_crew]++);
        }
    }
}

// ---------------------------------------------------------------- contracts
int world_committed(const World *w, int good) {
    if (w->job.state != CONTRACT_TAKEN || w->job.good != good) return 0;
    return w->job.qty;
}

void world_contract_accept(World *w) {
    if (w->job.state != CONTRACT_OFFERED) return;
    w->job.state = CONTRACT_TAKEN;
    INSTR(w->in.c_accepted++);
}

// Turning a job down. Until now the only thing a player could do with an offer
// was take it or walk away, and walking away was indistinguishable from not
// having looked -- so a board with a job on it that nobody wanted read exactly
// like a board with nothing to offer.
void world_contract_decline(World *w) {
    if (w->job.state != CONTRACT_OFFERED) return;
    w->job.state = CONTRACT_NONE;
    INSTR(w->in.c_declined++);
}

void world_errand_accept(World *w) {
    if (w->errand.state != ERR_OFFERED) return;
    w->errand.state = (w->errand.qty > 0) ? ERR_CARRY : ERR_VISIT;
    INSTR(w->in.err_taken++);
}

void world_errand_decline(World *w) {
    if (w->errand.state != ERR_OFFERED) return;
    w->errand.state = ERR_NONE;
}

// Cargo promised to a hand is not yours to sell, the same as contract cargo.
int world_errand_committed(const World *w, int good) {
    if (w->errand.state != ERR_CARRY || w->errand.arg != good) return 0;
    return w->errand.qty;
}

// Offers, completions and the cost of letting someone down. Runs on arrival at
// a settlement, alongside the contract board.
static void errand_tick(World *w) {
    Errand *e = &w->errand;
    const Node *nd = &w->node[w->sector][w->index];

    // Done? A visit is satisfied by standing in the right kind of place; a
    // carry by still holding what was promised when the sector comes up.
    if (e->state == ERR_VISIT && nd->archetype == e->arg) {
        e->state = ERR_DONE;
        INSTR(w->in.err_done++);
        // Paid in standing, which is what the third branch runs on.
        int who_c = CHAR_OF_ROLE[e->who];
        if (w->regard[who_c] < 3) w->regard[who_c]++;
    } else if (e->state == ERR_CARRY && w->sector >= e->by_sector
               && w->held[e->arg] >= e->qty) {
        e->state = ERR_DONE;
        INSTR(w->in.err_done++);
        int who_c = CHAR_OF_ROLE[e->who];
        if (w->regard[who_c] < 3) w->regard[who_c]++;
    }

    // Failed? Carried past the point it could have been done.
    if ((e->state == ERR_VISIT || e->state == ERR_CARRY)
        && w->sector > e->by_sector) {
        int who_c = CHAR_OF_ROLE[e->who];
        w->regard[who_c] -= 2;
        if (w->regard[who_c] < -3) w->regard[who_c] = -3;
        e->state = ERR_NONE;
        INSTR(w->in.err_failed++);
    }

    // A hand who has had enough gives notice, then goes. The warning is the
    // point: losing the manoeuvre you have come to rely on should be something
    // you saw coming and could have fixed.
    for (int k = 0; k < CREW_COUNT; ++k) {
        if (!w->crew[k]) continue;
        int who_c = CHAR_OF_ROLE[k];
        if (w->regard[who_c] <= -2) {
            if (!(w->warned & (1u << k))) { w->warned |= (uint8_t)(1u << k); continue; }
            w->crew[k] = 0;
            w->warned &= (uint8_t)~(1u << k);
            INSTR(w->in.crew_left++);
            if (w->errand.who == k && (w->errand.state == ERR_VISIT
                                       || w->errand.state == ERR_CARRY))
                w->errand.state = ERR_NONE;
        } else if (w->regard[who_c] >= 0) {
            w->warned &= (uint8_t)~(1u << k);
        }
    }

    if (e->state != ERR_NONE) return;

    // Someone aboard, on good terms, with road left to do it in.
    int cand[CREW_COUNT], n = 0;
    // Anyone aboard. Requiring regard >= 1 as well gated on goodwill they have
    // already demonstrated by agreeing to drive, and errands fired in 3% of
    // runs against a 25-60% target.
    for (int k = 0; k < CREW_COUNT; ++k) if (w->crew[k]) cand[n++] = k;
    int roll = rng_range(&w->rng_people, 0, 99);
    int pick = rng_range(&w->rng_people, 0, CREW_COUNT - 1);
    int kind = rng_range(&w->rng_people, 0, 1);
    int arch = rng_range(&w->rng_people, 0, ARCH_COUNT - 2);
    int good = rng_range(&w->rng_people, 0, GOODS_COUNT - 1);
    if (!n || w->sector > SECTORS - 5 || roll >= 18) return;

    e->who   = (uint8_t)cand[pick % n];
    e->state = ERR_OFFERED;
    if (kind == 0) { e->arg = (uint8_t)arch; e->qty = 0; }
    else           { e->arg = (uint8_t)good; e->qty = 2; }
    e->by_sector = (uint8_t)(w->sector + 5);
    INSTR(w->in.err_offered++);
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
        INSTR(w->in.c_completed++);
    }

    // A job carried well past the point it could have been delivered lapses.
    // by_sector is an *earliest* delivery point, not a deadline, so a taken
    // job that never found its cargo simply stayed taken -- and since the
    // board only posts when it is clear, one such job disabled contracts for
    // the rest of the run. With delivery at 61% of accepted, that was around
    // two runs in five ending with a permanently dead job board. The window is
    // generous: three sectors past the earliest place it could have been
    // handed over.
    if (j->state == CONTRACT_TAKEN && w->sector > j->by_sector + 2) {
        j->state = CONTRACT_NONE;
        INSTR(w->in.c_forfeit++);
    }

    if (j->state != CONTRACT_NONE) return;

    // Only worth offering while there is road left to carry it down.
    // The last settlement is at SECTORS-2: the final sector is the Green Zone,
    // which has no market, so contract_tick never runs there. A deadline of
    // SECTORS-1 was therefore a job that could not be delivered at all, and at
    // sector 10 half of all offers were generated that way.
    int hops_left = (SECTORS - 2) - w->sector;
    if (hops_left < 3) return;
    // A dark town posts less work.
    if (rng_range(&w->rng_offer, 0, 99) <
        (((w->sector * 255 / (SECTORS - 1)) > 170) ? 72 : 55)) return;

    int good = rng_range(&w->rng_offer, 0, GOODS_COUNT - 1);
    int qty  = rng_range(&w->rng_offer, 2, 5);
    int dist = rng_range(&w->rng_offer, 2, hops_left < 6 ? hops_left : 6);

    j->good      = (uint8_t)good;
    j->qty       = (uint8_t)qty;
    j->by_sector = (uint8_t)(w->sector + dist);
    INSTR(w->in.c_offered++);
    // Worth roughly double the cargo's value over a long haul. The first
    // pass paid 3.4x, which handed a starting convoy 408 credits against 150
    // of starting capital and made the rest of the economy irrelevant.
    j->reward    = (int16_t)(qty * BASE_PRICE[good] * (7 + dist * 2) / 10);
    j->state     = CONTRACT_OFFERED;
}

// ---------------------------------------------------------------- people
// Whether an encounter is something being taken from you or something being
// offered. The UI frames it red or green, and the generator uses it to keep
// market meetings friendlier than roadside ones.
int world_event_is_threat(int kind) {
    switch (kind) {
    case EV_WRECK: case EV_CACHE: case EV_RIVAL:
    case EV_TRADER: case EV_REFUGEE: case EV_SIGNAL:
        return 0;
    default:
        return 1;
    }
}

// Which hand knows this kind of trouble. The same three-per-role grouping the
// ability conditionals inside roll_event already imply, written down once so
// the alt branch and the bot agree with the switch instead of drifting from it.
const signed char CHAR_OF_ROLE[CREW_COUNT] = {
    CHAR_CAPTAIN,   // MECHANIC -- Marlow, after her rig dies
    CHAR_CHIEF,     // GUARD    -- Vulture already owns raids and tolls
    CHAR_DOC,       // MEDIC    -- Sister Rae already owns sickness
    CHAR_DRIFTER,   // SCOUT    -- the Walker knows the ground
    CHAR_TRADER,    // TRADER   -- Okonjo already owns the deals
};
const signed char ROLE_OF_CHAR[CHAR_COUNT] = {
    CREW_GUARD,     // CHIEF
    CREW_MECHANIC,  // CAPTAIN
    CREW_TRADER,    // TRADER
    CREW_MEDIC,     // DOC
    CREW_SCOUT,     // DRIFTER
};

// The two who take from you on the road. Recruiting them is meant to be a
// story, not a transaction.
int world_char_is_enemy(int who) {
    return who == CHAR_CHIEF || who == CHAR_CAPTAIN;
}

// Whether this person would drive for you.
//
// The gate is met at least once and regard at least +1 -- and both halves of
// that were measured before being chosen. The gate originally planned, met
// twice and regard +2, passed in 15% of runs against a 60-80% target, because
// characters are met 0.58 times a run and regard barely moves. This one passes
// in 65%.
int world_can_recruit(const World *w, int who) {
    if (who < 0 || who >= CHAR_COUNT) return 0;
    int role = ROLE_OF_CHAR[who];
    if (w->crew[role]) return 0;
    // Someone you have met and are not on bad terms with will drive for you;
    // someone who has been robbing you has to be actively won over.
    //
    // Friends were gated at regard >= 1 and that measured 7% of runs ending
    // with anyone aboard, because regard is volatile -- it rises only on an
    // acceptance that cost something and falls on every refusal, so few people
    // are holding a positive number at the moment an offer happens to roll.
    // Raising the offer rate did not move it and neither did widening the
    // window, which is how the gate was identified as the constraint.
    //
    // "Not disliked" is still a choice: refuse someone twice and they will not
    // ride with you.
    int need = world_char_is_enemy(who) ? 1 : 0;
    return w->met[who] >= 1 && w->regard[who] >= need;
}

int world_event_role(int kind) {
    switch (kind) {
    case EV_BREAK: case EV_LEAK: case EV_WRECK:      return CREW_MECHANIC;
    case EV_RAID:  case EV_TOLL: case EV_CHECKPOINT: return CREW_GUARD;
    case EV_SICK:  case EV_PLAGUE: case EV_REFUGEE:  return CREW_MEDIC;
    case EV_BRIDGE: case EV_CACHE:                   return CREW_SCOUT;
    case EV_TRADER: case EV_SIGNAL: case EV_RIVAL:   return CREW_TRADER;
    default:                                         return -1;
    }
}

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
// Standing moves with what a choice cost you, not with which key you pressed.
//
// Accepting is usually correct anyway, so goodwill used to accumulate as a free
// byproduct of playing well and the ±3 range never meant anything. Worse, it
// was a loop: positive regard already zeroes pay_qty in roll_event, so a hand
// that made encounters free went on earning goodwill from the encounters it had
// made free. Paying nothing now earns nothing.
static void regard_shift(World *w, int kind, int accepted) {
    int who = world_event_char(kind);
    if (who == CHAR_NONE) return;

    int d;
    if (!accepted)                    d = -1;
    else if (w->event.pay_qty > 0)    d = +1;   // it cost you something
    else                              d =  0;   // free to accept, so worth nothing

    // At most one step per person per sector, so no single stop can swing a
    // relationship. They appear about once a sector today, which makes this
    // free insurance now and a stated invariant before errands add more
    // sources of regard.
    if (d != 0 && w->regard_moved[who] == (uint8_t)(w->sector + 1)) return;
    if (d != 0) w->regard_moved[who] = (uint8_t)(w->sector + 1);

    int r = w->regard[who] + d;
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
                INSTR(w->in.pl_random++);
            }
            continue;
        }
        // Walk from a random start so losses spread across goods.
        int start = rng_range(&w->rng_event, 0, GOODS_COUNT - 1);
        for (int k = 0; k < GOODS_COUNT; ++k) {
            int g = (start + k) % GOODS_COUNT;
            if (w->held[g] > 0) { w->held[g]--; break; }
        }
    }
}

// Which nodes the convoy may drive to from here.
//
// One function, because there are two callers that must agree: world_reachable
// offers the choice and world_can_travel enforces it. Widening one and not the
// other let the bot select a node the game then refused to move to -- it
// pressed travel, nothing happened, and the scout measured -37 points.
uint8_t world_links(const World *w) {
    uint8_t links = w->node[w->sector][w->index].links;
    // A scout knows ways through that are not on the road. This replaces the
    // storm negation as the scout's headline: that was worth -19 points,
    // because it insured against a hazard competent routing already avoids for
    // free. An extra option every hop makes the routing itself cheaper, which
    // is what good play was doing by hand.
    if (w->crew[CREW_SCOUT]) links = 0x0F;
    return links;
}

int world_reachable(const World *w, int *out) {
    int n = 0;
    if (w->sector >= SECTORS - 1) return 0;
    uint8_t links = world_links(w);
    for (int m = 0; m < NODES_PER; ++m)
        if ((links & (1u << m)) && w->node[w->sector + 1][m].active) out[n++] = m;
    return n;
}

// Whether the next hop actually burns a unit. The economiser makes every
// second day free, and world_can_travel used to demand a unit in the hold
// regardless -- so a convoy with the economiser fitted, no fuel, on a free
// day was declared stranded and the run ended, on the one fitting sold as
// insurance against exactly that.
int world_hop_costs_fuel(const World *w) {
    return !(w->upgrade[UPG_ECON] && (w->day % 2) == 0);
}

int world_can_travel(const World *w, int next_index) {
    if (w->sector >= SECTORS - 1) return 0;
    if (next_index < 0 || next_index >= NODES_PER) return 0;
    if (!w->node[w->sector + 1][next_index].active) return 0;
    if (!(world_links(w) & (1u << next_index))) return 0;
    return !world_hop_costs_fuel(w) || w->held[G_FUEL] >= 1;
}

// ---------------------------------------------------------------- services
//
// GENERAL deliberately has none. It is the baseline the specialists are read
// against -- the same job its all-zeros row in ARCH_MOD does for prices -- and
// a trading post that also did something would leave nothing to compare to.

int world_service_kind(const World *w) {
    const Node *nd = &w->node[w->sector][w->index];
    if (nd->type != NODE_SETTLE) return SVC_NONE;
    if (w->svc_used) return SVC_NONE;
    if (nd->archetype == ARCH_GENERAL) return SVC_NONE;

    // A trade nobody needs is not on offer. This is the guard against the
    // failure v4 recorded about the garage and the crew board -- "furniture
    // nobody could afford: across five full bot runs, not one purchase" --
    // arriving from the other side: furniture nobody has any use for.
    switch (nd->archetype) {
    // Two modes, because one of them almost never comes up. A refit needs
    // something to have broken, which happens 0.03 times a run at a scrapyard
    // -- furniture, by this project's own standard. When nothing is broken they
    // will at least pay properly for metal, which is a reason to route here
    // that does not depend on bad luck having already happened.
    case ARCH_SCRAPYARD: if (w->kit_failed < 0 && w->held[G_SCRAP] < 2)
                             return SVC_NONE;
                         break;
    case ARCH_CLINIC:    if (world_crew_count(w) == 0)   return SVC_NONE; break;
    case ARCH_WELL:      if (w->held[G_WATER] >= 12)     return SVC_NONE; break;
    case ARCH_REFINERY:  if (w->held[G_SCRAP] < 2)       return SVC_NONE; break;
    case ARCH_ARMOURY:   if (w->escort > 0)              return SVC_NONE; break;
    default: break;
    }
    return nd->archetype;
}

int world_service_price(const World *w) {
    const Node *nd = &w->node[w->sector][w->index];
    switch (nd->archetype) {
    // Under the market, and that is the whole offer: the well is where water is
    // cheap, so the thing it does for you is sell it cheaper still.
    // Priced at a modest return, not a good one.
    //
    // First cut charged 1.5x list for four water and turned two scrap into
    // three fuel -- a 2.7x and a 1.75x return on the two goods that decide
    // whether a run ends, and NORMAL went from 50% to 68%. Water kills seven
    // out of ten convoys, so a cheap reliable source of it is not a service,
    // it is a difficulty setting.
    case ARCH_WELL:      return nd->price[G_WATER] * 7 / 2;
    case ARCH_REFINERY:  return nd->price[G_FUEL];
    case ARCH_CLINIC:    return nd->price[G_MEDS];
    // Free if the mechanic is aboard -- they do the work, the yard supplies
    // the metal.
    //
    // Without this the yard SUBSTITUTES for the role instead of complementing
    // it: putting broken kit right is precisely what a mechanic is for, so
    // adding a shop that does it for money took the mechanic from +11 in v5 to
    // +3 on a granted-free A/B, below the +8 bar that release set. A service
    // that quietly makes a crew role redundant is a worse outcome than no
    // service, and the fix is to make having the hand change what the shop
    // costs rather than to weaken the shop.
    case ARCH_SCRAPYARD:
        if (w->kit_failed < 0) return 0;
        return w->crew[CREW_MECHANIC] ? 0 : nd->price[G_SCRAP] * 3;
    case ARCH_ARMOURY:   return nd->price[G_AMMO] * 2;
    default:             return 0;
    }
}

// Applies the forced-policy service, if the harness asked for one. Called on
// arrival so the grant lands before the bot does anything, exactly as -U and -C
// are applied after the title -- set any earlier and world_init zeroes it, a
// mistake that once cost an entire armour A/B.
void world_service_forced(World *w) {
    if (!w->svc_forced) return;
    const Node *nd = &w->node[w->sector][w->index];
    if (nd->type != NODE_SETTLE) return;
    if (nd->archetype != w->svc_forced - 1) return;
    if (world_service_kind(w) == SVC_NONE) return;
    int keep = w->credits;
    world_service(w);
    w->credits = keep;      // granted, not sold
}

int world_can_service(const World *w) {
    int k = world_service_kind(w);
    if (k == SVC_NONE) return 0;
    if (w->credits < world_service_price(w)) return 0;
    if (k == ARCH_WELL && world_cargo(w) >= world_cargo_cap(w)) return 0;
    if (k == ARCH_REFINERY && w->held[G_SCRAP] < 2) return 0;
    return 1;
}

void world_service(World *w) {
    if (!world_can_service(w)) return;
    int k = world_service_kind(w);
    w->credits -= world_service_price(w);
    w->svc_used = 1;
    INSTR(w->in.svc_used[k]++);

    switch (k) {
    case ARCH_WELL: {
        // Up to four, and never past the hold.
        int room = world_cargo_cap(w) - world_cargo(w);
        int n = room < 3 ? room : 3;
        w->held[G_WATER] += n;
        break;
    }
    case ARCH_REFINERY:
        w->held[G_SCRAP] -= 2;
        w->held[G_FUEL]  += 2;
        break;
    case ARCH_CLINIC: {
        // Squares you with whoever is closest to walking. The desertion warning
        // has existed since v5 with nothing a player could do about it, which
        // makes it an announcement rather than a warning.
        int worst = -1;
        for (int r = 0; r < CREW_COUNT; ++r) {
            if (!w->crew[r]) continue;
            int c = CHAR_OF_ROLE[r];
            if (worst < 0 || w->regard[c] < w->regard[worst]) worst = c;
        }
        if (worst >= 0 && w->regard[worst] < 3) w->regard[worst]++;
        w->warned = 0;
        break;
    }
    case ARCH_SCRAPYARD:
        // No spread on metal here. The 20% bid-ask exists so the round trip is
        // not free money; a scrapyard paying list for scrap is the one place
        // that is not true, which is what makes it worth the detour.
        if (w->kit_failed < 0) {
            // Never the last of it. Selling a convoy's whole scrap stock is
            // how a breakdown two hops later stops being a decision.
            int spare = w->held[G_SCRAP] - 3;
            int n = spare < 0 ? 0 : (spare < 4 ? spare : 4);
            w->held[G_SCRAP] -= n;
            w->credits += n * w->node[w->sector][w->index].price[G_SCRAP];
            break;
        }
        // The counterplay salvaged kit never had: it broke, and that was that.
        if (w->kit_failed >= 0) {
            w->upgrade[w->kit_failed]     = 1;
            w->upg_salvaged[w->kit_failed] = 0;   // properly refitted, not patched
            w->kit_failed = -1;
        }
        break;
    case ARCH_ARMOURY:
        // Four hops, not three. At three the escort covered well under one
        // expected encounter -- they fire about 3.7 times across a fourteen
        // sector run -- so it was priced against a saving that usually never
        // arrived, and was taken in 12% of runs against a 15% bar.
        w->escort = 4;
        break;
    default: break;
    }
}

// The one thing you can do about what this town is going through.
//
// Built as an Event, and rolled on arrival rather than stored per node -- no
// Event on every Node, and correct under the fog for free, since a situation
// nobody has walked into has not happened yet.
//
// Drawn entirely from rng_town. The four older streams must not move: an extra
// draw in rng_event here would reshuffle every later encounter for the seed and
// the numbers would still look plausible.
void world_situation_enter(World *w) {
    Node *nd = &w->node[w->sector][w->index];
    if (nd->cond == COND_NONE || w->sit_done) return;
    Event *e = &w->event;
    for (unsigned i = 0; i < sizeof *e; ++i) ((uint8_t *)e)[i] = 0;
    e->gain_good = -1; e->lose_good = -1; e->alt_who = -1;

    int deep = w->sector * 100 / (SECTORS - 1);
    switch (nd->cond) {
    case COND_SIEGE:    // they need ammo and will pay for it
        e->kind = EV_TOLL;
        e->pay_good = G_AMMO;  e->pay_qty = 2;
        e->gain_credits = 20 + deep / 6;
        e->lose_good = G_WATER; e->lose_qty = 1;
        break;
    case COND_SICK:     // meds, or they will not let you near the well
        e->kind = EV_PLAGUE;
        e->pay_good = G_MEDS;  e->pay_qty = 1;
        e->gain_credits = 8 + deep / 6;
        e->lose_good = G_WATER; e->lose_qty = 2;
        break;
    case COND_BOOM:     // everyone is buying; sell into it
        e->kind = EV_TRADER;
        e->pay_good = G_FUEL;  e->pay_qty = 1;
        e->gain_credits = 26 + deep / 5;
        e->lose_good = -1;     e->lose_qty = 2;
        break;
    case COND_EMPTY:    // pick the place over
        e->kind = EV_CACHE;
        e->pay_good = G_FUEL;  e->pay_qty = 1;
        e->gain_good = G_MEDS; e->gain_qty = 1;
        e->lose_good = -1;     e->lose_qty = 0;
        break;
    case COND_CARTEL:   // pay the family, or they take a cut anyway
        e->kind = EV_CHECKPOINT;
        e->pay_good = G_AMMO;  e->pay_qty = 1;
        e->gain_credits = 12;
        e->lose_good = -1;     e->lose_qty = 2;   // random cargo
        break;
    case COND_DRY:      // haul water up for them
        e->kind = EV_REFUGEE;
        e->pay_good = G_WATER; e->pay_qty = 2;
        e->gain_credits = 70 + deep / 3;
        e->lose_good = -1;     e->lose_qty = 1;
        break;
    default: return;
    }

    // The third branch, on the same terms as any encounter: whoever is aboard
    // offers to try it their way. Matched to the trouble is free and likely;
    // anyone else improvises for a unit and worse odds.
    {
        int want = world_event_role(e->kind);
        int who = -1, base = 0, matched = 0;
        if (want >= 0 && w->crew[want]) { who = want; base = 80; matched = 1; }
        else for (int k = 0; k < CREW_COUNT; ++k)
                 if (w->crew[k]) { who = k; base = 60; break; }
        if (who >= 0) {
            e->alt_who      = (int8_t)who;
            e->alt_pay_good = matched ? -1 : e->pay_good;
            e->alt_pay_qty  = matched ?  0 : 1;
            int odds = base + rng_range(&w->rng_town, 0, 10) - 5;
            e->alt_odds = (uint8_t)(odds < 35 ? 35 : (odds > 85 ? 85 : odds));
        }
    }

    w->sit_done = 1;
    // Count it as a firing of its kind, which is what it is.
    //
    // Without this the -K table is not merely incomplete, it is WRONG:
    // world_accept and world_decline increment ev_accepted and ev_forced for
    // the kind regardless of where the encounter came from, so situation
    // outcomes were being divided by the random-encounter count alone. The
    // table reported accept 312% and refuse -212% for the reused kinds. A
    // percentage over a hundred is the instrument saying it is broken; the
    // danger is the ones that stay under a hundred and look like results.
    INSTR(w->in.sit_entered++; w->in.ev_fired[e->kind]++);
    w->after_event = ST_TRADE;
    w->state = ST_EVENT;
}

// ---------------------------------------------------------------- word
//
// A rumour may only claim what the fog hides. The map already draws node type
// and archetype, so "there is a storm at X" is not information, it is the map
// read aloud -- and a claim set that duplicates the map is noise wearing a hat.
// So: price, stock, condition, and which kind of trouble an encounter node
// holds. All four are things you would otherwise only learn by arriving.

// How much the teller is worth listening to. Honest, and shown honestly.
//
// This is where v5's regard finally reads its full range rather than its sign.
// A hand aboard who thinks well of you and knows the subject is near certain;
// a stranger in a room is a coin flip and a bit, which is worth exactly as much
// as it sounds.
static int rumour_conf(const World *w, int src, int claim, int dist) {
    int conf = 40;                                   // a stranger
    if (src >= 0) {
        int role = ROLE_OF_CHAR[src];
        if (role >= 0 && w->crew[role]) conf += 15;  // aboard
        conf += w->regard[src] * 8;                  // -24 .. +24

        // Standing on the subject. A scout knows roads, a trader knows prices.
        int domain = -1;
        switch (claim) {
        case CL_ROAD:  domain = CREW_SCOUT;    break;
        case CL_PRICE: domain = CREW_TRADER;   break;
        case CL_STOCK: domain = CREW_TRADER;   break;
        case CL_COND:  domain = CREW_MEDIC;    break;
        default: break;
        }
        if (domain >= 0 && role == domain) conf += 20;
    }
    // Your own market experience helps you read a claim about prices. Costs no
    // new state: seen_n is the same running sample world_price_bias uses.
    if (claim == CL_PRICE && w->seen_n[0] >= 4) conf += 5;
    if (dist == 2) conf -= 12;                       // two sectors out is hearsay

    // Floored at 35, not lower. A claim that is right one time in five is
    // noise, and noise teaches a player to ignore the whole system including
    // the parts that were true.
    return conf < 35 ? 35 : (conf > 92 ? 92 : conf);
}

// Somebody talks, at most once a stop.
static void rumour_tick(World *w) {
    if (rng_range(&w->rng_town, 0, 99) >= 55) return;

    // Who is talking. Someone aboard, else someone met, else the room.
    int src = -1, pool[CHAR_COUNT], n = 0;
    for (int i = 0; i < CHAR_COUNT; ++i) {
        int role = ROLE_OF_CHAR[i];
        if (role >= 0 && w->crew[role]) pool[n++] = i;
    }
    if (!n) for (int i = 0; i < CHAR_COUNT; ++i) if (w->met[i]) pool[n++] = i;
    int pick = rng_range(&w->rng_town, 0, CHAR_COUNT - 1);   // drawn regardless
    if (n) src = pool[pick % n];

    // Which node it is about: one or two sectors on.
    int dist = 1 + rng_range(&w->rng_town, 0, 1);
    int s = w->sector + dist;
    int idx = rng_range(&w->rng_town, 0, NODES_PER - 1);
    // Weighted, not uniform, and this is the correction for a mistake made in
    // this very file's design note.
    //
    // "A rumour may only claim what the fog hides" -- and then price and stock
    // claims were drawn as often as the rest. But the archetype is drawn on the
    // map, a well IS where water is cheap, and score_node already prefers a
    // well when the tank is low. Telling anyone that water is cheap at the well
    // is the map read aloud. Forcing every rumour true and then every rumour
    // false moved the win rate by exactly zero points, three arms identical to
    // the digit, because most of what was being said was already known.
    //
    // The informative claims are the ones the archetype does NOT imply: a well
    // that has gone dry, a boom town, which trouble is sitting on an event
    // node. Those get 70% of the weight between them.
    int croll = rng_range(&w->rng_town, 0, 99);
    int claim = croll < 40 ? CL_COND
              : croll < 70 ? CL_ROAD
              : croll < 88 ? CL_STOCK : CL_PRICE;
    int truth_roll = rng_range(&w->rng_town, 0, 99);
    int argroll = rng_range(&w->rng_town, 0, 99);
    if (s >= SECTORS - 1) return;
    const Node *nd = &w->node[s][idx];
    if (!nd->active) return;

    // A claim has to be about something that could be true of that node.
    if ((claim == CL_PRICE || claim == CL_STOCK || claim == CL_COND)
        && nd->type != NODE_SETTLE) claim = CL_ROAD;
    if (claim == CL_ROAD && nd->type != NODE_EVENT) {
        if (nd->type != NODE_SETTLE) return;
        claim = CL_PRICE;
    }

    int conf = rumour_conf(w, src, claim, dist);
    int truth = truth_roll < conf;
    if (w->rum_force == 1) truth = 1;
    else if (w->rum_force == 2) truth = 0;

    // The argument. Generate the SLOT first, then read the truth -- and when
    // lying, emit a different value for that same slot.
    //
    // This is the most important line in the phase. If false rumours came from
    // their own path they would acquire a shape, and a player who learns the
    // shape has a perfect oracle with extra steps. A lie has to be the sort of
    // thing the truth would have been.
    // A claim is true when it is true OF THE NODE. That sounds obvious and the
    // first version did not do it: `truth` was rolled and stored, and then the
    // argument was drawn at random anyway, so a price claim was neither right
    // nor wrong about anything. The oracle arms came back identical to the
    // digit -- all-true, honest and all-false all 39% -- because there was
    // nothing for perfect information to be perfect about.
    //
    // So the slot is chosen first (which node, which kind of claim), and then
    // the argument is picked to make the claim hold or fail against the real
    // node. A lie is the sort of thing the truth would have been, which is what
    // stops false rumours acquiring a shape a player could learn.
    // Claims lean hard on water and fuel, because those are what runs turn on:
    // seven of ten convoys die of thirst and the rest are stranded. The first
    // version drew the good uniformly, so most rumours were about ammo or meds
    // -- true or false, they changed nothing, and a perfect oracle measured
    // zero points. Information is only worth something when it is about the
    // thing that kills you.
    int surv = (argroll % 100) < 70;
    int g0 = surv ? (argroll & 1 ? G_WATER : G_FUEL) : (argroll % GOODS_COUNT);

    int arg = 0;
    switch (claim) {
    case CL_PRICE: {
        // "They are cheap in X." True when the node really is under the mean
        // the convoy has seen; false when it is not.
        int want = -1, fallback = -1;
        for (int t = 0; t < GOODS_COUNT; ++t) {
            int g = (g0 + t) % GOODS_COUNT;
            int mean = w->seen_n[g] > 0 ? (int)(w->seen_sum[g] / w->seen_n[g]) : 0;
            if (mean <= 0) { if (fallback < 0) fallback = g; continue; }
            int cheap = nd->price[g] * 100 < mean * 90;
            if (cheap == truth) { want = g; break; }
        }
        if (want < 0) want = fallback >= 0 ? fallback : (argroll % GOODS_COUNT);
        arg = want;
        break;
    }
    case CL_STOCK: {
        // "They have X to spare." True when the shelf is deep, false when thin.
        int want = -1;
        for (int t = 0; t < GOODS_COUNT; ++t) {
            int g = (g0 + t) % GOODS_COUNT;
            int deep = nd->stock[g] >= 6;
            if (deep == truth) { want = g; break; }
        }
        arg = want < 0 ? (argroll % GOODS_COUNT) : want;
        break;
    }
    case CL_COND:
        arg = truth ? nd->cond
                    : (uint8_t)(1 + (nd->cond + 1 + argroll % (COND_COUNT - 1))
                                    % (COND_COUNT - 1));
        break;
    case CL_ROAD:
        arg = truth ? EV_RAID : (uint8_t)(argroll % EV_KINDS);
        break;
    default: break;
    }

    Rumour r;
    r.sector = (uint8_t)s; r.index = (uint8_t)idx;
    r.claim = (uint8_t)claim; r.arg = (uint8_t)arg;
    r.src = (int8_t)src; r.conf = (uint8_t)conf; r.truth = (uint8_t)truth;

    // FIFO, four slots. You cannot bank a map.
    if (w->heard_n < RUMOUR_SLOTS) w->heard[w->heard_n++] = r;
    else {
        for (int i = 1; i < RUMOUR_SLOTS; ++i) w->heard[i - 1] = w->heard[i];
        w->heard[RUMOUR_SLOTS - 1] = r;
    }
    INSTR(w->in.rum_offered++;
          if (truth) w->in.rum_true++;
          { int band = conf >= 70 ? 0 : (conf >= 45 ? 1 : 2);
            w->in.rum_band[band]++;
            if (truth) w->in.rum_band_true[band]++; });
}

// Drop anything about a sector already behind you. Forward-only travel makes
// expiry free -- there is nothing to age, only something to pass.
static void rumour_expire(World *w) {
    int k = 0;
    for (int i = 0; i < w->heard_n; ++i)
        if (w->heard[i].sector > w->sector) w->heard[k++] = w->heard[i];
    w->heard_n = (uint8_t)k;
}

// What the convoy has been told about a node, or NULL.
const Rumour *world_rumour_for(const World *w, int s, int n) {
    for (int i = 0; i < w->heard_n; ++i)
        if (w->heard[i].sector == s && w->heard[i].index == n)
            return &w->heard[i];
    return 0;
}

// ---------------------------------------------------------------- travel
void world_travel(World *w, int next_index) {
    if (!world_can_travel(w, next_index)) {
        // Out of fuel with nowhere to go: the run ends here.
        if (world_hop_costs_fuel(w) && w->held[G_FUEL] < 1) {
            w->state = ST_DEAD; w->death = DEATH_STRANDED;
        }
        return;
    }

    if (world_hop_costs_fuel(w)) w->held[G_FUEL]--;
    if (w->escort > 0) w->escort--;
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

    // An offer the convoy drove away from lapses. It used to persist forever,
    // and since contract_tick early-returns while a job is on the board, one
    // ignored offer disabled the contract system for the rest of the run --
    // measured, 1.48 offers per run against 13 settlements visited.
    if (w->job.state == CONTRACT_OFFERED) {
        w->job.state = CONTRACT_NONE;
        INSTR(w->in.c_lapsed++);
    }

    w->svc_used = 0;    // a new town, a new favour to ask of it
    w->sit_done = 0;
    rumour_expire(w);
    salvage_check(w);

    // Sampled once per hop rather than per keypress, so the mean is over the
    // journey and not over how long the convoy stood in each market.
    INSTR({
        int c = world_cargo(w);
        w->in.cargo_sum += (uint32_t)c;
        w->in.cargo_samples++;
        if (c > w->in.peak_cargo) w->in.peak_cargo = (uint16_t)c;
        if (w->held[G_WATER] < w->in.min_water) w->in.min_water = (uint8_t)w->held[G_WATER];
        if (w->held[G_FUEL]  < w->in.min_fuel)  w->in.min_fuel  = (uint8_t)w->held[G_FUEL];
        if (w->held[G_WATER] <= 2 || w->held[G_FUEL] <= 2) w->in.days_thin++;
        w->in.stack_here = 0;
        for (int c = 0; c < CHAR_COUNT; ++c) {
            w->in.char_regard_end[c] = w->regard[c];
            // The gate Phase 2 will use, recorded now so its supply can be
            // measured before it is designed.
            if (world_can_recruit(w, c)) w->in.char_recruit[c] = 1;
            if (w->met[c] >= 1 && w->regard[c] >= 1) w->in.gate_any[0] = 1;
            if (w->met[c] >= 2 && w->regard[c] >= 1) w->in.gate_any[1] = 1;
            if (w->met[c] >= 1 && w->regard[c] >= 2) w->in.gate_any[2] = 1;
            if (w->met[c] >= 2 && w->regard[c] >= 2) w->in.gate_any[3] = 1;
        }
    });

    w->sector++;
    w->index = next_index;
    Node *nd = &w->node[w->sector][w->index];
    nd->visited = 1;

    if (nd->type == NODE_HAZARD && !w->crew[CREW_SCOUT]) {
        // A storm eats supplies on arrival, unless someone aboard knows the
        // safe line through it.
        // A medic rations the convoy through a blow. Their existing water
        // discipline only ever cancelled their own thirst, which is why a free
        // medic measured at +4 -- the smallest positive in the game.
        // What a storm takes, and who keeps it. A medic rations the water; a
        // guard lashes the load down so the fuel stays aboard.
        //
        // Both passives are deliberately OUTSIDE the encounter tables. The
        // guard's first version discounted what a threat cost to settle, which
        // measured well but crowded out the guard's own third branch -- the
        // convoy simply paid instead, and the alt was chosen 5% of the time.
        // A passive that competes with the same hand's manoeuvre is one
        // feature fighting another.
        if (!w->crew[CREW_MEDIC] && w->held[G_WATER] > 0) w->held[G_WATER]--;
        if (!w->crew[CREW_GUARD] && w->held[G_FUEL]  > 0) w->held[G_FUEL]--;

        // Heat and grit get into the crates. This is the one threat to the
        // seed that cannot be paid off, argued with or fought -- without it a
        // competent convoy always arrives intact, because every other risk to
        // the payload is an encounter you can simply buy your way out of, and
        // two of the five endings are unreachable.
        if (w->payload > 0 && rng_range(&w->rng_event, 0, 99) < DIFF[w->diff].spoil_pct) {
            w->payload--;
            INSTR(w->in.pl_storm++);
        }
        if (w->held[G_WATER] == 0 && w->held[G_FUEL] == 0 && world_cargo(w) == 0) {
            w->state = ST_DEAD; w->death = DEATH_STRIPPED; return;
        }
    }

    switch (nd->type) {
    case NODE_GREEN:  w->state = ST_WON;   break;
    case NODE_SETTLE:
        w->state = ST_TRADE; observe_market(w); contract_tick(w);
        errand_tick(w); roll_offers(w); world_service_forced(w);
        // Somebody talks. Listening is free and automatic -- there is no key to
        // press, because the decision a rumour asks for is not "do I listen"
        // but "do I believe", and belief is expressed by routing. It also keeps
        // the measured quantity honest: a take rate that is 100% by
        // construction measures nothing.
        rumour_tick(w);
        // Someone is waiting for you in the market. Encounters used to live
        // only on encounter nodes, which put the story in direct competition
        // with trading: the profitable route is the one through settlements,
        // so a measured run saw 1.76 encounters across fourteen sectors and
        // most of the cast never appeared at all. Meeting people where the
        // people are costs the player nothing to opt into.
        if (rng_range(&w->rng_event, 0, 99) < 14) {
            w->after_event = ST_TRADE;
            w->state = ST_EVENT; roll_event(w);
            // Raids happen on the road; deals happen in town. Re-rolling a
            // threat once halves the threat share of settlement meetings
            // without touching the road mix. Straight reuse of the road table
            // cost twelve points of win rate, which is not a fair price for
            // walking into a market.
            if (world_event_is_threat(w->event.kind)) roll_event(w);
            if (w->encounters < 255) w->encounters++;
            INSTR(if (w->event.alt_who >= 0) w->in.alt_offered++);
        }
        break;
    case NODE_EVENT:
        w->state = ST_EVENT;
        roll_event(w);
        if (w->encounters < 255) w->encounters++;
        INSTR(if (w->event.alt_who >= 0) w->in.alt_offered++);
        break;
    default:          w->state = ST_MAP;   break;
    }
}

// ---------------------------------------------------------------- market
// Trades move the local price permanently. Sell into a market and it stays
// depressed for the rest of the run: routes burn out behind the player, which
// is what forces the push outward.
// What this settlement still has on the shelf. A fact about the market the
// player is standing in, so the bot may read it -- and must, because a buy that
// fails silently is one the bot will retry forever.
int world_stock(const World *w, int good) {
    const Node *nd = &w->node[w->sector][w->index];
    if (nd->type != NODE_SETTLE) return 0;
    return nd->stock[good];
}

void world_buy(World *w, int good) {
    Node *nd = &w->node[w->sector][w->index];
    if (nd->type != NODE_SETTLE) return;
    if (nd->stock[good] < 1) { INSTR(w->in.bought_blocked++); return; }
    int p = nd->price[good];
    if (w->credits < p || world_cargo(w) >= world_cargo_cap(w)) return;

    w->credits -= p;
    w->held[good]++;
    nd->stock[good]--;
    INSTR(w->in.units_bought++; w->in.credits_out += p;
          if (nd->stock[good] == 0) w->in.stock_out[good]++);
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
    // Promised cargo is not yours to sell -- to a client or to a friend.
    if (w->held[good] - world_committed(w, good)
                      - world_errand_committed(w, good) < 1) return;

    int p = nd->price[good];
    int take = world_sell_price(w, good);
    w->credits += take;
    w->held[good]--;
    // Both numbers, because they are not the same one. The list price is what
    // the screen shows; the take is what a stack actually realises once the
    // price has been walked down by the sale itself.
    INSTR(w->in.units_sold++; w->in.credits_in += take; w->in.sold_headline += p;
          if (++w->in.stack_here > w->in.biggest_stack)
              w->in.biggest_stack = w->in.stack_here);
    // Symmetric with the buy nudge above. Selling used to move the price twice
    // as fast as buying -- p/8 against p/16 -- for no stated reason, so a
    // market punished offloading a stack far harder than it punished loading
    // one. The 20% bid-ask spread is what stops the round trip being free
    // money; this asymmetry was never doing that job and is not needed for it.
    //
    // Measured effect is small, and worth saying so: the largest stack sold at
    // any one market is about 2.6 units, so the decay barely engages and sales
    // already realise 79% of headline. The arithmetic that made this look like
    // a 60% loss assumed a ten-unit sale nobody makes.
    // Selling adds to their shelf, which is what finally makes the nudge above
    // mean something. The comment on the buy side has said since v4 that routes
    // burn out behind the player; with an infinite tap that was mostly a claim,
    // because the largest stack anyone sold was 2.6 units and the decay barely
    // engaged. Dumping water into a well now visibly deepens a market that was
    // already deep, and the price follows.
    if (nd->stock[good] < 30) nd->stock[good]++;

    int np = p - p / 16 - 1;
    nd->price[good] = (int16_t)(np < 1 ? 1 : np);
}

// ---------------------------------------------------------------- encounters
// Which of the two refusals applies. The UI showed "YOU CANNOT PAY THIS" for
// both, so a convoy with a full hold and a full purse was told it was broke.
int world_accept_block(const World *w) {
    const Event *e = &w->event;
    if (e->pay_good >= 0 && w->held[e->pay_good] < e->pay_qty) return 1;
    if (e->gain_good >= 0 && e->gain_qty > 0) {
        int freed = (e->pay_good >= 0) ? e->pay_qty : 0;
        int room  = world_cargo_cap(w) - (world_cargo(w) - freed);
        if (room < e->gain_qty) return 2;
    }
    return 0;
}

int world_can_accept(const World *w) {
    const Event *e = &w->event;
    if (e->pay_good >= 0 && w->held[e->pay_good] < e->pay_qty) return 0;

    // There also has to be somewhere to put what is being offered. Without
    // this the goods were quietly truncated to whatever fitted: the panel
    // advertised a gain, the player paid the price in full, and with a full
    // hold received nothing at all. Paying what is asked and getting less than
    // was shown is the one thing an encounter must never do -- the whole
    // system asks the player to take it at its word.
    if (e->gain_good >= 0 && e->gain_qty > 0) {
        int freed = (e->pay_good >= 0) ? e->pay_qty : 0;
        int room  = world_cargo_cap(w) - (world_cargo(w) - freed);
        if (room < e->gain_qty) return 0;
    }
    return 1;
}

// `stripped` is set only by world_decline: an empty hold ends the run, but
// only when something was taken. Accepting used to be able to kill you --
// several kinds pay in credits and cost goods, so taking the money for your
// last unit of cargo ended the run on the screen that had just shown a gain.
static void end_event(World *w, int stripped) {
    int back = w->after_event ? w->after_event : ST_MAP;
    w->after_event = 0;
    if (stripped && world_cargo(w) == 0) {
        w->state = ST_DEAD; w->death = DEATH_STRIPPED; return;
    }
    w->state = (uint8_t)back;
}

void world_accept(World *w) {
    if (w->state != ST_EVENT) return;
    if (!world_can_accept(w)) return;   // can't afford it; must decline
    INSTR(w->in.ev_accepted[w->event.kind]++);
    regard_shift(w, w->event.kind, 1);

    Event *e = &w->event;
    if (e->pay_good >= 0) w->held[e->pay_good] -= e->pay_qty;

    if (e->gain_good >= 0) {
        int room = world_cargo_cap(w) - world_cargo(w);
        int q = e->gain_qty < room ? e->gain_qty : room;
        if (q > 0) w->held[e->gain_good] += q;
    }
    w->credits += e->gain_credits;
    end_event(w, 0);
}

int world_can_attempt(const World *w) {
    const Event *e = &w->event;
    if (e->alt_who < 0) return 0;
    if (e->alt_pay_good >= 0 && w->held[e->alt_pay_good] < e->alt_pay_qty) return 0;
    // Room for whatever succeeding would hand over, on the same terms as
    // accepting -- paying in full and receiving less than was shown is the one
    // thing an encounter must never do.
    if (e->gain_good >= 0 && e->gain_qty > 0) {
        int freed = (e->alt_pay_good >= 0) ? e->alt_pay_qty : 0;
        int room  = world_cargo_cap(w) - (world_cargo(w) - freed);
        if (room < e->gain_qty) return 0;
    }
    return 1;
}

// Try it. On success you get the accept outcome for the alt's price; on
// failure you get the decline outcome and one unit worse, so the attempt is
// never a free reroll of a bad hand.
int world_attempt(World *w) {
    if (w->state != ST_EVENT || !world_can_attempt(w)) return 0;
    Event *e = &w->event;
    int who_c = world_event_char(e->kind);

    INSTR(w->in.alt_taken++);
    int roll = rng_range(&w->rng_people, 0, 99);
    if (roll < e->alt_odds) {
        if (e->alt_pay_good >= 0) w->held[e->alt_pay_good] -= e->alt_pay_qty;
        if (e->gain_good >= 0) {
            int room = world_cargo_cap(w) - world_cargo(w);
            int q = e->gain_qty < room ? e->gain_qty : room;
            if (q > 0) w->held[e->gain_good] += q;
        }
        w->credits += e->gain_credits;
        // Handling it without robbing anyone is worth something to them.
        if (who_c != CHAR_NONE && w->regard[who_c] < 3) w->regard[who_c]++;
        end_event(w, 0);
        return 1;
    }

    INSTR(w->in.alt_failed++);
    if (who_c != CHAR_NONE && w->regard[who_c] > -3) w->regard[who_c]--;
    // Worse than simply having refused.
    if (e->lose_qty > 0) {
        int take = e->lose_qty + 1;
        if (e->lose_good == -2) {
            while (take-- > 0 && w->payload > 0) { w->payload--; INSTR(w->in.pl_demand++); }
        } else if (e->lose_good < 0) {
            drop_random_cargo(w, take);
        } else {
            int g = e->lose_good;
            w->held[g] -= take;
            if (w->held[g] < 0) w->held[g] = 0;
        }
    }
    end_event(w, 1);
    return 0;
}

void world_decline(World *w) {
    if (w->state != ST_EVENT) return;
    // A refusal the player chose and a refusal the game forced are the same
    // keypress and mean opposite things.
    INSTR(if (!world_can_accept(w)) w->in.ev_forced[w->event.kind]++);
    regard_shift(w, w->event.kind, 0);
    Event *e = &w->event;

    if (e->lose_qty > 0) {
        if (e->lose_good == -2) {
            // Straight off the payload, whatever else is aboard.
            int take = e->lose_qty;
            while (take-- > 0 && w->payload > 0) { w->payload--; INSTR(w->in.pl_demand++); }
        } else if (e->lose_good < 0) {
            drop_random_cargo(w, e->lose_qty);
        } else {
            int g = e->lose_good;
            w->held[g] -= e->lose_qty;
            if (w->held[g] < 0) w->held[g] = 0;
        }
    }
    end_event(w, 1);
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
    int kind  = rng_range(&w->rng_event, 0, EV_KINDS - 1);
    e->kind = (uint8_t)kind;
    int hold_searched = (kind == EV_RAID || kind == EV_TOLL || kind == EV_CHECKPOINT);

    switch (kind) {
    case EV_RAID:
        e->pay_good = G_AMMO;  e->pay_qty = (int8_t)rng_range(&w->rng_event, 2, 3);
        e->lose_good = -1;     e->lose_qty = (int8_t)(rng_range(&w->rng_event, 2, 4) + depth / 3);
        if (w->crew[CREW_GUARD]) { e->pay_qty = 0; e->lose_qty /= 2; }
        break;

    case EV_WRECK:
        // Reversed: parts in, fuel out. It used to charge a unit of fuel for
        // three to six scrap -- about 17 credits for 27, and 51 for 27 once
        // fuel was scarce enough to matter -- which made it the only kind that
        // was a genuinely bad trade rather than merely unaffordable, at 12%
        // accepted. Raising the salvage made it worse, not better: a bigger
        // reward needs more free slots than a hold at 69% occupancy has, so
        // forced refusals tripled while acceptance stood still.
        //
        // Every encounter in this table charged in the two resources that end
        // runs and paid in credits worth about 3% of the final score. This is
        // the first that runs the other way: you spend parts stripping the
        // wreck and come away with what is left in its tank. It is a real
        // decision precisely because the answer changes -- scrap is worth more
        // as trade goods when the convoy is flush, and worth nothing at all
        // compared to fuel when it is not.
        // One to two, not two to three: breakdowns already draw on the same
        // small scrap stock, and asking for three left the convoy unable to
        // pay 55% of the time -- affordable-in-principle content that is
        // unaffordable in practice is the failure this whole phase is about.
        e->pay_good = G_SCRAP; e->pay_qty = (int8_t)rng_range(&w->rng_event, 1, 2);
        e->gain_good = G_FUEL; e->gain_qty = (int8_t)rng_range(&w->rng_event, 2, 3);
        if (w->crew[CREW_MECHANIC]) e->gain_qty += 1;   // knows where the lines run
        e->lose_qty = 0;
        break;

    case EV_SICK:
        e->pay_good = G_MEDS;  e->pay_qty = 1;
        e->lose_good = G_WATER; e->lose_qty = (int8_t)rng_range(&w->rng_event, 2, 3);
        if (w->crew[CREW_MEDIC]) e->pay_qty = 0;
        break;

    case EV_BREAK:
        // Scrap is now spoken for twice over -- a breakdown and a wreck both
        // draw on the same two or three units -- and asking for three left the
        // convoy unable to pay 38% of the time.
        e->pay_good = G_SCRAP; e->pay_qty = (int8_t)rng_range(&w->rng_event, 1, 2);
        e->lose_good = G_FUEL;  e->lose_qty = 2;
        if (w->crew[CREW_MECHANIC]) e->pay_qty = 0;
        break;

    case EV_TRADER:
        e->pay_good = G_WATER; e->pay_qty = (int8_t)rng_range(&w->rng_event, 2, 3);
        e->gain_credits = (int16_t)(rng_range(&w->rng_event, 30, 60) + depth * 8);
        if (w->crew[CREW_TRADER]) e->gain_credits += 35;
        e->lose_qty = 0;
        break;

    case EV_TOLL:
        e->pay_good = G_AMMO;  e->pay_qty = (int8_t)rng_range(&w->rng_event, 1, 2);
        e->lose_good = -1;     e->lose_qty = (int8_t)rng_range(&w->rng_event, 2, 3);
        if (w->crew[CREW_GUARD]) e->pay_qty = 0;
        break;

    case EV_CACHE:
        e->pay_good = G_FUEL;  e->pay_qty = 1;
        // A buried cache can hold water, which is the other half of letting
        // this table return survival margin rather than only charge in it.
        {
            int roll = rng_range(&w->rng_event, 0, 2);
            e->gain_good = (int8_t)(roll == 0 ? G_AMMO : roll == 1 ? G_MEDS : G_WATER);
        }
        e->gain_qty  = (int8_t)rng_range(&w->rng_event, 2, 4);
        if (w->crew[CREW_SCOUT]) e->gain_qty += 2;      // knows where to dig
        e->lose_qty  = 0;
        break;

    case EV_BRIDGE:
        e->pay_good = G_FUEL;  e->pay_qty = (int8_t)rng_range(&w->rng_event, 1, 2);
        e->lose_good = G_WATER; e->lose_qty = (int8_t)rng_range(&w->rng_event, 2, 4);
        if (w->crew[CREW_SCOUT])    e->pay_qty = 0;    // knows a ford
        if (w->crew[CREW_MECHANIC]) e->lose_qty /= 2;  // rigs a crossing
        break;

    case EV_RIVAL: {
        // A straight swap. Both sides think they are winning.
        //
        // It used to name a good at random, and 43% of the time that was
        // something the convoy did not have two of -- so the commonest outcome
        // was not a refusal but an inability, which reads the same on screen
        // and means the opposite. A rival eyes what you are actually carrying.
        int give = rng_range(&w->rng_event, 0, GOODS_COUNT - 1);
        for (int t = 0; t < GOODS_COUNT; ++t) {
            if (w->held[give] >= 2) break;
            give = (give + 1) % GOODS_COUNT;
        }
        int take = (give + 1 + rng_range(&w->rng_event, 0, GOODS_COUNT - 2)) % GOODS_COUNT;
        int want = rng_range(&w->rng_event, 2, 4);
        if (want > w->held[give]) want = w->held[give];   // never more than is aboard
        e->pay_good  = (int8_t)give; e->pay_qty  = (int8_t)want;
        e->gain_good = (int8_t)take; e->gain_qty = (int8_t)rng_range(&w->rng_event, 2, 4);
        if (w->crew[CREW_TRADER]) e->gain_qty++;      // drives a bargain
        e->lose_qty = 0;
        break;
    }

    case EV_PLAGUE:
        e->pay_good = G_MEDS;  e->pay_qty = (int8_t)rng_range(&w->rng_event, 1, 2);
        e->lose_good = G_WATER; e->lose_qty = (int8_t)(3 + depth / 4);
        if (w->crew[CREW_MEDIC]) e->pay_qty = 0;
        break;

    case EV_CHECKPOINT:
        e->pay_good = G_AMMO;  e->pay_qty = (int8_t)rng_range(&w->rng_event, 1, 3);
        e->lose_good = -1;     e->lose_qty = (int8_t)rng_range(&w->rng_event, 3, 5);
        if (w->crew[CREW_GUARD]) { e->pay_qty = 0; e->lose_qty /= 2; }
        break;

    case EV_LEAK:
        e->pay_good = G_SCRAP; e->pay_qty = 1;
        e->lose_good = G_FUEL;  e->lose_qty = (int8_t)rng_range(&w->rng_event, 2, 3);
        if (w->crew[CREW_MECHANIC]) e->pay_qty = 0;
        break;

    case EV_REFUGEE:
        e->pay_good = G_WATER; e->pay_qty = (int8_t)rng_range(&w->rng_event, 1, 3);
        e->gain_credits = (int16_t)(rng_range(&w->rng_event, 20, 45) + depth * 5);
        if (w->crew[CREW_MEDIC]) e->gain_credits += 30;  // tends them as they pass
        e->lose_qty = 0;
        break;

    default: // EV_SIGNAL
        // Accepted 87% of the time: a unit of fuel for ninety-odd credits is
        // not a decision, it is a formality. Trimmed until chasing the tip is
        // worth it when fuel is plentiful and a real question when it is not.
        e->pay_good = G_FUEL;  e->pay_qty = 1;
        e->gain_credits = (int16_t)(rng_range(&w->rng_event, 25, 50) + depth * 4);
        if (w->crew[CREW_TRADER]) e->gain_credits += 40;  // knows what a tip is worth
        e->lose_qty = 0;
        break;
    }

    INSTR(w->in.ev_fired[kind]++);

    // Anyone who searches the hold past the halfway mark knows what the crates
    // are worth. This is what puts the ending at stake rather than merely the
    // accounting -- without it the seed always arrives and three of the five
    // endings are unreachable.
    //
    // Both the chance and the price climb with depth. At a flat 45% for one or
    // two crates the arithmetic never got there: about 0.19 demands per run at
    // 1.5 crates each is 0.29 crates lost, against six needed, so arriving
    // empty-handed did not occur once in 800 measured runs and the ending was
    // decoration. Late raiders know exactly what is in the crates, and refusing
    // them costs accordingly -- which is what makes the last sectors the ones
    // where the run is actually decided.
    if (hold_searched && depth >= (SECTORS - 1) / 3 && w->payload > 0
        && rng_range(&w->rng_event, 0, 99) < 45 + depth * 3) {
        e->lose_good = -2;                        // -2 means the payload itself
        e->lose_qty  = (int8_t)(rng_range(&w->rng_event, 1, 2) + depth / 5);
    }

    // Standing, applied after the demand above rather than before it. Bad
    // standing used to add a unit to what refusing costs and then have that
    // thrown away by the payload override -- on precisely the three kinds the
    // raider chief appears in, which are the only ones his standing affects.
    //
    // The gates are symmetric now. They were not: a discount needed pay_qty
    // above 1 and a bonus needed regard above 1, while both penalties needed
    // only regard below 0. Half the table has pay_qty of exactly 1, so good
    // standing bought nothing at all on those kinds while bad standing always
    // cost. Goodwill can now take a price to zero, which is the same shape as
    // what having the right crew aboard does.
    {
        int who = world_event_char(kind);
        if (who != CHAR_NONE) {
            w->met[who]++;
            INSTR(w->in.char_met[who] = 1;
                  if (w->met[who] >= 2) w->in.char_met2[who] = 1);
            int r = w->regard[who];
            if (r > 0 && e->pay_qty  > 0) e->pay_qty--;
            if (r < 0 && e->pay_qty  > 0) e->pay_qty++;
            if (r < 0 && e->lose_qty > 0) e->lose_qty++;
            if (r > 0 && e->gain_qty > 0) e->gain_qty++;
        }
    }

    // Armour, last. It used to be clamped inside each case and then overwritten
    // wholesale by the payload demand above, so the one fitting sold as
    // protection gave exactly none in the half of the run where raiders start
    // asking for the seed -- which is the half a player buys it for. It now
    // covers the crates too: one crate back is worth more than a full hold.
    // A guard aboard makes every threat cost less, not only the three kinds
    // they specialise in. Always-on is the shape that measured well: the
    // trader was the least-bad role in v4 for exactly this reason.
    // Hired guns. They do not stop trouble finding you, they make paying it
    // off cheaper -- which is the same shape as the guard aboard, deliberately,
    // because a service that did something structurally new would need its own
    // measurement before it could be trusted and this one can be read straight
    // off the existing threat counters.
    if (w->escort > 0 && e->pay_good == G_AMMO && e->pay_qty > 0) e->pay_qty--;

    if (w->upgrade[UPG_ARMOUR] && e->lose_qty > 1) {
        if (e->lose_good == -2) e->lose_qty--;
        else                    e->lose_qty = 1;
    }

    // ---- the third way through -----------------------------------------
    //
    // Two tiers. The hand who knows this kind of trouble offers a manoeuvre
    // that costs nothing and usually works; anyone else aboard offers to
    // improvise, for a unit of whatever the deal was priced in and worse odds.
    // Matched beats improvised.
    e->alt_who = -1;
    {
        int want = world_event_role(kind);
        int who  = -1, base = 0, matched = 0;
        if (want >= 0 && w->crew[want]) { who = want; base = 80; matched = 1; }
        else {
            for (int k = 0; k < CREW_COUNT; ++k)
                if (w->crew[k]) { who = k; base = 60; break; }
        }
        if (who >= 0) {
            e->alt_who      = (int8_t)who;
            // A flag, not `base == 70`.
            //
            // That test is what used to be here, and base is only ever 80 or
            // 60, so it was never true: both tiers charged a unit and the
            // specialist's manoeuvre -- free by design, and described as free
            // in three separate comments -- shipped in v5 costing exactly what
            // the improvised one did. The two tiers differed only in odds.
            //
            // The 70 was the number this branch carried in the plan before the
            // odds were retuned to 80/60. Nothing failed when it drifted,
            // because a condition that is merely never true throws no warning
            // and breaks no test; it just quietly deletes the feature. Hence a
            // flag set where the tier is chosen, which cannot drift away from
            // the thing it describes.
            e->alt_pay_good = matched ? -1 : e->pay_good;
            e->alt_pay_qty  = matched ?  0 : 1;

            // Odds move with how the counterpart feels about you. This is the
            // payoff the -3..+3 range has never had: until now only its sign
            // was ever read, and only to nudge a quantity by one.
            int who_c = world_event_char(kind);
            int r = (who_c == CHAR_NONE) ? 0 : w->regard[who_c];
            int odds = base + r * 8;
            if (odds < 35) odds = 35;
            if (odds > 85) odds = 85;
            e->alt_odds = (uint8_t)odds;
        }
    }
}
