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
} Bot;

void bot_init(Bot *b, int float_credits);

// Returns the button to press this step, or -1 when the run has ended.
int  bot_step(Bot *b, const World *w, int sel, int map_sel, int title);

#endif
