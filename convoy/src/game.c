// convoy -- game core: state machine and presentation over the world sim.
//
// Every screen is language-free. Meaning is carried by icon, colour, shape and
// Arabic numerals only; there is no alphabetic font in the build.
#include "game.h"
#include "render.h"
#include "world.h"
#include "audio.h"

typedef struct {
    World      w;
    uint32_t   tick;
    uint32_t   seed;
    int        sel;        // selected good on the trade screen
    int        map_sel;    // index into the reachable-node list on the map
    AudioState audio;
} GameState;

// Exposed so the headless harness can trace the simulation; not used by the
// Windows build, and costs nothing there since it is never referenced.
const World *game_world(GameMemory *mem) {
    return &((GameState *)mem->permanent)->w;
}

// ---------------------------------------------------------------- helpers
// Collects the nodes reachable from the current position, in index order.
static int reachable(const World *w, int *out) {
    int n = 0;
    if (w->sector >= SECTORS - 1) return 0;
    uint8_t links = w->node[w->sector][w->index].links;
    for (int m = 0; m < NODES_PER; ++m)
        if ((links & (1u << m)) && w->node[w->sector + 1][m].active) out[n++] = m;
    return n;
}

static void restart(GameState *gs, uint32_t seed) {
    world_init(&gs->w, seed);
    gs->sel = 0;
    gs->map_sel = 0;
}

void game_init(GameMemory *mem, uint32_t seed) {
    GameState *gs = (GameState *)mem->permanent;
    gs->tick = 0;
    gs->seed = seed ? seed : 1u;
    restart(gs, gs->seed);
    audio_init(&gs->audio, gs->seed);
    mem->initialized = 1;
}

// ---------------------------------------------------------------- hud
static void draw_hud(Framebuffer *fb, const World *w) {
    draw_panel(fb, 0, 0, fb->w, 30);

    int x = 8;
    // Water and fuel are the two that kill you; they lead.
    draw_icon(fb, x, 7, ICON_WATER, 1); x += 20;
    x += draw_number(fb, x, 11, w->held[G_WATER], 2,
                     w->held[G_WATER] <= 2 ? PALETTE[C_BAD] : PALETTE[C_BONE]) + 12;
    draw_icon(fb, x, 7, ICON_FUEL, 1); x += 20;
    x += draw_number(fb, x, 11, w->held[G_FUEL], 2,
                     w->held[G_FUEL] <= 2 ? PALETTE[C_BAD] : PALETTE[C_BONE]) + 20;

    // Cargo occupancy: held / capacity.
    x += draw_number(fb, x, 11, world_cargo(w), 2, PALETTE[C_DIM]);
    draw_glyph(fb, x, 11, G_SLASH, 2, PALETTE[C_DIM]); x += glyph_w(2);
    x += draw_number(fb, x, 11, CARGO_CAP, 2, PALETTE[C_DIM]) + 20;

    // Credits, right-aligned.
    int cw = number_w(w->credits, 2);
    draw_number(fb, fb->w - 12 - cw, 11, w->credits, 2, PALETTE[C_WARN]);

    // Day counter, just left of credits.
    int dw = number_w(w->day, 2);
    draw_number(fb, fb->w - 40 - cw - dw, 11, w->day, 2, PALETTE[C_DIM]);
}

// ---------------------------------------------------------------- map
static int sector_count(const World *w, int s) {
    int c = 0;
    for (int n = 0; n < NODES_PER; ++n) if (w->node[s][n].active) ++c;
    return c;
}

// Each sector's nodes are centred vertically, so the route reads as a spine
// down the middle of the screen rather than a band stuck under the HUD.
static void node_pos(const World *w, int s, int n, int *x, int *y) {
    int count = sector_count(w, s);
    *x = 32 + s * 78;
    *y = 258 - (count - 1) * 72 / 2 + n * 72;
}

static uint32_t node_colour(int type) {
    switch (type) {
    case NODE_SETTLE: return PALETTE[C_BONE];
    case NODE_EVENT:  return PALETTE[C_RUST];
    case NODE_HAZARD: return PALETTE[C_WARN];
    case NODE_GREEN:  return PALETTE[C_GREEN];
    default:          return PALETTE[C_DIM];
    }
}

