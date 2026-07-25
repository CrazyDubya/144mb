// convoy -- world simulation: route generation, markets, travel and survival.
// Pure logic, no rendering, no OS. Fully deterministic from a 32-bit seed.
#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

#define SECTORS      14
#define NODES_PER    4
#define GOODS_COUNT  5
#define CARGO_CAP    30

// Goods are indices into every per-good array, and match the ICON_* order.
enum { G_WATER, G_FUEL, G_AMMO, G_MEDS, G_SCRAP };

// What a settlement produces. Its own good is cheap here and the things it
// cannot make are dear, which is what gives a route a reason to exist: you buy
// water at a well and sell it to a refinery that has none.
typedef enum {
    ARCH_WELL, ARCH_REFINERY, ARCH_ARMOURY,
    ARCH_CLINIC, ARCH_SCRAPYARD, ARCH_GENERAL,
    ARCH_COUNT
} Archetype;

typedef enum {
    NODE_EMPTY = 0,   // nothing here
    NODE_SETTLE,      // market
    NODE_EVENT,       // an encounter, kind unknown until you arrive
    NODE_HAZARD,      // storm: costs extra water/fuel
    NODE_GREEN        // the goal
} NodeType;

// Encounters are one shape with different numbers in it: pay something to take
// the good outcome, or refuse and take the bad one. Every one of them is an
// economic decision, because the price is always paid in cargo.
typedef enum {
    EV_RAID,      // pay ammo, or they take cargo
    EV_WRECK,     // pay fuel to detour, gain salvage
    EV_SICK,      // pay meds, or the crew drink extra water
    EV_BREAK,     // pay scrap to repair, or lose fuel
    EV_TRADER,    // sell water at a premium
    EV_KINDS
} EventKind;

typedef struct {
    uint8_t kind;
    int8_t  pay_good,  pay_qty;    // cost of the "accept" option
    int8_t  gain_good, gain_qty;   // what accepting yields (-1 = nothing)
    int8_t  lose_good, lose_qty;   // what refusing costs (-1 good = random cargo)
    int16_t gain_credits;
} Event;

typedef struct {
    uint8_t active;
    uint8_t type;
    uint8_t visited;
    uint8_t links;              // bitmask of reachable nodes in the next sector
    uint8_t archetype;          // meaningful only for NODE_SETTLE
    int16_t price[GOODS_COUNT]; // current, mutated permanently by player trades
} Node;

typedef enum {
    ST_MAP, ST_TRADE, ST_EVENT, ST_DEAD, ST_WON
} WorldState;

typedef enum {
    DEATH_NONE = 0, DEATH_THIRST, DEATH_STRANDED, DEATH_STRIPPED
} DeathCause;

// A delivery job. Gives the hold a purpose beyond speculation: cargo you are
// contractually holding is cargo you cannot panic-sell for fuel, which is the
// tension worth having.
typedef struct {
    uint8_t state;     // CONTRACT_NONE / OFFERED / TAKEN
    uint8_t good;
    uint8_t qty;
    uint8_t by_sector; // deliver at a settlement at or beyond this sector
    int16_t reward;
} Contract;

enum { CONTRACT_NONE, CONTRACT_OFFERED, CONTRACT_TAKEN };

typedef struct {
    uint32_t rng;
    Node     node[SECTORS][NODES_PER];

    int sector, index;          // where the convoy is
    int held[GOODS_COUNT];
    int credits;
    int day;

    // Every price the player has personally seen, so the game can tell them
    // whether this market is cheap or dear without them memorising a table.
    // The bot keeps the same running average for itself, so both reason from
    // identical information.
    int32_t seen_sum[GOODS_COUNT];
    int16_t seen_n[GOODS_COUNT];

    int state;
    int death;

    Event    event;
    Contract job;      // one at a time: two would just be arithmetic
    int      job_paid; // reward banked this stop, for the UI to celebrate
} World;

void world_init  (World *w, uint32_t seed);
int  world_cargo (const World *w);
int  world_can_travel(const World *w, int next_index);
// Fills `out` with the node indices reachable from here, returning how many.
// Lives here rather than in the UI because it is a fact about the route.
int  world_reachable (const World *w, int *out);
// The good a settlement specialises in, or -1 for a general trading post.
int  world_arch_good (int archetype);
// -1 if this market is notably cheap for the good, +1 if notably dear, 0 if
// it is about what the player has seen elsewhere.
int  world_price_bias(const World *w, int good);
// What a market actually pays for a good, which is less than it charges.
// Without that spread, buying and immediately reselling at the same stall is
// profitable -- the buy nudges the price up and you sell into your own nudge.
int  world_sell_price(const World *w, int good);
void world_contract_accept(World *w);
// Units of `good` promised to an accepted contract, so nothing sells them out
// from under the job.
int  world_committed (const World *w, int good);
void world_travel(World *w, int next_index);
void world_buy   (World *w, int good);
void world_sell  (World *w, int good);
int  world_can_accept(const World *w);
void world_accept(World *w);    // pay the price
void world_decline(World *w);   // take the consequence

#endif
