#include "cutscene.h"
#include "render.h"
#include "scene.h"
#include "text.h"

// ---------------------------------------------------------------- scenes
static const Panel OPENING[] = {
    { ART_LOADING, 0, { T_OPEN_1A, T_OPEN_1B, 0, 0 } },
    { ART_SEEDS,   0, { T_OPEN_2A, T_OPEN_2B, 0, 0 } },
    { ART_ROAD,    1, { T_OPEN_3A, T_OPEN_3B, T_OPEN_3C, 0 } },
};
const Cutscene CS_OPENING = { OPENING, 3 };

static const Panel END_DEAD[]   = {
    { ART_WRECK, 7, { T_END_DEAD_A, T_END_DEAD_B, 0, 0 } } };
static const Panel END_EMPTY[]  = {
    { ART_EMPTY_HOLD, 13, { T_END_EMPTY_A, T_END_EMPTY_B, T_END_EMPTY_C, 0 } } };
static const Panel END_PART[]   = {
    { ART_GREEN, 13, { T_END_PART_A, T_END_PART_B, T_END_PART_C, 0 } } };
static const Panel END_INTACT[] = {
    { ART_GREEN, 13, { T_END_INTACT_A, T_END_INTACT_B, 0, 0 } } };
static const Panel END_EXEMP[]  = {
    { ART_GREEN, 13, { T_END_EXEMP_A, T_END_EXEMP_B, T_END_EXEMP_C, 0 } } };

const Cutscene CS_ENDING[OUT_COUNT] = {
    { END_DEAD,   1 },
    { END_EMPTY,  1 },
    { END_PART,   1 },
    { END_INTACT, 1 },
    { END_EXEMP,  1 },
};

// ---------------------------------------------------------------- vignettes
static const Panel VIG_FIRST[] = {
    { ART_ROAD, 1, { T_VIG_FIRST_A, T_VIG_FIRST_B, 0, 0 } } };
static const Panel VIG_HALF[]  = {
    { ART_ROAD, 6, { T_VIG_HALF_A, T_VIG_HALF_B, 0, 0 } } };
static const Panel VIG_STORM[] = {
    { ART_STORM, 4, { T_VIG_STORM_A, 0, 0, 0 } } };
static const Panel VIG_LAST[]  = {
    { ART_GREEN, 12, { T_VIG_LAST_A, 0, 0, 0 } } };
static const Panel VIG_LOSS[]  = {
    { ART_EMPTY_HOLD, 6, { T_VIG_LOSS_A, T_VIG_LOSS_B, 0, 0 } } };

static const Cutscene CS_FIRST = { VIG_FIRST, 1 };
static const Cutscene CS_HALF  = { VIG_HALF,  1 };
static const Cutscene CS_STORM = { VIG_STORM, 1 };
static const Cutscene CS_LAST  = { VIG_LAST,  1 };
static const Cutscene CS_LOSS  = { VIG_LOSS,  1 };

// One beat per milestone, each fired once. Interrupting a run repeatedly is
// how a cut scene stops being a reward and starts being a toll booth.
//
// "Once" is per *kind*, not per sector, which is what the caller used to track.
// Two of these are conditions rather than places: the seed being gone stays
// true for the rest of the run, and storms recur. Keyed by sector, the loss
// beat replayed at every remaining settlement -- and since it is tested first,
// it also suppressed the halfway and last-hop beats entirely, so a convoy that
// lost its cargo saw the same three lines four times and nothing else again.
//
// The 0xFE guard that used to sit on the loss test is gone with it: that value
// was never assigned anywhere in the program, so the condition was always true
// and the sentinel was decoration.
const Cutscene *cutscene_vignette(const World *w, int *kind) {
    int k; const Cutscene *cs;
    if      (w->payload == 0)                 { k = VIG_LOSS_K;  cs = &CS_LOSS;  }
    else if (w->sector == 1)                  { k = VIG_FIRST_K; cs = &CS_FIRST; }
    else if (w->sector == (SECTORS - 1) / 2)  { k = VIG_HALF_K;  cs = &CS_HALF;  }
    else if (w->sector == SECTORS - 2)        { k = VIG_LAST_K;  cs = &CS_LAST;  }
    else if (w->node[w->sector][w->index].type == NODE_HAZARD)
                                              { k = VIG_STORM_K; cs = &CS_STORM; }
    else return 0;
    if (kind) *kind = k;
    return cs;
}

// ---------------------------------------------------------------- art
#define REVEAL_TICKS 2      // ticks per character

static void art_crates(Framebuffer *fb, int x, int y, int n, int s) {
    for (int i = 0; i < n; ++i) {
        int cx = x + (i % 3) * 13 * s, cy = y - (i / 3) * 13 * s;
        fill_rect(fb, cx, cy, 11 * s, 11 * s, PALETTE[C_DUNE_NEAR]);
        fill_rect(fb, cx + s, cy + s, 9 * s, 9 * s, PALETTE[C_RUST]);
        fill_rect(fb, cx + s, cy + 4 * s, 9 * s, 2 * s, PALETTE[C_DUNE_MID]);
        draw_bevel(fb, cx, cy, 11 * s, 11 * s, 0);
    }
}

