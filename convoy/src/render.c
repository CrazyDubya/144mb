#include "render.h"

// ---------------------------------------------------------------- palette
// Dust-loaded post-apocalyptic range: bleached sky, rust, bone. The goods
// colours are chosen to stay distinguishable from each other at 16px and to
// survive being sat on the dune browns.
const uint32_t PALETTE[C_COUNT] = {
    [C_SKY]       = 0xC9A87C,
    [C_HAZE]      = 0xE0C9A0,
    [C_DUNE_FAR]  = 0x8A6F4E,
    [C_DUNE_NEAR] = 0x6B5238,
    [C_ROAD]      = 0x4A3B2A,
    [C_INK]       = 0x1A1410,
    [C_PANEL]     = 0x2B231A,
    [C_BORDER]    = 0x6E5C42,
    [C_BONE]      = 0xE8DCC0,
    [C_DIM]       = 0x9A8A6E,
    [C_WATER]     = 0x4FA8C9,
    [C_FUEL]      = 0xD9862B,
    [C_AMMO]      = 0xC9A83F,
    [C_MEDS]      = 0xE05A5A,
    [C_SCRAP]     = 0x8A8A93,
    [C_GOOD]      = 0x6FA84B,
    [C_BAD]       = 0xC7402F,
    [C_WARN]      = 0xE0B33F,
    [C_GREEN]     = 0x6FA84B,
    [C_RUST]      = 0xA84B20,
    [C_SKY_HI]    = 0x7A6A5A,   // dust-choked zenith
    [C_SKY_MID]   = 0xB08A5E,
    [C_SUN]       = 0xF7E0A8,
    [C_SUN_GLOW]  = 0xE8B060,
    [C_DUNE_MID]  = 0x7A6044,
    [C_DUST]      = 0xD8C098,
    [C_PANEL_HI]  = 0x4A3D2C,   // lit bevel edge
    [C_PANEL_LO]  = 0x140F0A,   // shaded bevel edge
};

// ---------------------------------------------------------------- maths
int32_t isin(int32_t a) {
    a &= 2047;
    int32_t sign = 1;
    if (a >= 1024) { a -= 1024; sign = -1; }
    int32_t u = a * (1024 - a);              // 0 .. 262144
    int32_t v = (int32_t)((16LL * u * 256) / (5242880 - 4 * u));
    return sign * v;                          // -256 .. 256
}

