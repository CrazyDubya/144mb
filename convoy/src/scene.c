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

void scene_draw(Framebuffer *fb, uint32_t tick, int depth, int tension) {
    int horizon = HORIZON;

    // ---- sky ---------------------------------------------------------
    // Two dithered bands: a dark dusty zenith falling to a bright, hazy
    // horizon. Deeper sectors bleach it further.
    int bleach = depth * 3;
    fill_vgrad(fb, 0, 0, fb->w, horizon * 2 / 3,
               PALETTE[C_SKY_HI], PALETTE[C_SKY_MID]);
    fill_vgrad(fb, 0, horizon * 2 / 3, fb->w, horizon - horizon * 2 / 3 + 1,
               PALETTE[C_SKY_MID], PALETTE[C_SKY]);

    // ---- sun ---------------------------------------------------------
    // Sits low and drifts slightly east with progress, so late sectors feel
    // like a longer day.
    int sx = fb->w / 2 + depth * 14 - 40;
    int sy = horizon - 44 + bleach / 2;
    // Dithered corona, then hard discs. The scatter sells "sun through dust"
    // far better than another solid ring would.
    fill_glow(fb, sx, sy, 96, PALETTE[C_SUN_GLOW], 13);
    fill_glow(fb, sx, sy, 52, PALETTE[C_SUN], 14);
    fill_disc(fb, sx, sy, 30, PALETTE[C_SUN_GLOW]);
    fill_disc(fb, sx, sy, 22, PALETTE[C_SUN]);
    fill_disc(fb, sx, sy, 16, 0xFFF4D0);

    // ---- haze band ---------------------------------------------------
    fill_vgrad(fb, 0, horizon - 16, fb->w, 16,
               PALETTE[C_SKY], PALETTE[C_HAZE]);

    // ---- dunes -------------------------------------------------------
    // Base ground first. Without it, any column where every dune silhouette
    // happens to dip below the horizon is never painted at all, and shows
    // whatever was left in the framebuffer.
    fill_rect(fb, 0, horizon - 1, fb->w, fb->h - horizon + 1, PALETTE[C_DUNE_FAR]);

    for (int i = 0; i < 3; ++i) {
        const DuneLayer *l = &LAYERS[i];
        int scroll = (int)(tick / 8) * l->drift;
        for (int x = 0; x < fb->w; ++x) {
            int top = horizon + (i * 10) - dune_y(l, x + scroll, depth);
            if (top < 0) top = 0;
            // Each layer ramps from lit crest to shaded base, so the sand has
            // grain and the layers separate without outlines.
            fill_vgrad(fb, x, top, 1, fb->h - top,
                       PALETTE[l->lit], PALETTE[l->shade]);
            // A brighter lip along the crest catches the low sun.
            fill_rect(fb, x, top, 1, 1, PALETTE[C_HAZE]);
        }
    }

    // ---- dust --------------------------------------------------------
    // Motes are a pure function of index and tick: no particle state anywhere.
    int motes = 40 + tension / 4;
    if (motes > 110) motes = 110;
    for (int i = 0; i < motes; ++i) {
        uint32_t h = (uint32_t)i * 2654435761u;
        int speed = 1 + (int)((h >> 3) & 3);
        int px = (int)(((h >> 8) % (uint32_t)fb->w
                        + tick * (uint32_t)speed / 2) % (uint32_t)fb->w);
        int base = horizon + 10 + (int)((h >> 16) % (uint32_t)(fb->h - horizon - 12));
        int py = base + isin((int32_t)(tick * 3 + i * 130)) / 40;
        int sz = (speed > 2) ? 2 : 1;
        fill_rect(fb, px, py, sz, sz, PALETTE[C_DUST]);
    }
}
