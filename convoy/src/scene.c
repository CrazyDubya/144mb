#include "scene.h"
#include "render.h"

#define HORIZON (FB_H * 42 / 100)

// Each dune layer: amplitude, two spatial frequencies, a phase offset, how fast
// it parallaxes, and its colour. Three layers is enough to read as depth.
typedef struct {
    int16_t  amp, f1, f2, phase, drift;
    uint8_t  lit, shade;   // dithered between, so the sand has grain
} DuneLayer;

static const DuneLayer LAYERS[3] = {
    {  14,  7, 17,   0, 1, C_DUNE_FAR,  C_DUNE_MID  },
    {  22, 11,  5, 700, 2, C_DUNE_MID,  C_DUNE_NEAR },
    {  34,  5, 13, 300, 4, C_DUNE_NEAR, C_ROAD      },
};

// Height of a dune layer at column x, as an offset below the horizon.
static int dune_y(const DuneLayer *l, int x, int depth) {
    int a = x * l->f1 + l->phase;
    int b = x * l->f2 + l->phase * 2;
    int v = (isin(a) * l->amp + isin(b) * l->amp / 2) / 256;
    return v + depth;   // the land rises as you go east
}

void draw_convoy(Framebuffer *fb, int x, int y, int s, uint32_t tick, int wrecked) {
    // Suspension bob: one unit up and down, unless it is no longer going
    // anywhere.
    int bob = wrecked ? 0 : (isin((int32_t)tick * 24) > 0 ? 0 : 1) * s;
    y += bob;

    uint32_t body = PALETTE[wrecked ? C_ROAD : C_RUST];
    uint32_t trim = PALETTE[wrecked ? C_PANEL : C_DUNE_NEAR];

    // Dust kicked up behind, or smoke going straight up.
    for (int i = 0; i < 7; ++i) {
        uint32_t h = (uint32_t)i * 2654435761u;
        if (wrecked) {
            int px = x + 6 * s + (int)((h >> 9) % 5u) * s - 2 * s;
            int py = y - 4 * s - i * 3 * s - (int)((tick / 3 + i * 5) % 20u) * s / 2;
            fill_rect(fb, px, py, s * 2, s * 2, PALETTE[C_DIM]);
        } else {
            int px = x - 4 * s - i * 3 * s - (int)((tick / 2 + i * 7) % 12u) * s / 2;
            int py = y + 10 * s + (int)((h >> 11) % 3u) * s;
            int sz = (7 - i) * s / 3 + s;
            fill_rect(fb, px, py, sz, sz, PALETTE[C_DUST]);
        }
    }

    if (wrecked) y += 2 * s;

    // Trailer, its load, then the cab.
    fill_rect(fb, x,            y + 2 * s,  16 * s, 8 * s, body);
    fill_rect(fb, x,            y + 8 * s,  16 * s, 2 * s, PALETTE[C_ROAD]);
    fill_rect(fb, x + 2 * s,    y - 1 * s,   5 * s, 3 * s, trim);
    fill_rect(fb, x + 8 * s,    y,           4 * s, 2 * s, PALETTE[C_DUNE_MID]);

    fill_rect(fb, x + 16 * s,   y + 3 * s,   7 * s, 7 * s, body);
    fill_rect(fb, x + 18 * s,   y + 4 * s,   4 * s, 3 * s,
              PALETTE[wrecked ? C_INK : C_SKY]);          // windscreen
    fill_rect(fb, x + 23 * s,   y + 7 * s,   1 * s, 3 * s, PALETTE[C_ROAD]);

    // Wheels.
    int wy = y + 10 * s;
    fill_disc(fb, x + 4 * s,  wy, 2 * s, PALETTE[C_INK]);
    fill_disc(fb, x + 12 * s, wy, 2 * s, PALETTE[C_INK]);
    fill_disc(fb, x + 20 * s, wy, 2 * s, PALETTE[C_INK]);
    if (!wrecked && s > 1) {
        fill_rect(fb, x + 4 * s - s / 2,  wy - s / 2, s, s, PALETTE[C_DIM]);
        fill_rect(fb, x + 12 * s - s / 2, wy - s / 2, s, s, PALETTE[C_DIM]);
        fill_rect(fb, x + 20 * s - s / 2, wy - s / 2, s, s, PALETTE[C_DIM]);
    }
}

// The sky across a whole run, as four keyframes blended by phase. Starting at
// first light and finishing after dark gives the journey a shape you can read
// at a glance, without a clock or a day counter.
typedef struct { uint32_t hi, mid, low, sun, glow, land; } SkyKey;

static const SkyKey SKY_KEY[4] = {
    /* dawn  */ { 0x2E2A3E, 0x7A5A62, 0xC98F70, 0xFFE6BE, 0xE8905A, 0x5A4A44 },
    /* noon  */ { 0x7A6A5A, 0xB08A5E, 0xC9A87C, 0xF7E0A8, 0xE8B060, 0x8A6F4E },
    /* dusk  */ { 0x4A3A52, 0xA05C42, 0xD08A46, 0xFFD08A, 0xE07038, 0x6B4A38 },
    /* night */ { 0x0E1020, 0x1E2438, 0x38405A, 0xBFD0E8, 0x5A6A90, 0x2A2A36 },
};

static SkyKey sky_at(int phase) {
    if (phase < 0) phase = 0;
    if (phase > 255) phase = 255;
    int seg = phase * 3 / 256;            // 0..2
    int t   = (phase * 3 - seg * 256) % 256;
    const SkyKey *a = &SKY_KEY[seg], *b = &SKY_KEY[seg + 1];
    SkyKey k;
    k.hi   = rgb_lerp(a->hi,   b->hi,   t);
    k.mid  = rgb_lerp(a->mid,  b->mid,  t);
    k.low  = rgb_lerp(a->low,  b->low,  t);
    k.sun  = rgb_lerp(a->sun,  b->sun,  t);
    k.glow = rgb_lerp(a->glow, b->glow, t);
    k.land = rgb_lerp(a->land, b->land, t);
    return k;
}