uint32_t rgb_lerp(uint32_t a, uint32_t b, int t) {
    if (t <= 0) return a;
    if (t >= 255) return b;
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    int r = ar + (br - ar) * t / 255;
    int g = ag + (bg - ag) * t / 255;
    int bl = ab + (bb - ab) * t / 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

// 4x4 ordered dither. Values are the standard Bayer matrix scaled 0..15.
static const uint8_t BAYER[16] = {
     0,  8,  2, 10,
    12,  4, 14,  6,
     3, 11,  1,  9,
    15,  7, 13,  5
};

// ---------------------------------------------------------------- font 8x8
// Five pixels wide inside an 8px cell, MSB on the left.
static const uint8_t FONT[G_COUNT][8] = {
    [G_0]     = { 0x00, 0x7C, 0x44, 0x44, 0x44, 0x44, 0x7C, 0x00 },
    [G_1]     = { 0x00, 0x10, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00 },
    [G_2]     = { 0x00, 0x7C, 0x04, 0x7C, 0x40, 0x40, 0x7C, 0x00 },
    [G_3]     = { 0x00, 0x7C, 0x04, 0x7C, 0x04, 0x04, 0x7C, 0x00 },
    [G_4]     = { 0x00, 0x44, 0x44, 0x7C, 0x04, 0x04, 0x04, 0x00 },
    [G_5]     = { 0x00, 0x7C, 0x40, 0x7C, 0x04, 0x04, 0x7C, 0x00 },
    [G_6]     = { 0x00, 0x7C, 0x40, 0x7C, 0x44, 0x44, 0x7C, 0x00 },
    [G_7]     = { 0x00, 0x7C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00 },
    [G_8]     = { 0x00, 0x7C, 0x44, 0x7C, 0x44, 0x44, 0x7C, 0x00 },
    [G_9]     = { 0x00, 0x7C, 0x44, 0x7C, 0x04, 0x04, 0x7C, 0x00 },
    [G_MINUS] = { 0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00 },
    [G_PLUS]  = { 0x00, 0x10, 0x10, 0x7C, 0x10, 0x10, 0x00, 0x00 },
    [G_UP]    = { 0x00, 0x10, 0x38, 0x54, 0x10, 0x10, 0x10, 0x00 },
    [G_DOWN]  = { 0x00, 0x10, 0x10, 0x10, 0x54, 0x38, 0x10, 0x00 },
    [G_X]     = { 0x00, 0x00, 0x44, 0x28, 0x10, 0x28, 0x44, 0x00 },
    [G_SLASH] = { 0x00, 0x04, 0x08, 0x10, 0x10, 0x20, 0x40, 0x00 },
    [G_DOT]   = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00 },
    [G_KEY_Z] = { 0x00, 0x7C, 0x08, 0x10, 0x20, 0x40, 0x7C, 0x00 },
    [G_ENTER] = { 0x00, 0x04, 0x04, 0x24, 0x44, 0xFC, 0x40, 0x20 },
    [G_RIGHT] = { 0x00, 0x10, 0x08, 0xFC, 0x08, 0x10, 0x00, 0x00 },

    // Uppercase alphabet, five pixels wide inside the eight-pixel cell.
    [G_A] = { 0x00, 0x38, 0x44, 0x44, 0x7C, 0x44, 0x44, 0x00 },
    [G_B] = { 0x00, 0x78, 0x44, 0x78, 0x44, 0x44, 0x78, 0x00 },
    [G_C] = { 0x00, 0x38, 0x44, 0x40, 0x40, 0x44, 0x38, 0x00 },
    [G_D] = { 0x00, 0x78, 0x44, 0x44, 0x44, 0x44, 0x78, 0x00 },
    [G_E] = { 0x00, 0x7C, 0x40, 0x78, 0x40, 0x40, 0x7C, 0x00 },
    [G_F] = { 0x00, 0x7C, 0x40, 0x78, 0x40, 0x40, 0x40, 0x00 },
    [G_G] = { 0x00, 0x38, 0x44, 0x40, 0x4C, 0x44, 0x3C, 0x00 },
    [G_H] = { 0x00, 0x44, 0x44, 0x7C, 0x44, 0x44, 0x44, 0x00 },
    [G_I] = { 0x00, 0x38, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00 },
    [G_J] = { 0x00, 0x1C, 0x08, 0x08, 0x08, 0x48, 0x30, 0x00 },
    [G_K] = { 0x00, 0x44, 0x48, 0x70, 0x48, 0x44, 0x44, 0x00 },
    [G_L] = { 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7C, 0x00 },
    [G_M] = { 0x00, 0x44, 0x6C, 0x54, 0x44, 0x44, 0x44, 0x00 },
    [G_N] = { 0x00, 0x44, 0x64, 0x54, 0x4C, 0x44, 0x44, 0x00 },
    [G_O] = { 0x00, 0x38, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00 },
    [G_P] = { 0x00, 0x78, 0x44, 0x44, 0x78, 0x40, 0x40, 0x00 },
    [G_Q] = { 0x00, 0x38, 0x44, 0x44, 0x54, 0x48, 0x34, 0x00 },
    [G_R] = { 0x00, 0x78, 0x44, 0x44, 0x78, 0x48, 0x44, 0x00 },
    [G_S] = { 0x00, 0x3C, 0x40, 0x38, 0x04, 0x04, 0x78, 0x00 },
    [G_T] = { 0x00, 0x7C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00 },
    [G_U] = { 0x00, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00 },
    [G_V] = { 0x00, 0x44, 0x44, 0x44, 0x44, 0x28, 0x10, 0x00 },
    [G_W] = { 0x00, 0x44, 0x44, 0x44, 0x54, 0x6C, 0x44, 0x00 },
    [G_Y] = { 0x00, 0x44, 0x44, 0x28, 0x10, 0x10, 0x10, 0x00 },
    [G_Z] = { 0x00, 0x7C, 0x08, 0x10, 0x20, 0x40, 0x7C, 0x00 },

    [G_COLON] = { 0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x00, 0x00 },
    [G_COMMA] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x20 },
    [G_EXCL]  = { 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10, 0x00 },
    [G_QUES]  = { 0x00, 0x38, 0x44, 0x08, 0x10, 0x00, 0x10, 0x00 },
    [G_APOS]  = { 0x00, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00 },
    [G_PCT]   = { 0x00, 0x44, 0x08, 0x10, 0x20, 0x40, 0x44, 0x00 },
    // The multiplication sign sits low and short; the letter needs
    // full cap height or it reads as a different size mid-word.
    [G_LETTER_X] = { 0x00, 0x44, 0x44, 0x28, 0x10, 0x28, 0x44, 0x00 },
};

