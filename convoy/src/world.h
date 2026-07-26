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

    // A third way through, offered by whoever is aboard.
    //
    // This exists because a crew role covers three of fourteen encounter kinds,
    // so its ability fired 0.79 times in a run and every role measured
    // net-negative even when granted free. An alt is offered on essentially
    // every encounter instead, which is a 4.7x change in how often a hand
    // matters without changing what it does per occasion.
    //
    // It cannot dominate: cheaper than accepting, so it can fail; and failing
    // costs one more than declining would have, so it is not a free reroll.
    int8_t  alt_who;               // CREW_* offering it, or -1 for none
    int8_t  alt_pay_good, alt_pay_qty;
    uint8_t alt_odds;              // percent, 35..85
} Event;

// What a settlement is going through when you get there. Rolled at world-gen
// and hidden until arrival: the world always knew, the fog is the player's.
enum {
    COND_NONE = 0, COND_SIEGE, COND_QUARANTINE, COND_BOOM,
    COND_ABANDONED, COND_CARTEL, COND_DRY, COND_COUNT
};

typedef struct {
    uint8_t active;
    uint8_t type;
    uint8_t visited;
    uint8_t links;              // bitmask of reachable nodes in the next sector
    uint8_t archetype;          // meaningful only for NODE_SETTLE
    uint8_t cond;               // COND_*, hidden until visited
    uint8_t name;               // high nibble picks the first word, low the second
    uint8_t stock[GOODS_COUNT]; // units this settlement will part with
    int16_t price[GOODS_COUNT]; // current, mutated permanently by player trades
} Node;

// What the player is allowed to know about a node they have not stood in.
//
// The fog lives here rather than in the drawing code, and that is the whole
// point of the type existing. A rule enforced only where a panel is drawn is a
// rule the test bot never meets, and the bot is what every measurement in this
// project is made with -- so a bot reading node[s][n].price directly would make
// the rumour system score perfectly against information the player cannot see,
// and the release's central number would be measuring nothing.
//
// Type, archetype, links and name survive the fog: all four are drawn on the
// map, and a name is not information -- it is what a rumour points at.
typedef struct {
    uint8_t known;              // 0 when this node has never been visited
    uint8_t type, archetype, links, name;
    uint8_t cond;               // 0 unless known
    uint8_t stock[GOODS_COUNT]; // 0 unless known
    int16_t price[GOODS_COUNT]; // 0 unless known
} NodeView;

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

// A personal errand from someone aboard.
//
// Deliberately not a Contract. That structure is one-at-a-time, so an errand
// sharing it would disable the job board -- the exact failure v4 spent a phase
// fixing -- and its six fate counters would stop meaning anything. An errand is
// a commitment of *route*, which is the resource the map contests and nothing
// else taxes, or of hold space held under a promise.
enum { ERR_NONE, ERR_OFFERED, ERR_VISIT, ERR_CARRY, ERR_DONE };

typedef struct {
    uint8_t state;      // ERR_*
    uint8_t who;        // CREW_* whose errand it is
    uint8_t arg;        // ARCH_* to visit, or the good to carry
    uint8_t qty;        // units to carry
    uint8_t by_sector;  // do it before arriving here
} Errand;

// Something someone told you about the road ahead.
//
// Held on the World and not on the Node, deliberately. A rumour parked on the
// node it describes would let a player accumulate a solved map, and it would be
// state whose meaning depends on when it was written. Four slots, FIFO, and a
// rumour is dropped the moment its sector is behind you -- forward-only travel
// makes expiry free.
//
// A rumour may only claim things the fog hides. The map already draws node type
// and archetype, so "there is a storm at X" is not information, it is the map
// read aloud, and a claim set that duplicates the map is noise wearing a hat.
#define RUMOUR_SLOTS 4

enum {
    CL_PRICE,   // "they pay well for meds there" / "fuel is cheap"
    CL_STOCK,   // "they have water to spare" / "no ammo left"
    CL_COND,    // "that place is under siege"
    CL_ROAD,    // which kind of trouble an encounter node holds
    CL_COUNT
};

