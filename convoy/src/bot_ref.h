// convoy -- the frozen reference agent.
//
// A byte-for-byte copy of src/bot.c as it stood at the start of v4, renamed so
// both can be linked at once. It is never edited again, and that is the whole
// point: when the working bot changes, this one holds the game fixed, so a
// moved win rate can be attributed to the change rather than to the observer.
//
// It plays badly. It samples prices once per keypress, its hire test is a
// tautology, and it never runs short of hold space. A control does not need to
// be good, it needs to be constant.
//
// Caveat: it calls world_crew_price and world_upg_price, so a phase that
// reprices kit WILL move it. It is a control for changes to the agent, not an
// absolute yardstick.
// convoy -- test-only bot. Compiled into the headless harness only; it adds
// nothing to the submitted executable.
#ifndef BOT_REF_H
#define BOT_REF_H

#include "game.h"
#include "world.h"

typedef struct {
    long sum[GOODS_COUNT];    // running total of prices observed per good
    int  seen[GOODS_COUNT];   // how many markets contributed
    int  float_credits;       // cash the bot refuses to tie up in speculation
    // Refuse every encounter, whatever it costs. Not a strategy -- a probe.
    // Raiders past the halfway mark demand the seed itself, so this is the
    // only way to demonstrate that the empty-handed ending can actually be
    // reached; a bot that pays its way never loses a crate to anyone.
    int  refuse_all;

    // Goods bought at the current stop. Any buy rule and any sell rule can be
    // true of the same good at the same stall -- thresholds that look well
    // separated overlap once the spread and integer rounding are applied --
    // and the result is an infinite buy/sell oscillation. Refusing to sell
    // what was just bought here rules that out structurally, without having to
    // keep two sets of thresholds permanently disjoint.
    int8_t bought_here[GOODS_COUNT];
    int    at_sector, at_index;
} BotRef;

void botref_init(BotRef *b, int float_credits);

// Returns the button to press this step, or -1 when the run has ended.
int  botref_step(BotRef *b, const World *w, int sel, int map_sel, int tab, int title);

#endif