// ---------------------------------------------------------------- icons 16x16
// Authored as ASCII art: '.' transparent, '1' body, '2' highlight, '3' shadow.
// Cheap in bytes, and editable without a tool.
static const char *const ART_WATER[16] = {
    "................",
    ".......22.......",
    "......2112......",
    "......1111......",
    ".....111111.....",
    ".....111111.....",
    "....11111111....",
    "....11111111....",
    "...1111111111...",
    "...1113111111...",
    "...1113111111...",
    "...1111111111...",
    "....11111111....",
    ".....111111.....",
    "......1111......",
    "................",
};
static const char *const ART_FUEL[16] = {
    "................",
    "......11........",
    "....111111......",
    "...11111111.....",
    "...11111111.....",
    "...11222111.....",
    "...11222111.....",
    "...11111111.....",
    "...11111111.....",
    "...11111111.....",
    "...11111111.....",
    "...11111111.....",
    "...11111111.....",
    "....111111......",
    "................",
    "................",
};
static const char *const ART_AMMO[16] = {
    "................",
    "..2...2...2.....",
    ".111.111.111....",
    ".111.111.111....",
    ".111.111.111....",
    ".111.111.111....",
    ".111.111.111....",
    ".111.111.111....",
    ".111.111.111....",
    ".111.111.111....",
    ".333.333.333....",
    ".333.333.333....",
    ".333.333.333....",
    "................",
    "................",
    "................",
};
static const char *const ART_MEDS[16] = {
    "................",
    "................",
    "......1111......",
    "......1111......",
    "......1111......",
    "..111111111111..",
    "..111111111111..",
    "..111111111111..",
    "..111111111111..",
    "......1111......",
    "......1111......",
    "......1111......",
    "................",
    "................",
    "................",
    "................",
};
static const char *const ART_SCRAP[16] = {
    "................",
    "................",
    "....11111111....",
    "...1111111111...",
    "..111111111111..",
    "..1111....1111..",
    "..111......111..",
    "..111......111..",
    "..111......111..",
    "..1111....1111..",
    "..111111111111..",
    "...1111111111...",
    "....11111111....",
    "................",
    "................",
    "................",
};

typedef struct {
    const char *const *art;
    uint32_t body, hi, lo;
} IconDef;

static const IconDef ICONS[ICON_COUNT] = {
    [ICON_WATER] = { ART_WATER, 0x4FA8C9, 0xA8DCEE, 0x2A6F8C },
    [ICON_FUEL]  = { ART_FUEL,  0xD9862B, 0xF0C070, 0x8A4E12 },
    [ICON_AMMO]  = { ART_AMMO,  0xC9A83F, 0xF0DC90, 0x7A6420 },
    [ICON_MEDS]  = { ART_MEDS,  0xE05A5A, 0xF7A0A0, 0x8C2F20 },
    [ICON_SCRAP] = { ART_SCRAP, 0x8A8A93, 0xC4C4CC, 0x4E4E56 },
};

// ---------------------------------------------------------------- primitives
void clear(Framebuffer *fb, uint32_t rgb) {
    int n = fb->w * fb->h;
    for (int i = 0; i < n; ++i) fb->pixels[i] = rgb;
}

