// convoy -- world simulation: route generation, markets, travel and survival.
// Pure logic, no rendering, no OS. Fully deterministic from a 32-bit seed.
#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

#define SECTORS      14
#define NODES_PER    4
#define GOODS_COUNT  5
#define CARGO_CAP    30
#define PAYLOAD_SLOTS 6

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
    EV_RAID,       // pay ammo, or they take cargo
    EV_WRECK,      // pay fuel to detour, gain salvage
    EV_SICK,       // pay meds, or the crew drink extra water
    EV_BREAK,      // pay scrap to repair, or lose fuel
    EV_TRADER,     // sell water at a premium
    EV_TOLL,       // pay ammo to pass, or they help themselves
    EV_CACHE,      // dig it out with fuel, or leave it
    EV_BRIDGE,     // pay fuel for the detour, or bake a day in the sun
    EV_RIVAL,      // swap cargo with another convoy
    EV_PLAGUE,     // pay meds, or lose water to a sick crew
    EV_CHECKPOINT, // pay ammo as a bribe, or they confiscate
    EV_LEAK,       // pay scrap to patch, or bleed fuel
    EV_REFUGEE,    // pay water, and they pay you for it
    EV_SIGNAL,     // pay fuel to chase a tip-off, gain credits
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

// Difficulty does not scale one number. Each setting leans on a different one
// of the three ways a run ends -- thirst, being stranded, being stripped -- so
// the modes fail differently rather than just more often.
enum { DIFF_EASY, DIFF_NORMAL, DIFF_HARD, DIFF_COUNT };

typedef enum {
    DEATH_NONE = 0, DEATH_THIRST, DEATH_STRANDED, DEATH_STRIPPED
} DeathCause;

// How a run is judged, in descending order of how well it went. Arriving is
// not the same as arriving with what you set out to carry.
typedef enum {
    OUT_DEAD,          // never got there
    OUT_EMPTY,         // arrived with none of the seed stock
    OUT_PARTIAL,       // arrived with some of it
    OUT_INTACT,        // arrived with all of it
    OUT_EXEMPLARY,     // all of it, crew alive, and money in the bank
    OUT_COUNT
} Outcome;

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

// People you meet more than once. Regard shifts with how you treat them and
// changes what they ask for next time, so a character is a mechanic rather
// than a portrait with a line of dialogue attached.
enum {
    CHAR_NONE = -1,
    CHAR_CHIEF,     // raiders, tolls, checkpoints
    CHAR_CAPTAIN,   // the rival convoy, running west
    CHAR_TRADER,    // road traders and radio tip-offs
    CHAR_DOC,       // sickness and plague
    CHAR_DRIFTER,   // walkers, wrecks, caches
    CHAR_COUNT
};

// One-off purchases that change the rules for the rest of the run.
enum { UPG_HOLD, UPG_ECON, UPG_ARMOUR, UPG_TANKS, UPG_COUNT };

// Crew are hands that help and mouths that drink. Every one aboard raises the
// daily water burn, so hiring is a running cost rather than a straight upgrade.
enum { CREW_MECHANIC, CREW_GUARD, CREW_MEDIC, CREW_SCOUT, CREW_TRADER, CREW_COUNT };

// Measurement counters, compiled into the headless harness only. They exist
// because outcomes alone cannot tell you why: a kind nobody accepts because it
// is a bad deal and a kind nobody *can* accept read identically in a win-rate
// column, and an option never offered looks exactly like an option refused.
//
// Kept out of the shipped executable entirely -- the contest target should not
// carry the cost of its own test rig, and the exe stays byte-identical whether
// or not the harness is instrumented.
#ifdef CONVOY_INSTRUMENT
typedef struct {
    uint16_t ev_fired[EV_KINDS];    // times each encounter kind came up
    uint16_t ev_accepted[EV_KINDS]; // ...and was paid
    uint16_t ev_forced[EV_KINDS];   // ...and could not be paid, so was refused

    // Four distinct fates, deliberately not one "expired" bucket: a job the
    // player refused, one they walked away from, and one they took and could
    // not deliver mean three different things about the mechanic.
    uint16_t c_offered, c_accepted, c_completed;
    uint16_t c_declined;    // refused at the board
    uint16_t c_lapsed;      // left on the board when the convoy departed
    uint16_t c_forfeit;     // taken, then carried past any hope of delivery

    uint16_t pl_storm, pl_demand, pl_random;   // crates lost, by cause

    uint32_t units_bought, units_sold;
    int32_t  credits_in, credits_out;
    int32_t  sold_headline;   // list value of everything sold, before the take
    uint16_t biggest_stack;   // most units offloaded at one settlement
    uint16_t stack_here;      // running count at the current stop

    uint32_t cargo_sum;       // for a time-weighted mean occupancy
    uint16_t cargo_samples;
    uint16_t peak_cargo;

    uint8_t  min_water, min_fuel;
    uint16_t days_thin;       // days ending with water or fuel at 2 or less
} Metrics;
#define INSTR(stmt) do { stmt; } while (0)
#else
#define INSTR(stmt) do { } while (0)
#endif

