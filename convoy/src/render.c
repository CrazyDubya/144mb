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

void draw_rect(Framebuffer *fb, int x, int y, int w, int h, uint32_t rgb) {
    fill_rect(fb, x, y, w, 1, rgb);
    fill_rect(fb, x, y + h - 1, w, 1, rgb);
    fill_rect(fb, x, y, 1, h, rgb);
    fill_rect(fb, x + w - 1, y, 1, h, rgb);
}

void draw_panel(Framebuffer *fb, int x, int y, int w, int h) {
    fill_rect(fb, x, y, w, h, PALETTE[C_PANEL]);
    draw_rect(fb, x, y, w, h, PALETTE[C_BORDER]);
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
    return x - start;
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
    return w;
}

void draw_trend(Framebuffer *fb, int x, int y, int dir, int scale) {
    if (dir > 0)      draw_glyph(fb, x, y, G_UP,    scale, PALETTE[C_GOOD]);
    else if (dir < 0) draw_glyph(fb, x, y, G_DOWN,  scale, PALETTE[C_BAD]);
    else              draw_glyph(fb, x, y, G_MINUS, scale, PALETTE[C_DIM]);
}