static void draw_map(Framebuffer *fb, GameState *gs) {
    World *w = &gs->w;

    int cand[NODES_PER], ncand = reachable(w, cand);

    // Links first, so nodes sit on top of them.
    for (int s = 0; s < SECTORS - 1; ++s) {
        for (int n = 0; n < NODES_PER; ++n) {
            if (!w->node[s][n].active) continue;
            int x0, y0; node_pos(w, s, n, &x0, &y0);
            for (int m = 0; m < NODES_PER; ++m) {
                if (!(w->node[s][n].links & (1u << m))) continue;
                if (!w->node[s + 1][m].active) continue;
                int x1, y1; node_pos(w, s + 1, m, &x1, &y1);
                draw_line(fb, x0 + 10, y0 + 10, x1 + 10, y1 + 10, PALETTE[C_BORDER]);
            }
        }
    }

    for (int s = 0; s < SECTORS; ++s) {
        for (int n = 0; n < NODES_PER; ++n) {
            Node *nd = &w->node[s][n];
            if (!nd->active) continue;
            int x, y; node_pos(w, s, n, &x, &y);
            uint32_t c = node_colour(nd->type);

            if (nd->type == NODE_GREEN) {
                fill_rect(fb, x - 2, y - 2, 24, 24, c);
                draw_rect(fb, x - 2, y - 2, 24, 24, PALETTE[C_BONE]);
            } else if (nd->type == NODE_EMPTY) {
                fill_rect(fb, x + 6, y + 6, 8, 8, c);
            } else {
                fill_rect(fb, x, y, 20, 20, PALETTE[C_INK]);
                fill_rect(fb, x + 3, y + 3, 14, 14, c);
                if (nd->type == NODE_EVENT)
                    draw_glyph(fb, x + 6, y + 6, G_X, 1, PALETTE[C_INK]);
            }

            if (nd->visited) draw_rect(fb, x - 1, y - 1, 22, 22, PALETTE[C_DIM]);
        }
    }

    // Current position marker.
    int cx, cy; node_pos(w, w->sector, w->index, &cx, &cy);
    draw_rect(fb, cx - 5, cy - 5, 30, 30, PALETTE[C_BONE]);
    draw_rect(fb, cx - 4, cy - 4, 28, 28, PALETTE[C_BONE]);

    // Highlight the selected destination.
    if (ncand > 0) {
        int m = cand[gs->map_sel % ncand];
        int nx, ny; node_pos(w, w->sector + 1, m, &nx, &ny);
        draw_rect(fb, nx - 6, ny - 6, 32, 32, PALETTE[C_WARN]);
        draw_line(fb, cx + 10, cy + 10, nx + 10, ny + 10, PALETTE[C_WARN]);
        // Fuel cost of the hop, and the key that commits to it.
        draw_glyph(fb, (cx + nx) / 2 - 6, (cy + ny) / 2 - 20, G_MINUS, 1, PALETTE[C_WARN]);
        draw_icon(fb, (cx + nx) / 2 + 2, (cy + ny) / 2 - 24, ICON_FUEL, 1);
        draw_number(fb, (cx + nx) / 2 + 20, (cy + ny) / 2 - 20, 1, 1, PALETTE[C_WARN]);
        draw_key(fb, nx + 2, ny + 34, G_KEY_Z, 2);

        // Up/down move the selection when there is a choice to make.
        if (ncand > 1) {
            draw_glyph(fb, cx + 44, cy - 4,  G_UP,   2, PALETTE[C_DIM]);
            draw_glyph(fb, cx + 44, cy + 18, G_DOWN, 2, PALETTE[C_DIM]);
        }
    }
}

