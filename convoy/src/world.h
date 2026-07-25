// convoy -- world simulation: route generation, markets, travel and survival.
// Pure logic, no rendering, no OS. Fully deterministic from a 32-bit seed.
#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

#define SECTORS      10
#define NODES_PER    4
#define GOODS_COUNT  5
#define CARGO_CAP    30

// Goods are indices into every per-good array, and match the ICON_* order.
enum { G_WATER, G_FUEL, G_AMMO, G_MEDS, G_SCRAP };

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
    int16_t price[GOODS_COUNT]; // current, mutated permanently by player trades
} Node;

typedef enum {
    ST_MAP, ST_TRADE, ST_EVENT, ST_DEAD, ST_WON
} WorldState;

typedef enum {
    DEATH_NONE = 0, DEATH_THIRST, DEATH_STRANDED, DEATH_STRIPPED
} DeathCause;

typedef struct {
    uint32_t rng;
    Node     node[SECTORS][NODES_PER];

    int sector, index;          // where the convoy is
    int held[GOODS_COUNT];
    int credits;
    int day;

    int state;
    int death;

    Event event;
} World;

void world_init  (World *w, uint32_t seed);
int  world_cargo (const World *w);
int  world_can_travel(const World *w, int next_index);
void world_travel(World *w, int next_index);
void world_buy   (World *w, int good);
void world_sell  (World *w, int good);
int  world_can_accept(const World *w);
void world_accept(World *w);    // pay the price
void world_decline(World *w);   // take the consequence

#endif
