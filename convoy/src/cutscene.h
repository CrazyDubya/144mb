// convoy -- cut scenes.
//
// Letterboxed panels with typewriter text over a procedural backdrop. Nothing
// here is stored art: each panel names a small drawing routine and the scene
// is assembled from the same primitives the game already uses.
//
// Kept data-driven so a scene is a table of panels, not a function. That is
// the same reason encounter copy lives in tables: content that needs an edit
// in three places is content that ships broken.
#ifndef CUTSCENE_H
#define CUTSCENE_H

#include "state.h"

// Foreground drawings a panel can call for.
enum {
    ART_LOADING,     // crates going aboard at dawn
    ART_ROAD,        // the convoy running east
    ART_SEEDS,       // what is in the crates
    ART_GREEN,       // the Green Zone
    ART_WRECK,       // the convoy stopped for good
    ART_EMPTY_HOLD,  // arriving with nothing
    ART_STORM,       // weather closing in
    ART_COUNT
};

#define PANEL_LINES 4

typedef struct {
    uint8_t     art;
    uint8_t     depth;              // how far east, for the backdrop palette
    const char *line[PANEL_LINES];  // NULL terminates early
} Panel;

struct Cutscene {
    const Panel *panel;
    uint8_t      count;
};

void cutscene_begin (CutsceneState *s, const Cutscene *cs, uint32_t tick);
// Returns non-zero while the scene still owns the screen. `advance` is a
// keypress: it completes the current panel's text, or moves to the next.
int  cutscene_update(CutsceneState *s, int advance, uint32_t tick);
void cutscene_draw  (Framebuffer *fb, const CutsceneState *s, uint32_t tick);

// The scenes themselves.
extern const Cutscene CS_OPENING;
extern const Cutscene CS_ENDING[];   // indexed by Outcome
// Short one-panel beats shown between sectors. Returns NULL when there is
// nothing to say, which is most of the time.
const Cutscene *cutscene_vignette(const World *w, int *kind);

#endif