// ---------------------------------------------------------------- trade
static void draw_trade(Framebuffer *fb, GameState *gs) {
    World *w = &gs->w;
    Node *nd = &w->node[w->sector][w->index];

    const int x = 40, y = 70, pw = 300, rowh = 34;
    // One extra row below the goods for the depart action, so leaving the
    // market looks like another menu choice rather than a hidden key.
    draw_panel(fb, x, y, pw, 14 + (GOODS_COUNT + 1) * rowh);

    for (int g = 0; g < GOODS_COUNT; ++g) {
        int ry = y + 10 + g * rowh;
        if (g == gs->sel) {
            fill_rect(fb, x + 4, ry - 4, pw - 8, rowh - 2, PALETTE[C_BORDER]);
            draw_rect(fb, x + 4, ry - 4, pw - 8, rowh - 2, PALETTE[C_BONE]);
        }
        draw_icon(fb, x + 10, ry + 2, g, 1);
        draw_number(fb, x + 40, ry + 6, nd->price[g], 2, PALETTE[C_BONE]);

        // On the selected row, the two trade actions appear inline.
        if (g == gs->sel) {
            int kx = x + 118;
            kx += draw_key(fb, kx, ry + 2, G_KEY_Z, 2) + 2;
            draw_glyph(fb, kx, ry + 8, G_PLUS, 2, PALETTE[C_GOOD]);
            kx += glyph_w(2) + 14;
            kx += draw_key(fb, kx, ry + 2, G_X, 2) + 2;
            draw_glyph(fb, kx, ry + 8, G_MINUS, 2, PALETTE[C_BAD]);
        }

        // Held quantity, right side.
        int qw = number_w(w->held[g], 2);
        draw_number(fb, x + pw - 20 - qw, ry + 6, w->held[g], 2,
                    w->held[g] ? PALETTE[C_DIM] : PALETTE[C_PANEL]);
    }

    // Departing the market: its own row, reading as "leave, onward".
    {
        int dy = y + 10 + GOODS_COUNT * rowh;
        fill_rect(fb, x + 4, dy - 4, pw - 8, rowh - 2, PALETTE[C_INK]);
        int dx = x + 10;
        dx += draw_key(fb, dx, dy + 2, G_ENTER, 2) + 10;
        draw_glyph(fb, dx, dy + 8, G_RIGHT, 2, PALETTE[C_BONE]);
        draw_icon(fb, dx + 26, dy + 2, ICON_FUEL, 1);
    }

    // Cargo hold below, the run's health bar.
    const int cell = 20, cols = 15;
    int cy = y + 20 + (GOODS_COUNT + 1) * rowh;
    draw_panel(fb, x, cy, cols * cell + 12, 2 * cell + 12);
    int slot = 0;
    for (int g = 0; g < GOODS_COUNT; ++g) {
        for (int n = 0; n < w->held[g] && slot < CARGO_CAP; ++n, ++slot) {
            int sx = x + 6 + (slot % cols) * cell;
            int sy = cy + 6 + (slot / cols) * cell;
            fill_rect(fb, sx + 1, sy + 1, cell - 2, cell - 2, PALETTE[C_INK]);
            draw_icon(fb, sx + 2, sy + 2, g, 1);
        }
    }
    for (; slot < CARGO_CAP; ++slot) {
        int sx = x + 6 + (slot % cols) * cell;
        int sy = cy + 6 + (slot / cols) * cell;
        draw_rect(fb, sx + 1, sy + 1, cell - 2, cell - 2, PALETTE[C_BORDER]);
    }
}

// ---------------------------------------------------------------- event
// Draws a signed quantity of a good: "-<icon>x<n>" or "+<icon>x<n>".
// The sign is not decoration -- without it a cost and a reward look identical,
// which is fatal in a game with no words to disambiguate them.
static int draw_stack(Framebuffer *fb, int x, int y, int sign, int icon, int qty,
                      uint32_t tint) {
    int x0 = x;
    if (sign) {
        draw_glyph(fb, x, y + 10, sign < 0 ? G_MINUS : G_PLUS, 2, tint);
        x += glyph_w(2) + 6;
    }
    draw_icon(fb, x, y, icon, 2);
    x += 40;
    draw_glyph(fb, x, y + 10, G_X, 2, tint); x += glyph_w(2) + 2;
    x += draw_number(fb, x, y + 10, qty, 2, tint);
    return x - x0;
}