typedef struct {
    uint8_t sector, index;   // the node it is about
    uint8_t claim;           // CL_*
    uint8_t arg;             // the good, condition or event kind claimed
    int8_t  src;             // CHAR_* who said it, or -1 for the room
    uint8_t conf;            // 0..100, honestly derived from who is speaking
    // Whether the claim is actually true. Never shown, and never read by the
    // bot -- see the note on NodeView. The confidence above is honest even when
    // this is not: the claim may lie, the game's account of how well it knows
    // may not. A game that misrepresents its own certainty teaches the player
    // to discard all of its information, the true parts included.
    uint8_t truth;
} Rumour;

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

// The five hands are the five people on the road. Four of the pairings were
// already exact -- the chief owns raids, the doc owns sickness, the trader owns
// trades, the drifter owns wrecks -- and Marlow is the one invention: the rival
// convoy captain joins as mechanic after her own rig dies, which is a story
// only losing can tell.
//
// crew[] stays indexed by ROLE, not by character. Every ability site in
// roll_event compiles untouched and identity comes for free.
extern const signed char CHAR_OF_ROLE[CREW_COUNT];
extern const signed char ROLE_OF_CHAR[CHAR_COUNT];

// Turning an enemy is meant to be an achievement, so the two who rob you want
// more goodwill and more money than the three who do not.
int  world_char_is_enemy(int who);

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

    // Per role, because "12% of runs hire someone" hides whether that is one
    // role always taken and four never offered, or five taken evenly.
    uint16_t crew_offered[CREW_COUNT];
    uint16_t crew_hired[CREW_COUNT];
    uint16_t upg_offered[UPG_COUNT];
    uint16_t upg_bought[UPG_COUNT];

    uint32_t units_bought, units_sold;
    int32_t  credits_in, credits_out;
    int32_t  sold_headline;   // list value of everything sold, before the take
    uint16_t biggest_stack;   // most units offloaded at one settlement
    uint16_t stack_here;      // running count at the current stop

    uint32_t cargo_sum;       // for a time-weighted mean occupancy
    uint16_t cargo_samples;
    uint16_t peak_cargo;

    // Who the road actually introduced, and how it was left. The BOT line
    // prints only the sum of regard, which cancels a +3 against a -3 and hides
    // exactly the distribution a recruitment gate would read.
    uint8_t  char_met[CHAR_COUNT];       // met at least once this run
    uint8_t  char_met2[CHAR_COUNT];      // ...at least twice
    // Four candidate gates, measured together so Phase 2 picks from data
    // instead of taste. g[0]=met1/reg1, g[1]=met2/reg1, g[2]=met1/reg2,
    // g[3]=met2/reg2 (the one originally planned).
    uint8_t  char_recruit[CHAR_COUNT];   // gate condition was true at some point
    uint8_t  gate_any[4];                // any character passed gate i this run
    int8_t   char_regard_end[CHAR_COUNT];

    // Reserved so Metrics grows once rather than every phase.
    uint16_t alt_offered, alt_taken, alt_failed;
    uint16_t err_offered, err_taken, err_done, err_failed, crew_left;

    uint8_t  min_water, min_fuel;
    uint16_t days_thin;       // days ending with water or fuel at 2 or less

    // Towns. Reserved in one go rather than a field at a time, for the reason
    // the block above was: Metrics should grow once in a release, not in every
    // phase, so that a struct change is never confused with a result.
    //
    // The pair that decides whether finite stock shipped as a mechanic or as
    // decoration is stock_out against bought_blocked: a constraint that never
    // binds is furniture, and one that binds constantly is starving the run.
    uint16_t stock_out[GOODS_COUNT];  // stops where a wanted good had none left
    uint16_t bought_blocked;          // buys refused for want of stock
    uint16_t svc_offered[ARCH_COUNT], svc_used[ARCH_COUNT];
    uint16_t cond_seen[COND_COUNT],   cond_won[COND_COUNT];
    uint16_t sit_present, sit_entered, sit_accepted, sit_declined;
    uint16_t places_live, places_visited, calls_spent;
    // Rumours, banded by stated confidence. Accuracy is measured per band
    // because the bands are the only thing the player can act on: a system
    // whose "swears to it" and "reckons" come true equally often has told them
    // nothing, however good its average.
    uint16_t rum_offered, rum_true, rum_acted;
    uint16_t rum_band[3], rum_band_true[3];
} Metrics;
#define INSTR(stmt) do { stmt; } while (0)
#else
#define INSTR(stmt) do { } while (0)
#endif

