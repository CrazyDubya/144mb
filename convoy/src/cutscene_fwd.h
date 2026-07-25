// Just enough of the cut scene player for state.h to hold one by value,
// without state.h and cutscene.h including each other.
#ifndef CUTSCENE_FWD_H
#define CUTSCENE_FWD_H

#include <stdint.h>

typedef struct Cutscene Cutscene;

// Vignette kinds, so the caller can fire each at most once. Sectors were the
// wrong key: two of these are conditions rather than places, and keyed by
// sector the loss beat replayed at every remaining stop.
enum { VIG_FIRST_K, VIG_HALF_K, VIG_STORM_K, VIG_LAST_K, VIG_LOSS_K, VIG_COUNT };

typedef struct {
    const Cutscene *cs;
    int      index;
    uint32_t started;
    int      running;
} CutsceneState;

#endif