void fill_rect(Framebuffer *fb, int x, int y, int w, int h, uint32_t rgb) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w, y1 = y + h;
    if (x1 > fb->w) x1 = fb->w;
    if (y1 > fb->h) y1 = fb->h;
    for (int py = y0; py < y1; ++py) {
        uint32_t *row = fb->pixels + py * fb->w;
        for (int px = x0; px < x1; ++px) row[px] = rgb;
    }
}

void fill_vgrad(Framebuffer *fb, int x, int y, int w, int h,
                uint32_t top, uint32_t bot) {
    if (h <= 0) return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w, y1 = y + h;
    if (x1 > fb->w) x1 = fb->w;
    if (y1 > fb->h) y1 = fb->h;

    for (int py = y0; py < y1; ++py) {
        int t = (py - y) * 17 / h;            // 0..16 down the band
        uint32_t *row = fb->pixels + py * fb->w;
        const uint8_t *brow = BAYER + ((py & 3) << 2);
        for (int px = x0; px < x1; ++px)
            row[px] = (t > brow[px & 3]) ? bot : top;
    }
}

void fill_scrim(Framebuffer *fb, int x, int y, int w, int h,
                uint32_t rgb, int level) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w, y1 = y + h;
    if (x1 > fb->w) x1 = fb->w;
    if (y1 > fb->h) y1 = fb->h;
    for (int py = y0; py < y1; ++py) {
        uint32_t *row = fb->pixels + py * fb->w;
        const uint8_t *brow = BAYER + ((py & 3) << 2);
        for (int px = x0; px < x1; ++px)
            if (level > brow[px & 3]) row[px] = rgb;
    }
}

void fill_glow(Framebuffer *fb, int cx, int cy, int r, uint32_t rgb, int peak) {
    if (r <= 0) return;
    int y0 = cy - r < 0 ? 0 : cy - r, y1 = cy + r;
    int x0 = cx - r < 0 ? 0 : cx - r, x1 = cx + r;
    if (y1 > fb->h) y1 = fb->h;
    if (x1 > fb->w) x1 = fb->w;
    int rr = r * r;

    for (int py = y0; py < y1; ++py) {
        int dy = py - cy;
        uint32_t *row = fb->pixels + py * fb->w;
        const uint8_t *brow = BAYER + ((py & 3) << 2);
        for (int px = x0; px < x1; ++px) {
            int dx = px - cx;
            int d2 = dx * dx + dy * dy;
            if (d2 >= rr) continue;
            int level = peak - peak * d2 / rr;   // dense centre, thin rim
            if (level > brow[px & 3]) row[px] = rgb;
        }
    }
}

void fill_disc(Framebuffer *fb, int cx, int cy, int r, uint32_t rgb) {
    if (r <= 0) return;
    int y0 = cy - r < 0 ? 0 : cy - r, y1 = cy + r;
    if (y1 > fb->h) y1 = fb->h;
    int rr = r * r;
    for (int py = y0; py < y1; ++py) {
        int dy = py - cy, span = rr - dy * dy;
        if (span < 0) continue;
        // Integer square root by descent; radii here are small.
        int dx = 0;
        while ((dx + 1) * (dx + 1) <= span) ++dx;
        fill_rect(fb, cx - dx, py, dx * 2, 1, rgb);
    }
}

void draw_rect(Framebuffer *fb, int x, int y, int w, int h, uint32_t rgb) {
    fill_rect(fb, x, y, w, 1, rgb);
    fill_rect(fb, x, y + h - 1, w, 1, rgb);
    fill_rect(fb, x, y, 1, h, rgb);
    fill_rect(fb, x + w - 1, y, 1, h, rgb);
}

// Two rows of increasingly transparent-looking shadow. There is no alpha in the
// framebuffer, so the "softness" comes from stepping through darker browns.
void draw_drop(Framebuffer *fb, int x, int y, int w, int h) {
    fill_rect(fb, x + 6, y + h, w, 3, 0x241C13);
    fill_rect(fb, x + w, y + 6, 3, h - 3, 0x241C13);
    fill_rect(fb, x + 4, y + h + 3, w, 2, 0x2E251A);
    fill_rect(fb, x + w + 3, y + 4, 2, h, 0x2E251A);
}

