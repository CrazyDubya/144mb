// convoy -- the procedural backdrop.
//
// Nothing here is stored. The sky is a dithered ramp, the dunes are sums of
// sines evaluated per column, the sun is a few discs, and the dust motes are
// derived from the tick counter so they carry no state at all. Total cost is a
// few hundred bytes of code and zero bytes of data.
#ifndef SCENE_H
#define SCENE_H

#include "game.h"

// depth 0..7 shifts the palette and the dune profile as the convoy pushes east;
// tension 0..255 thickens the dust.
void scene_draw(Framebuffer *fb, uint32_t tick, int depth, int tension);

// The convoy itself, drawn from rectangles at `s` pixels per design unit
// (roughly 24x14 units). Bobs on the tick; `wrecked` tips it over and swaps the
// dust trail for smoke.
void draw_convoy(Framebuffer *fb, int x, int y, int s, uint32_t tick, int wrecked);

#endif