void scene_draw(Framebuffer *fb, uint32_t tick, int depth, int tension,
                int phase, int weather) {
    int horizon = HORIZON;
    SkyKey sky = sky_at(phase);

    // ---- sky ---------------------------------------------------------
    int bleach = depth * 3;
    fill_vgrad(fb, 0, 0, fb->w, horizon * 2 / 3, sky.hi, sky.mid);
    fill_vgrad(fb, 0, horizon * 2 / 3, fb->w, horizon - horizon * 2 / 3 + 1,
               sky.mid, sky.low);

    // Stars, once the sky is dark enough to hold them. Fixed positions from a
    // hash so they do not crawl, and fading in rather than switching on.
    if (phase > 150) {
        int bright = (phase - 150) * 255 / 105;
        for (int i = 0; i < 70; ++i) {
            uint32_t h = (uint32_t)i * 2654435761u;
            int sx = (int)((h >> 9)  % (uint32_t)fb->w);
            int sy = (int)((h >> 19) % (uint32_t)(horizon - 20));
            if ((int)((h >> 5) & 255) > bright) continue;
            fill_rect(fb, sx, sy, 1, 1, rgb_lerp(sky.hi, 0xE8E8FF, bright));
        }
    }

    // ---- sun ---------------------------------------------------------
    // Sits low and drifts slightly east with progress, so late sectors feel
    // like a longer day.
    // The sun climbs and sinks with the phase, so late runs finish under a low
    // red one or a moon.
    int sx = fb->w / 2 + depth * 14 - 40;
    int arc = isin(256 + phase * 512 / 255);      // high at noon, low at either end
    int sy = horizon - 20 - arc / 4 + bleach / 2;
    int r  = (phase > 190) ? 14 : 22;             // a moon is smaller than a sun
    fill_glow(fb, sx, sy, 96, sky.glow, phase > 190 ? 7 : 13);
    fill_glow(fb, sx, sy, 52, sky.sun,  phase > 190 ? 8 : 14);
    fill_disc(fb, sx, sy, r + 8, sky.glow);
    fill_disc(fb, sx, sy, r, sky.sun);
    fill_disc(fb, sx, sy, r - 6, rgb_lerp(sky.sun, 0xFFFFFF, 120));

    // ---- haze band ---------------------------------------------------
    fill_vgrad(fb, 0, horizon - 16, fb->w, 16, sky.low,
               rgb_lerp(sky.low, PALETTE[C_HAZE], 160));

    // ---- dunes -------------------------------------------------------
    // Base ground first. Without it, any column where every dune silhouette
    // happens to dip below the horizon is never painted at all, and shows
    // whatever was left in the framebuffer.
    fill_rect(fb, 0, horizon - 1, fb->w, fb->h - horizon + 1, sky.land);

    for (int i = 0; i < 3; ++i) {
        const DuneLayer *l = &LAYERS[i];
        int scroll = (int)(tick / 8) * l->drift;
        // The land takes its cast from the sky, so dusk reddens the sand and
        // night drains it, rather than the ground staying noon-brown all run.
        uint32_t lit   = rgb_lerp(PALETTE[l->lit],   sky.land, 150);
        uint32_t shade = rgb_lerp(PALETTE[l->shade], sky.land, 120);
        uint32_t crest = rgb_lerp(PALETTE[C_HAZE],   sky.low,  120);
        for (int x = 0; x < fb->w; ++x) {
            int top = horizon + (i * 10) - dune_y(l, x + scroll, depth);
            if (top < 0) top = 0;
            fill_vgrad(fb, x, top, 1, fb->h - top, lit, shade);
            fill_rect(fb, x, top, 1, 1, crest);
        }
    }

    // ---- dust --------------------------------------------------------
    // Motes are a pure function of index and tick: no particle state anywhere.
    int motes = 40 + tension / 4;
    if (weather == WX_HAZE)  motes += 60;
    if (weather == WX_STORM) motes += 220;
    if (motes > 340) motes = 340;
    for (int i = 0; i < motes; ++i) {
        uint32_t h = (uint32_t)i * 2654435761u;
        int speed = 1 + (int)((h >> 3) & 3);
        int px = (int)(((h >> 8) % (uint32_t)fb->w
                        + tick * (uint32_t)speed / 2) % (uint32_t)fb->w);
        int base = horizon + 10 + (int)((h >> 16) % (uint32_t)(fb->h - horizon - 12));
        int py = base + isin((int32_t)(tick * 3 + i * 130)) / 40;
        int sz = (speed > 2) ? 2 : 1;
        if (weather == WX_STORM) sz += 1;
        fill_rect(fb, px, py, sz, sz, rgb_lerp(PALETTE[C_DUST], sky.low, 90));
    }

    // A storm also drags a moving veil across everything, which is what makes
    // it read as weather rather than as more dust.
    if (weather == WX_STORM) {
        for (int band = 0; band < 5; ++band) {
            int by = horizon - 40 + band * 26;
            int off = (int)((tick * (3 + band)) % (uint32_t)fb->w);
            fill_scrim(fb, -off, by, fb->w, 22, PALETTE[C_DUST], 3 + band % 2);
            fill_scrim(fb, fb->w - off, by, fb->w, 22, PALETTE[C_DUST], 3 + band % 2);
        }
    }
}
