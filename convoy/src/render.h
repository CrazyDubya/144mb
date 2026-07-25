// convoy -- software rasterizer, bitmap font and icon vocabulary.
//
// Everything the player reads must be language-free: shapes, colours, Arabic
// numerals and arrows only. There is deliberately no alphabetic font here, so
// it is impossible to accidentally introduce English text the judges must read.
#ifndef RENDER_H
#define RENDER_H

#include "game.h"

// ---------------------------------------------------------------- palette
enum {
    C_SKY, C_HAZE, C_DUNE_FAR, C_DUNE_NEAR, C_ROAD,
    C_INK, C_PANEL, C_BORDER, C_BONE, C_DIM,
    C_WATER, C_FUEL, C_AMMO, C_MEDS, C_SCRAP,
    C_GOOD, C_BAD, C_WARN, C_GREEN, C_RUST,
    C_SKY_HI, C_SKY_MID, C_SUN, C_SUN_GLOW,
    C_DUNE_MID, C_DUST, C_PANEL_HI, C_PANEL_LO,
    C_COUNT
};
extern const uint32_t PALETTE[C_COUNT];

// ---------------------------------------------------------------- glyphs
// Digits are indices 0..9 so draw_number can index directly.
// G_KEY_Z and G_KEY_X label physical keycaps, not words: the same keys sit in
// the same place on a Korean keyboard as an English one, so they carry no
// language. G_ENTER is the universal return arrow.
enum {
    G_0, G_1, G_2, G_3, G_4, G_5, G_6, G_7, G_8, G_9,
    G_MINUS, G_PLUS, G_UP, G_DOWN, G_X, G_SLASH, G_DOT,
    G_KEY_Z, G_ENTER, G_RIGHT,
    G_A, G_B, G_C, G_D, G_E, G_F, G_G, G_H, G_I, G_J, G_K, G_L, G_M,
    G_N, G_O, G_P, G_Q, G_R, G_S, G_T, G_U, G_V, G_W, G_Y, G_Z,
    G_COLON, G_COMMA, G_EXCL, G_QUES, G_APOS, G_PCT, G_LETTER_X,
    G_COUNT
};

// ---------------------------------------------------------------- goods
enum { ICON_WATER, ICON_FUEL, ICON_AMMO, ICON_MEDS, ICON_SCRAP, ICON_COUNT };

// ---------------------------------------------------------------- api
// ---------------------------------------------------------------- maths
// Fixed-point sine: angle 0..2047 spans a full turn, result -256..256.
// Bhaskara's approximation, so there is no table and no libm.
int32_t isin(int32_t a);
// Blend two packed 0x00RRGGBB colours. t is 0..255.
uint32_t rgb_lerp(uint32_t a, uint32_t b, int t);

// ---------------------------------------------------------------- api
void clear     (Framebuffer *fb, uint32_t rgb);
// Ordered-dither vertical ramp between two colours. The dither is the point:
// it keeps the limited-palette look instead of smoothing into 24-bit mush.
void fill_vgrad(Framebuffer *fb, int x, int y, int w, int h,
                uint32_t top, uint32_t bot);
void fill_disc (Framebuffer *fb, int cx, int cy, int r, uint32_t rgb);
// Dithered overlay at a given density (0..16). There is no alpha channel, so
// darkening is done with a screen-door pattern -- which also happens to be the
// period-correct way to do it.
void fill_scrim(Framebuffer *fb, int x, int y, int w, int h,
                uint32_t rgb, int level);
// Radial dithered glow: dense at the centre, thinning to nothing at the rim.
void fill_glow (Framebuffer *fb, int cx, int cy, int r, uint32_t rgb, int peak);
// Panel with a lit top-left edge, shaded bottom-right, and a soft drop shadow.
void draw_bevel(Framebuffer *fb, int x, int y, int w, int h, int inset);
void draw_drop (Framebuffer *fb, int x, int y, int w, int h);
// A dithered wipe used between screens. `t` runs 0..255 as it clears.
void draw_wipe (Framebuffer *fb, int t);
void fill_rect (Framebuffer *fb, int x, int y, int w, int h, uint32_t rgb);
void draw_rect (Framebuffer *fb, int x, int y, int w, int h, uint32_t rgb);
void draw_panel(Framebuffer *fb, int x, int y, int w, int h);
void draw_line (Framebuffer *fb, int x0, int y0, int x1, int y1, uint32_t rgb);

void draw_glyph (Framebuffer *fb, int x, int y, int glyph, int scale, uint32_t rgb);
int  glyph_w    (int scale);
// Returns the width drawn, so callers can lay out rows without measuring twice.
int  draw_number(Framebuffer *fb, int x, int y, int value, int scale, uint32_t rgb);
int  number_w   (int value, int scale);

// Text. Uppercase-only 5x7, which is both period-correct and a third of the
// glyphs a mixed-case face would need. Lowercase input is folded up.
int  draw_text  (Framebuffer *fb, int x, int y, const char *s, int scale, uint32_t rgb);
int  text_w     (const char *s, int scale);
// Centred on cx. Returns the left edge used.
int  draw_text_c(Framebuffer *fb, int cx, int y, const char *s, int scale, uint32_t rgb);

void draw_icon  (Framebuffer *fb, int x, int y, int icon, int scale);
// Draws a keycap containing a glyph. Returns its width.
int  draw_key   (Framebuffer *fb, int x, int y, int glyph, int scale);
int  key_w      (int scale);
// Small up/down/flat indicator used for price trends.
void draw_trend (Framebuffer *fb, int x, int y, int dir, int scale);

#endif