void draw_bevel(Framebuffer *fb, int x, int y, int w, int h, int inset) {
    uint32_t lit = PALETTE[inset ? C_PANEL_LO : C_PANEL_HI];
    uint32_t shd = PALETTE[inset ? C_PANEL_HI : C_PANEL_LO];
    fill_rect(fb, x, y, w, 1, lit);
    fill_rect(fb, x, y, 1, h, lit);
    fill_rect(fb, x, y + h - 1, w, 1, shd);
    fill_rect(fb, x + w - 1, y, 1, h, shd);
}

void draw_panel(Framebuffer *fb, int x, int y, int w, int h) {
    draw_drop(fb, x, y, w, h);
    // A faint internal ramp stops large panels reading as flat slabs.
    fill_vgrad(fb, x, y, w, h, PALETTE[C_PANEL], PALETTE[C_INK]);
    draw_bevel(fb, x, y, w, h, 0);
    draw_rect(fb, x + 1, y + 1, w - 2, h - 2, PALETTE[C_BORDER]);
}

void draw_line(Framebuffer *fb, int x0, int y0, int x1, int y1, uint32_t rgb) {
    int dx = x1 - x0, dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    int err = adx - ady;
    for (;;) {
        if (x0 >= 0 && x0 < fb->w && y0 >= 0 && y0 < fb->h)
            fb->pixels[y0 * fb->w + x0] = rgb;
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -ady) { err -= ady; x0 += sx; }
        if (e2 <  adx) { err += adx; y0 += sy; }
    }
}

// ---------------------------------------------------------------- ui probe
// See render.h for why this exists. Harness only.
#ifdef CONVOY_INSTRUMENT
enum { PROBE_MAX_BOX = 256, PROBE_MAX_PAIR = 64 };

static TextBox probe_box[PROBE_MAX_BOX];
static TextBox probe_a[PROBE_MAX_PAIR], probe_b[PROBE_MAX_PAIR];
static int     probe_nbox, probe_npair, probe_on;

void render_probe_enable(int on) { probe_on = on; }
void render_probe_reset (void)   { probe_nbox = 0; probe_npair = 0; }
int  render_probe_overlaps(void) { return probe_npair; }
int  render_probe_boxes (void)   { return probe_nbox; }

int render_probe_pair(int i, TextBox *a, TextBox *b) {
    if (i < 0 || i >= probe_npair || i >= PROBE_MAX_PAIR) return 0;
    *a = probe_a[i];
    *b = probe_b[i];
    return 1;
}

static void probe_label(char *dst, const char *s) {
    int i = 0;
    if (s) for (; s[i] && i < (int)sizeof probe_box[0].s - 1; ++i) dst[i] = s[i];
    dst[i] = '\0';
}

// Half-open rectangles, so two runs of text laid out edge to edge -- which is
// what `tx += draw_text(...)` produces all over the UI -- touch without
// colliding. Only a real overlap of drawn area is reported.
static int probe_hit(const TextBox *a, const TextBox *b) {
    return a->x < b->x + b->w && b->x < a->x + a->w &&
           a->y < b->y + b->h && b->y < a->y + a->h;
}

// The one text-on-text overlap that is deliberate: the same string drawn twice,
// same size, a couple of pixels apart, is a drop shadow or an outline -- the
// title does exactly that. Nothing else is excused; a panel or a highlight
// under text is not text, so it never reaches here in the first place.
static int probe_shadow(const TextBox *a, const TextBox *b) {
    int dx = a->x - b->x, dy = a->y - b->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx > 4 || dy > 4 || a->w != b->w || a->h != b->h || !a->s[0]) return 0;
    for (int i = 0; i < (int)sizeof a->s; ++i) {
        if (a->s[i] != b->s[i]) return 0;
        if (!a->s[i]) break;
    }
    return 1;
}

