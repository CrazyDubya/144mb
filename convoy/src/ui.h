// convoy -- screen drawing. Reads state, writes pixels, decides nothing.
//
// Split out of game.c so the state machine stays legible as the number of
// screens grows. Everything here sits on top of render.c and scene.c.
#ifndef UI_H
#define UI_H

#include "state.h"

// The generated desert every in-run screen sits on.
void ui_backdrop(Framebuffer *fb, GameState *gs);
void ui_hud  (Framebuffer *fb, const World *w);
void ui_map  (Framebuffer *fb, GameState *gs);
void ui_trade(Framebuffer *fb, GameState *gs);
void ui_event(Framebuffer *fb, GameState *gs);
void ui_title(Framebuffer *fb, GameState *gs);
void ui_help (Framebuffer *fb, GameState *gs);
void ui_end  (Framebuffer *fb, GameState *gs, int won);

// How many selectable rows the given tab offers. Lets game.c clamp the cursor
// without knowing how any tab is laid out.
int  ui_tab_rows(const GameState *gs, int tab);
// Whether a tab should appear at all. Distinct from its row count: the journal
// is a record with no selectable rows, but it is still a place to go.
int  ui_tab_live(const GameState *gs, int tab);

#endif
