// Just enough of the cut scene player for state.h to hold one by value,
// without state.h and cutscene.h including each other.
#ifndef CUTSCENE_FWD_H
#define CUTSCENE_FWD_H

#include <stdint.h>

typedef struct Cutscene Cutscene;

typedef struct {
    const Cutscene *cs;
    int      index;
    uint32_t started;
    int      running;
} CutsceneState;

#endif