// Records one drawn box, clipped to the framebuffer: text scrolled off the
// edge is not on screen and cannot collide with anything that is.
static void probe_add(const Framebuffer *fb, int x, int y, int w, int h,
                      const char *s) {
    if (!probe_on || w <= 0 || h <= 0) return;

    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb->w) x1 = fb->w;
    if (y1 > fb->h) y1 = fb->h;
    if (x1 <= x0 || y1 <= y0) return;

    TextBox nb;
    nb.x = x0; nb.y = y0; nb.w = x1 - x0; nb.h = y1 - y0;
    probe_label(nb.s, s);

    for (int i = 0; i < probe_nbox; ++i) {
        if (!probe_hit(&nb, &probe_box[i])) continue;
        if (probe_shadow(&nb, &probe_box[i])) continue;
        if (probe_npair < PROBE_MAX_PAIR) {
            probe_a[probe_npair] = probe_box[i];
            probe_b[probe_npair] = nb;
        }
        ++probe_npair;
    }
    if (probe_nbox < PROBE_MAX_BOX) probe_box[probe_nbox++] = nb;
}
#endif

// ---------------------------------------------------------------- text
int glyph_w(int scale) { return 6 * scale; }   // 5px glyph + 1px gap

void draw_glyph(Framebuffer *fb, int x, int y, int glyph, int scale, uint32_t rgb) {
    if (glyph < 0 || glyph >= G_COUNT) return;
    const uint8_t *rows = FONT[glyph];
    for (int gy = 0; gy < 8; ++gy) {
        uint8_t bits = rows[gy];
        for (int gx = 0; gx < 8; ++gx) {
            if (bits & (0x80 >> gx))
                fill_rect(fb, x + gx * scale, y + gy * scale, scale, scale, rgb);
        }
    }
}

int number_w(int value, int scale) {
    int digits = 1, v = value < 0 ? -value : value;
    while (v >= 10) { v /= 10; ++digits; }
    return (digits + (value < 0 ? 1 : 0)) * glyph_w(scale);
}

int draw_number(Framebuffer *fb, int x, int y, int value, int scale, uint32_t rgb) {
    int start = x;
#ifdef CONVOY_INSTRUMENT
    int probe_val = value;
#endif
    if (value < 0) {
        draw_glyph(fb, x, y, G_MINUS, scale, rgb);
        x += glyph_w(scale);
        value = -value;
    }
    int digits[10], n = 0;
    do { digits[n++] = value % 10; value /= 10; } while (value);
    while (n--) {
        draw_glyph(fb, x, y, G_0 + digits[n], scale, rgb);
        x += glyph_w(scale);
    }
#ifdef CONVOY_INSTRUMENT
    {
        // The font's ink lives in rows 1..6 of the 8-row cell, so the box is
        // the ink and not the leading: two lines 15px apart at scale 1 do not
        // register as a collision, which they would if the cell were used.
        char lab[24];
        int  li = 0, v2 = probe_val, neg = (v2 < 0);
        char tmp[12]; int tn = 0;
        if (neg) v2 = -v2;
        do { tmp[tn++] = (char)('0' + v2 % 10); v2 /= 10; } while (v2 && tn < 11);
        if (neg) lab[li++] = '-';
        while (tn--) lab[li++] = tmp[tn];
        lab[li] = '\0';
        probe_add(fb, start, y + scale, x - start, 6 * scale, lab);
    }
#endif
    return x - start;
}

// 'X' reuses the multiplication glyph rather than duplicating an identical
// shape; every other letter has its own entry.
static int glyph_for(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c >= '0' && c <= '9') return G_0 + (c - '0');
    if (c == 'X') return G_LETTER_X;
    if (c >= 'A' && c <= 'W') return G_A + (c - 'A');
    if (c == 'Y') return G_Y;
    if (c == 'Z') return G_Z;
    switch (c) {
    case '-':  return G_MINUS;
    case '+':  return G_PLUS;
    case '/':  return G_SLASH;
    case '.':  return G_DOT;
    case ':':  return G_COLON;
    case ',':  return G_COMMA;
    case '!':  return G_EXCL;
    case '?':  return G_QUES;
    case '\'': return G_APOS;
    case '%':  return G_PCT;
    default:   return -1;    // space, and anything unmapped
    }
}

int text_w(const char *s, int scale) {
    int n = 0;
    while (*s++) ++n;
    return n * glyph_w(scale);
}

