// convoy -- the run's complete state.
//
// Shared by game.c (which owns the state machine and input) and ui.c (which
// only ever reads it). Kept in its own header so neither has to include the
// other.
#ifndef STATE_H
#define STATE_H

#include "game.h"
#include "world.h"
#include "audio.h"

// Tabs on the settlement screen. Switched with left/right.
enum { TAB_MARKET, TAB_GARAGE, TAB_CREW, TAB_CONTRACTS, TAB_COUNT };

typedef struct {
    World      w;
    uint32_t   tick;
    uint32_t   seed;
    int        sel;        // selected row within the current tab
    int        map_sel;    // index into the reachable-node list on the map
    int        tab;        // which settlement tab is showing
    int        title;      // showing the title screen rather than a run
    int        help;       // showing the instructions overlay
    AudioState audio;
} GameState;

#endif