typedef struct {
    // Three independent streams, not one. Everything used to draw from a
    // single generator, which meant adding one die roll to an encounter table
    // reshuffled every later market offer and contract for that seed -- so a
    // seed stopped being the same run the moment any table was edited, and
    // paired before/after comparison was impossible. Worse, salvaged kit rolls
    // for failure only when it is fitted, so the convoy's own purchase moved
    // the stream and re-rolled its own encounters.
    //
    // Splitting them means the map is a function of the seed alone and stays
    // fixed while the tables are tuned.
    uint32_t rng_map;      // route layout and prices: world_init only
    uint32_t rng_offer;    // market offers, contracts, salvage failure
    uint32_t rng_event;    // encounters, storm spoilage, random cargo loss
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

    // The reason for the whole run: seed stock bound for the Green Zone.
    // It occupies the hold, cannot be sold at any price, and can be taken --
    // so arriving and succeeding are not the same thing.
    uint8_t  payload;              // slots still aboard, of PAYLOAD_SLOTS
    uint8_t  encounters;           // how many fired all run, for measurement
    uint8_t  after_event;          // state to return to; 0 (ST_MAP) unless set
    uint8_t  diff;                 // DIFF_*, fixed for the whole run
    uint32_t seed;                 // the seed as handed in; rng has moved on
    // `payload_lost_to` used to sit here, documented as "what took the last of
    // it, for the ending". It was written on only one of the three paths that
    // can take a crate, and what it stored was a WorldState rather than a
    // cause. Its single reader compared it against a sentinel that was never
    // assigned anywhere in the program, so that test was always true. Nothing
    // reads it now; the harness counts crates lost by cause properly.

    uint8_t  upgrade[UPG_COUNT];   // fitted or not
    uint8_t  crew[CREW_COUNT];     // aboard or not
    uint8_t  offer_upg;            // what this settlement will fit, 0xFF if none
    uint8_t  offer_salvaged;       // that offer is salvaged: cheap, and may fail
    uint8_t  upg_salvaged[UPG_COUNT];
    int8_t   kit_failed;           // an upgrade broke on the last hop, or -1
    uint8_t  offer_crew;           // who is looking for work here, 0xFF if none

    uint8_t  met   [CHAR_COUNT];   // times encountered
    int8_t   regard[CHAR_COUNT];   // -3..+3, shifts with what you did

    Event    event;
    Contract job;      // one at a time: two would just be arithmetic
    int      job_paid; // reward banked this stop, for the UI to celebrate

#ifdef CONVOY_INSTRUMENT
    Metrics in;
#endif
} World;

void world_init  (World *w, uint32_t seed, int diff);
// A run's final number, so two players can compare the same daily seed.
int  world_score (const World *w);
int  world_cargo (const World *w);
int  world_hop_costs_fuel(const World *w);
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
void world_contract_decline(World *w);
// Units of `good` promised to an accepted contract, so nothing sells them out
// from under the job.
int  world_committed (const World *w, int good);
// Slots the payload occupies. Counted against capacity like any other cargo,
// because the whole point is that it competes for space you need.
int  world_payload   (const World *w);
// Which of the endings this run has earned.
int  world_outcome   (const World *w);
// Who is on the other side of this encounter, or CHAR_NONE.
int  world_event_is_threat(int kind);
int  world_event_char(int kind);

int  world_cargo_cap (const World *w);   // grows with fitted racks
int  world_crew_count(const World *w);
int  world_water_burn(const World *w);   // per day, given crew and tanks
int  world_water_burn_on(const World *w, int day);
int  world_crew_drinks_on(const World *w, int day);
int  world_upg_price (const World *w, int upg, int salvaged);
// What a fitting can still return over the hops that remain. Price is derived
// from this, so an offer that cannot repay itself is never made.
int  world_upg_payback(const World *w, int upg);
int  world_crew_payback(const World *w, int crew);
// What the road east still holds, for deciding whether kit is worth it.
void world_road_ahead (const World *w, int *storms, int *encounters);
int  world_crew_price(const World *w, int crew);
void world_buy_upgrade(World *w);
void world_hire_crew  (World *w);
void world_travel(World *w, int next_index);
void world_buy   (World *w, int good);
void world_sell  (World *w, int good);
int  world_can_accept(const World *w);
// Why an encounter cannot be taken: 0 fine, 1 cannot pay, 2 no room.
int  world_accept_block(const World *w);
void world_accept(World *w);    // pay the price
void world_decline(World *w);   // take the consequence

#endif