int draw_text(Framebuffer *fb, int x, int y, const char *s, int scale, uint32_t rgb) {
    int start = x;
#ifdef CONVOY_INSTRUMENT
    const char *probe_s = s;
    int probe_first = -1, probe_last = -1;   // ink extent, so leading and
#endif                                       // trailing spaces do not collide
    for (; *s; ++s) {
        int g = glyph_for(*s);
        if (g >= 0) draw_glyph(fb, x, y, g, scale, rgb);
#ifdef CONVOY_INSTRUMENT
        if (g >= 0) { if (probe_first < 0) probe_first = x; probe_last = x; }
#endif
        x += glyph_w(scale);
    }
#ifdef CONVOY_INSTRUMENT
    if (probe_first >= 0)
        probe_add(fb, probe_first, y + scale,
                  probe_last + glyph_w(scale) - probe_first, 6 * scale, probe_s);
#endif
    return x - start;
}

int draw_text_c(Framebuffer *fb, int cx, int y, const char *s, int scale, uint32_t rgb) {
    int x = cx - text_w(s, scale) / 2;
    draw_text(fb, x, y, s, scale, rgb);
    return x;
}

// ---------------------------------------------------------------- icons
void draw_icon(Framebuffer *fb, int x, int y, int icon, int scale) {
    if (icon < 0 || icon >= ICON_COUNT) return;
    const IconDef *def = &ICONS[icon];
    for (int iy = 0; iy < 16; ++iy) {
        const char *row = def->art[iy];
        for (int ix = 0; ix < 16; ++ix) {
            uint32_t c;
            switch (row[ix]) {
            case '1': c = def->body; break;
            case '2': c = def->hi;   break;
            case '3': c = def->lo;   break;
            default:  continue;      // '.' is transparent
            }
            fill_rect(fb, x + ix * scale, y + iy * scale, scale, scale, c);
        }
    }
}

int key_w(int scale) { return 8 * scale + 6; }

// A keycap: bone-bordered box with the key's symbol inside. Placed inline
// beside the thing it acts on, so the game needs no legend and no instructions.
int draw_key(Framebuffer *fb, int x, int y, int glyph, int scale) {
    int w = key_w(scale), h = 8 * scale + 6;
    fill_rect(fb, x, y, w, h, PALETTE[C_INK]);
    draw_rect(fb, x, y, w, h, PALETTE[C_BONE]);
    draw_glyph(fb, x + 3, y + 3, glyph, scale, PALETTE[C_BONE]);
#ifdef CONVOY_INSTRUMENT
    // The whole keycap, not just the symbol: the cap is opaque, so anything
    // under it is hidden by it.
    {
        static const char *const KEYNAME[] = {
            "<KEY 0>", "<KEY 1>", "<KEY 2>", "<KEY 3>", "<KEY 4>",
            "<KEY 5>", "<KEY 6>", "<KEY 7>", "<KEY 8>", "<KEY 9>"
        };
        const char *lab = "<KEY>";
        if (glyph >= G_0 && glyph <= G_9)   lab = KEYNAME[glyph - G_0];
        else if (glyph == G_KEY_Z)          lab = "<KEY Z>";
        else if (glyph == G_ENTER)          lab = "<KEY ENTER>";
        else if (glyph == G_UP)             lab = "<KEY UP>";
        else if (glyph == G_DOWN)           lab = "<KEY DOWN>";
        else if (glyph == G_RIGHT)          lab = "<KEY RIGHT>";
        else if (glyph == G_X)              lab = "<KEY X>";
        probe_add(fb, x, y, w, h, lab);
    }
#endif
    return w;
}