// One layout for every encounter. Top row is what accepting costs and yields,
// bottom row is what refusing costs. Threat encounters are framed red,
// opportunities green -- readable before any of the numbers are.
static void draw_event(Framebuffer *fb, GameState *gs) {
    World *w = &gs->w;
    const Event *e = &w->event;
    const int x = 140, y = 96, pw = 360, ph = 250;

    int threat = (e->kind == EV_RAID || e->kind == EV_SICK || e->kind == EV_BREAK);
    uint32_t frame = threat ? PALETTE[C_BAD] : PALETTE[C_GOOD];

    draw_panel(fb, x, y, pw, ph);
    fill_rect(fb, x + 3, y + 3, pw - 6, 10, frame);

    int affordable = world_can_accept(w);

    // --- accept -------------------------------------------------------
    int ay = y + 34;
    fill_rect(fb, x + 10, ay - 6, pw - 20, 74, PALETTE[C_INK]);
    draw_rect(fb, x + 10, ay - 6, pw - 20, 74, affordable ? PALETTE[C_BONE] : PALETTE[C_DIM]);

    draw_key(fb, x + 20, ay + 6, G_KEY_Z, 2);
    int ax = x + 20 + key_w(2) + 18;
    if (e->pay_good >= 0)
        ax += draw_stack(fb, ax, ay, -1, e->pay_good, e->pay_qty,
                         affordable ? PALETTE[C_BONE] : PALETTE[C_BAD]) + 24;

    if (e->gain_good >= 0) {
        draw_stack(fb, ax, ay, +1, e->gain_good, e->gain_qty, PALETTE[C_GOOD]);
    } else if (e->gain_credits > 0) {
        draw_glyph(fb, ax, ay + 10, G_PLUS, 2, PALETTE[C_WARN]);
        draw_number(fb, ax + glyph_w(2) + 4, ay + 10, e->gain_credits, 2, PALETTE[C_WARN]);
    } else {
        // Nothing gained -- you simply survive it.
        draw_glyph(fb, ax, ay + 10, G_MINUS, 2, PALETTE[C_DIM]);
    }

    // --- decline ------------------------------------------------------
    int by = y + 138;
    fill_rect(fb, x + 10, by - 6, pw - 20, 74, PALETTE[C_INK]);
    draw_rect(fb, x + 10, by - 6, pw - 20, 74, PALETTE[C_DIM]);

    draw_key(fb, x + 20, by + 6, G_X, 2);
    int bx = x + 20 + key_w(2) + 18;
    if (e->lose_qty > 0) {
        if (e->lose_good >= 0) {
            draw_stack(fb, bx, by, -1, e->lose_good, e->lose_qty, PALETTE[C_BAD]);
        } else {
            // Random cargo: show the hold itself being taken.
            draw_glyph(fb, bx, by + 10, G_MINUS, 2, PALETTE[C_BAD]);
            draw_number(fb, bx + glyph_w(2) + 6, by + 10, e->lose_qty, 2, PALETTE[C_BAD]);
            for (int i = 0; i < 3; ++i)
                draw_rect(fb, bx + 70 + i * 20, by + 6, 16, 16, PALETTE[C_BAD]);
        }
    } else {
        draw_glyph(fb, bx, by + 10, G_MINUS, 2, PALETTE[C_DIM]);
    }
}

// ---------------------------------------------------------------- end
static void draw_end(Framebuffer *fb, GameState *gs, int won) {
    World *w = &gs->w;
    fill_rect(fb, 0, 0, fb->w, fb->h,
              won ? PALETTE[C_GREEN] : PALETTE[C_INK]);
    const int x = fb->w / 2 - 120, y = fb->h / 2 - 70;
    draw_panel(fb, x, y, 240, 140);

    if (won) {
        fill_rect(fb, x + 100, y + 24, 40, 40, PALETTE[C_GREEN]);
    } else {
        // Cause of death as an icon, not a sentence.
        int icon = w->death == DEATH_THIRST    ? ICON_WATER :
                   w->death == DEATH_STRANDED  ? ICON_FUEL  : ICON_SCRAP;
        draw_icon(fb, x + 104, y + 24, icon, 2);
        draw_glyph(fb, x + 150, y + 34, G_X, 3, PALETTE[C_BAD]);
    }

    // Final score: day reached and credits.
    draw_number(fb, x + 40, y + 84, w->day, 2, PALETTE[C_BONE]);
    draw_number(fb, x + 140, y + 84, w->credits, 2, PALETTE[C_WARN]);

    // Run it again.
    draw_key(fb, x + 108, y + 108, G_ENTER, 2);
}