typedef struct {
    // Five independent streams, not one. Everything used to draw from a
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
    // People: recruit offers, errands, who speaks at a stop, and whether a
    // crew manoeuvre comes off. Separate from the three above for the reason
    // v4 proved expensively -- a single extra draw in roll_event reshuffles
    // every later market offer for that seed, and the resulting numbers still
    // look entirely plausible.
    uint32_t rng_people;
    // Towns: stock levels, names, which places are open, what the situation is,
    // and who is talking. Split off for the same reason as the four above, and
    // the rule for v6 is absolute -- nothing in the town layer draws from any
    // other stream, so that a settlement change cannot silently reshuffle an
    // encounter or a contract and hand back numbers that look plausible.
    uint32_t rng_town;
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
    uint8_t  regard_moved[CHAR_COUNT];  // sector+1 of the last shift, or 0

    Event    event;
    Contract job;      // one at a time: two would just be arithmetic
    Errand   errand;   // likewise, and never more than one across all hands
    uint8_t  warned;   // a hand has said they are thinking of leaving
    int      job_paid; // reward banked this stop, for the UI to celebrate

    // A stop is not a menu you exhaust. Daylight is what makes a town a choice
    // rather than a longer list: the stalls are free, and the other places in
    // town cost a call each. Reset on arrival.
    uint8_t  calls_left;
    uint8_t  svc_used;      // the local trade is a once-per-stop thing
    Rumour   heard[RUMOUR_SLOTS];
    uint8_t  heard_n;

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
int  world_can_recruit(const World *w, int who);
uint8_t world_links(const World *w);
int  world_reachable (const World *w, int *out);
// The good a settlement specialises in, or -1 for a general trading post.
int  world_arch_good (int archetype);
// What may be known about a node from where the convoy is standing. Every
// reader of a node other than the one underfoot must come through here.
void world_node_known(const World *w, int s, int n, NodeView *out);
// Units of a good the market underfoot will still sell. Zero means the shelf is
// empty and a buy will do nothing -- which anything pressing BUY must check,
// because world_buy fails silently and an unchecked retry never terminates.
int  world_stock(const World *w, int good);
// -1 if this market is notably cheap for the good, +1 if notably dear, 0 if
// it is about what the player has seen elsewhere.
int  world_price_bias(const World *w, int good);
// What a market actually pays for a good, which is less than it charges.
// Without that spread, buying and immediately reselling at the same stall is
// profitable -- the buy nudges the price up and you sell into your own nudge.
int  world_sell_price(const World *w, int good);
void world_contract_accept(World *w);
void world_contract_decline(World *w);
void world_errand_accept(World *w);
void world_errand_decline(World *w);
int  world_errand_committed(const World *w, int good);
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
int  world_event_role(int kind);
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
// The third branch: attempt it. Succeeds on alt_odds, else the decline outcome
// plus one. Returns 1 if it came off.
int  world_attempt(World *w);
int  world_can_attempt(const World *w);
// Why an encounter cannot be taken: 0 fine, 1 cannot pay, 2 no room.
int  world_accept_block(const World *w);
void world_accept(World *w);    // pay the price
void world_decline(World *w);   // take the consequence

#endif