// Up means this market charges more than the player has seen elsewhere, which
// is where you want to be selling; down means cheap, which is where you buy.
// Colour follows opportunity, not direction.
// 16x16 design units at `s` pixels each. Everything -- skin, headwear, eyes,
// the set of the mouth -- comes off the seed, so five characters cost no art
// and a sixth costs none either. `mood` is -1..1 and only moves the mouth.
void draw_portrait(Framebuffer *fb, int x, int y, int s, uint32_t seed, int mood) {
    static const uint32_t SKIN[4] = { 0xC9926A, 0x8A5F42, 0xE0B48A, 0x6E4630 };
    static const uint32_t CLOTH[4]= { 0xA84B20, 0x4A6B7A, 0x7A6044, 0x5A4A6B };

    uint32_t h = seed * 2654435761u;
    uint32_t skin  = SKIN [(h >> 3) & 3];
    uint32_t cloth = CLOTH[(h >> 7) & 3];
    int hat   = (h >> 11) & 3;      // 0 none, 1 cap, 2 wrap, 3 goggles-up
    int beard = (h >> 15) & 1;
    int scar  = (h >> 17) & 1;

    // Shoulders, then head.
    fill_rect(fb, x,          y + 11 * s, 16 * s, 5 * s, cloth);
    fill_rect(fb, x + 3 * s,  y + 2 * s,  10 * s, 10 * s, skin);
    fill_rect(fb, x + 2 * s,  y + 5 * s,   1 * s,  4 * s, skin);   // ears
    fill_rect(fb, x + 13 * s, y + 5 * s,   1 * s,  4 * s, skin);

    // Eyes: a dark bar each, which reads at 16px far better than pupils.
    fill_rect(fb, x + 5 * s, y + 6 * s, 2 * s, 2 * s, PALETTE[C_INK]);
    fill_rect(fb, x + 9 * s, y + 6 * s, 2 * s, 2 * s, PALETTE[C_INK]);

    if (beard) fill_rect(fb, x + 4 * s, y + 9 * s, 8 * s, 3 * s,
                         rgb_lerp(skin, PALETTE[C_INK], 130));
    if (scar)  fill_rect(fb, x + 4 * s, y + 4 * s, 1 * s, 5 * s,
                         rgb_lerp(skin, PALETTE[C_BAD], 120));

    // Mouth: flat, down or up. One row of pixels carries the whole attitude.
    int my = y + 10 * s;
    if (mood < 0) {
        fill_rect(fb, x + 6 * s, my,          4 * s, s, PALETTE[C_INK]);
        fill_rect(fb, x + 5 * s, my - s,      1 * s, s, PALETTE[C_INK]);
        fill_rect(fb, x + 10 * s, my - s,     1 * s, s, PALETTE[C_INK]);
    } else if (mood > 0) {
        fill_rect(fb, x + 6 * s, my,          4 * s, s, PALETTE[C_INK]);
        fill_rect(fb, x + 5 * s, my + s,      1 * s, s, PALETTE[C_INK]);
        fill_rect(fb, x + 10 * s, my + s,     1 * s, s, PALETTE[C_INK]);
    } else {
        fill_rect(fb, x + 6 * s, my, 5 * s, s, PALETTE[C_INK]);
    }

    switch (hat) {
    case 1:  // cap with a brim
        fill_rect(fb, x + 3 * s, y,         10 * s, 3 * s, cloth);
        fill_rect(fb, x + 2 * s, y + 3 * s, 12 * s, 1 * s, PALETTE[C_INK]);
        break;
    case 2:  // head wrap against the dust
        fill_rect(fb, x + 2 * s, y + s,     12 * s, 4 * s, PALETTE[C_BONE]);
        fill_rect(fb, x + 2 * s, y + 5 * s,  2 * s, 6 * s, PALETTE[C_BONE]);
        break;
    case 3:  // goggles pushed up on the forehead
        fill_rect(fb, x + 3 * s, y + 2 * s, 10 * s, 2 * s, PALETTE[C_ROAD]);
        fill_rect(fb, x + 4 * s, y + 2 * s,  3 * s, 2 * s, PALETTE[C_SKY]);
        fill_rect(fb, x + 9 * s, y + 2 * s,  3 * s, 2 * s, PALETTE[C_SKY]);
        break;
    default: break;
    }

    draw_bevel(fb, x, y, 16 * s, 16 * s, 0);
}

void draw_trend(Framebuffer *fb, int x, int y, int dir, int scale) {
    if (dir > 0)      draw_glyph(fb, x, y, G_UP,    scale, PALETTE[C_WARN]);
    else if (dir < 0) draw_glyph(fb, x, y, G_DOWN,  scale, PALETTE[C_GOOD]);
    else              draw_glyph(fb, x, y, G_MINUS, scale, PALETTE[C_DIM]);
}
