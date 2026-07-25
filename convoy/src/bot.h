// convoy -- test-only bot. Compiled into the headless harness only; it adds
// nothing to the submitted executable.
#ifndef BOT_H
#define BOT_H

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
} Bot;

void bot_init(Bot *b, int float_credits);

// Returns the button to press this step, or -1 when the run has ended.
int  bot_step(Bot *b, const World *w, int sel, int map_sel, int tab, int title);

#endif