static void draw_art(Framebuffer *fb, int art, uint32_t tick) {
    int cx = fb->w / 2, mid = fb->h / 2 - 20;

    switch (art) {
    case ART_LOADING:
        draw_convoy(fb, cx - 150, mid - 30, 3, tick, 0);
        art_crates(fb, cx + 40, mid + 10, 6, 2);
        break;

    case ART_SEEDS:
        art_crates(fb, cx - 60, mid + 20, 6, 3);
        break;

    case ART_ROAD:
        draw_convoy(fb, cx - 60 + (int)((tick / 3) % 40), mid - 20, 4, tick, 0);
        break;

    case ART_GREEN: {
        int gx = cx - 30, gy = mid - 40;
        draw_drop(fb, gx, gy, 60, 60);
        fill_rect(fb, gx, gy, 60, 60, PALETTE[C_GREEN]);
        fill_rect(fb, gx, gy + 50, 60, 10, PALETTE[C_ROAD]);
        draw_bevel(fb, gx, gy, 60, 60, 0);
        fill_rect(fb, gx + 28, gy + 12, 5, 38, PALETTE[C_BONE]);
        fill_rect(fb, gx + 16, gy + 20, 29, 5, PALETTE[C_BONE]);
        fill_rect(fb, gx + 20, gy + 32, 21, 5, PALETTE[C_BONE]);
        break;
    }

    case ART_WRECK:
        draw_convoy(fb, cx - 60, mid - 20, 4, tick, 1);
        break;

    case ART_EMPTY_HOLD: {
        // An open, empty hold: the same grid the market screen draws, with
        // nothing in it.
        int gx = cx - 110, gy = mid - 30;
        for (int i = 0; i < 12; ++i)
            draw_rect(fb, gx + (i % 6) * 38, gy + (i / 6) * 38, 34, 34,
                      PALETTE[C_BORDER]);
        break;
    }

    default: // ART_STORM
        for (int i = 0; i < 90; ++i) {
            uint32_t h = (uint32_t)i * 2654435761u;
            int px = (int)(((h >> 8) + tick * 4) % (uint32_t)fb->w);
            int py = mid - 60 + (int)((h >> 16) % 120u);
            fill_rect(fb, px, py, 3 + (int)(h & 3u), 2, PALETTE[C_DUST]);
        }
        break;
    }
}

// ---------------------------------------------------------------- driver
static int panel_chars(const Panel *p) {
    int n = 0;
    for (int i = 0; i < PANEL_LINES && p->line[i]; ++i) {
        const char *s = p->line[i];
        while (*s++) ++n;
    }
    return n;
}

void cutscene_begin(CutsceneState *s, const Cutscene *cs, uint32_t tick) {
    s->cs = cs;
    s->index = 0;
    s->started = tick;
    s->running = (cs && cs->count) ? 1 : 0;
}

int cutscene_update(CutsceneState *s, int advance, uint32_t tick) {
    if (!s->running) return 0;

    const Panel *p = &s->cs->panel[s->index];
    uint32_t shown = (tick - s->started) / REVEAL_TICKS;
    int total = panel_chars(p);

    if (advance) {
        // First press finishes the text, second moves on. Nobody should have
        // to wait for a typewriter they have already read.
        if ((int)shown < total) {
            s->started = tick - (uint32_t)total * REVEAL_TICKS;
        } else if (++s->index >= s->cs->count) {
            s->running = 0;
        } else {
            s->started = tick;
        }
    }
    return s->running;
}

void cutscene_draw(Framebuffer *fb, const CutsceneState *s, uint32_t tick) {
    if (!s->running) return;
    const Panel *p = &s->cs->panel[s->index];

    scene_draw(fb, tick, p->depth, 30,
               p->depth * 255 / (SECTORS - 1), WX_CLEAR);
    draw_art(fb, p->art, tick);

    // Letterbox. The bars are what make it read as a cut scene rather than a
    // popup, and they give the text a ground to sit on.
    const int bar = 72;
    fill_rect(fb, 0, 0, fb->w, bar, PALETTE[C_INK]);
    fill_rect(fb, 0, fb->h - bar, fb->w, bar, PALETTE[C_INK]);
    fill_rect(fb, 0, bar, fb->w, 1, PALETTE[C_PANEL]);
    fill_rect(fb, 0, fb->h - bar - 1, fb->w, 1, PALETTE[C_PANEL]);

    // Typewriter: a running budget of characters spent line by line.
    int budget = (int)((tick - s->started) / REVEAL_TICKS);
    int y = fb->h - bar + 14;
    for (int i = 0; i < PANEL_LINES && p->line[i]; ++i) {
        const char *line = p->line[i];
        int len = 0; while (line[len]) ++len;
        int show = budget < 0 ? 0 : (budget > len ? len : budget);
        budget -= len;

        // Draw the revealed prefix one glyph at a time.
        int x = 24;
        for (int c = 0; c < show; ++c) {
            char buf[2] = { line[c], 0 };
            draw_text(fb, x, y, buf, 1, PALETTE[C_BONE]);
            x += glyph_w(1);
        }
        y += 15;
    }

    // After the loop, budget is (characters revealed - characters wanted), so
    // it is non-negative exactly when the panel has finished typing. The
    // prompt must not appear before then or it reads as "nothing more coming".
    if (budget >= 0 && ((tick / 24) & 1))
        draw_text_c(fb, fb->w / 2, fb->h - 22, T_OPEN_SKIP, 1, PALETTE[C_DIM]);
}
