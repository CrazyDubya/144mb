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
#include "cutscene_fwd.h"

// Tabs on the settlement screen. Switched with left/right.
enum { TAB_MARKET, TAB_GARAGE, TAB_CREW, TAB_CONTRACTS, TAB_JOURNAL, TAB_COUNT };

typedef struct {
    World      w;
    uint32_t   tick;
    uint32_t   seed;
    int        sel;        // selected row within the current tab
    int        map_sel;    // index into the reachable-node list on the map
    int        tab;        // which settlement tab is showing
    int        title;      // showing the title screen rather than a run

    // Title-screen choices. The daily seed comes from the platform layer,
    // which is the only part of the program allowed to know the date; the
    // core just receives a number, so a run stays reproducible from it.
    int        diff;       // DIFF_*, chosen before the run starts
    int        daily;      // 1 = play today's fixed seed
    int        menu_row;   // which title row the cursor is on
    uint32_t   daily_seed;

    uint8_t    vignette_seen[SECTORS];   // each beat fires once
    int        help;       // showing the instructions overlay
    AudioState audio;

    // A dithered wipe on screen changes, and the convoy driving the map link
    // on a hop rather than teleporting. Both are pure presentation: the
    // simulation has already moved on.
    int      trans;          // ticks left in the wipe
    int      travel;         // ticks left in the drive
    uint8_t  from_sector, from_index;

    // Cut scenes own the screen while they run: the opening, the endings and
    // the short beats between sectors all share one player.
    CutsceneState cut;
} GameState;

#endif