// ---------------------------------------------------------------- update
void game_update(GameMemory *mem, const Input *in, Framebuffer *fb) {
    GameState *gs = (GameState *)mem->permanent;
    World *w = &gs->w;
    gs->tick++;

    // Snapshot enough to tell whether an action actually did anything, so
    // sounds only fire on real state changes rather than on every keypress.
    int prev_state  = w->state;
    int prev_cargo  = world_cargo(w);
    int prev_credit = w->credits;
    int prev_sector = w->sector;

    switch (w->state) {
    case ST_TRADE:
        if (in->pressed[BTN_UP]   && gs->sel > 0)               gs->sel--;
        if (in->pressed[BTN_DOWN] && gs->sel < GOODS_COUNT - 1) gs->sel++;
        if (in->pressed[BTN_A]) world_buy(w, gs->sel);
        if (in->pressed[BTN_B]) world_sell(w, gs->sel);
        if (in->pressed[BTN_START]) { w->state = ST_MAP; gs->map_sel = 0; }
        break;

    case ST_MAP: {
        int cand[NODES_PER], n = reachable(w, cand);
        if (n > 0) {
            if (in->pressed[BTN_UP]   && gs->map_sel > 0)     gs->map_sel--;
            if (in->pressed[BTN_DOWN] && gs->map_sel < n - 1) gs->map_sel++;
            if (in->pressed[BTN_A])   world_travel(w, cand[gs->map_sel]);
        }
        break;
    }

    case ST_EVENT:
        if (in->pressed[BTN_A]) world_accept(w);
        if (in->pressed[BTN_B]) world_decline(w);
        break;

    case ST_DEAD:
    case ST_WON:
        if (in->pressed[BTN_START]) restart(gs, gs->seed + gs->tick);
        break;
    }

    // ------------------------------------------------------------ audio
    if (w->state != prev_state) {
        if (w->state == ST_DEAD)      audio_trigger(&gs->audio, SFX_DEATH);
        else if (w->state == ST_WON)  audio_trigger(&gs->audio, SFX_WIN);
    }
    if (w->sector != prev_sector)                  audio_trigger(&gs->audio, SFX_TRAVEL);
    else if (world_cargo(w) > prev_cargo)          audio_trigger(&gs->audio, SFX_BUY);
    else if (w->credits > prev_credit)             audio_trigger(&gs->audio, SFX_SELL);
    else if (world_cargo(w) < prev_cargo && prev_state == ST_EVENT)
                                                   audio_trigger(&gs->audio, SFX_HIT);

    // Tension climbs with depth and with how close the convoy is to running dry.
    {
        int depth  = w->sector * 20;
        int thirst = w->held[G_WATER] < 4 ? (4 - w->held[G_WATER]) * 30 : 0;
        int dry    = w->held[G_FUEL]  < 3 ? (3 - w->held[G_FUEL])  * 30 : 0;
        audio_tension(&gs->audio, depth + thirst + dry);
    }

    // ------------------------------------------------------------ render
    if (w->state == ST_DEAD || w->state == ST_WON) {
        draw_end(fb, gs, w->state == ST_WON);
        return;
    }

    // Shared backdrop.
    const int horizon = fb->h * 34 / 100;
    fill_rect(fb, 0, 0, fb->w, horizon, PALETTE[C_SKY]);
    fill_rect(fb, 0, horizon - 12, fb->w, 12, PALETTE[C_HAZE]);
    fill_rect(fb, 0, horizon, fb->w, fb->h - horizon, PALETTE[C_DUNE_NEAR]);

    switch (w->state) {
    case ST_MAP:   draw_map(fb, gs);   break;
    case ST_TRADE: draw_trade(fb, gs); break;
    case ST_EVENT: draw_event(fb, gs); break;
    default: break;
    }
    draw_hud(fb, w);
}

void game_audio(GameMemory *mem, AudioBuffer *ab) {
    GameState *gs = (GameState *)mem->permanent;
    audio_render(&gs->audio, ab->samples, ab->frames);
}
