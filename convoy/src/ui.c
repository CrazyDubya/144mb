// convoy -- screen drawing. Reads state, writes pixels, decides nothing.
//
// Split out of game.c once that file was doing both the state machine and
// every screen at 722 lines. render.c owns primitives; this owns layout.
#include "ui.h"
#include "render.h"
#include "scene.h"
#include "text.h"

const char *const GOOD_NAME[5] = { T_WATER, T_FUEL, T_AMMO, T_MEDS, T_SCRAP };
const char *const GOOD_USE[5]  = {
    T_USE_WATER, T_USE_FUEL, T_USE_AMMO, T_USE_MEDS, T_USE_SCRAP
};

// Only the market tab has rows today; the others land in phases 1 and 2.
int ui_tab_rows(const GameState *gs, int tab) {
    (void)gs;
    return tab == TAB_MARKET ? GOODS_COUNT : 0;
}

// ---------------------------------------------------------------- backdrop
void ui_backdrop(Framebuffer *fb, GameState *gs) {
    const World *w = &gs->w;
    int thirst = w->held[G_WATER] < 4 ? (4 - w->held[G_WATER]) * 30 : 0;
    scene_draw(fb, gs->tick, w->sector, w->sector * 20 + thirst);
}

// ---------------------------------------------------------------- hud
void ui_hud(Framebuffer *fb, const World *w) {
    draw_panel(fb, 0, 0, fb->w, 34);

    int x = 8;
    // Water and fuel are the two that kill you; they lead, and both are named
    // so the icon never has to carry the meaning alone.
    draw_icon(fb, x, 9, ICON_WATER, 1); x += 20;
    x += draw_number(fb, x, 13, w->held[G_WATER], 2,
                     w->held[G_WATER] <= 2 ? PALETTE[C_BAD] : PALETTE[C_BONE]) + 4;
    x += draw_text(fb, x, 15, T_WATER, 1, PALETTE[C_DIM]) + 14;

    draw_icon(fb, x, 9, ICON_FUEL, 1); x += 20;
    x += draw_number(fb, x, 13, w->held[G_FUEL], 2,
                     w->held[G_FUEL] <= 2 ? PALETTE[C_BAD] : PALETTE[C_BONE]) + 4;
    x += draw_text(fb, x, 15, T_FUEL, 1, PALETTE[C_DIM]) + 18;

    // Cargo occupancy: held / capacity.
    x += draw_text(fb, x, 15, T_HOLD, 1, PALETTE[C_DIM]) + 6;
    x += draw_number(fb, x, 13, world_cargo(w), 2, PALETTE[C_BONE]);
    draw_glyph(fb, x, 13, G_SLASH, 2, PALETTE[C_DIM]); x += glyph_w(2);
    x += draw_number(fb, x, 13, CARGO_CAP, 2, PALETTE[C_DIM]);

    // Credits, right-aligned, then the day just left of it.
    int cw = number_w(w->credits, 2);
    draw_number(fb, fb->w - 10 - cw, 13, w->credits, 2, PALETTE[C_WARN]);
    int lw = text_w(T_CREDITS, 1);
    draw_text(fb, fb->w - 16 - cw - lw, 15, T_CREDITS, 1, PALETTE[C_DIM]);

    int dw = number_w(w->day, 2);
    int right = fb->w - 26 - cw - lw;
    draw_number(fb, right - dw, 13, w->day, 2, PALETTE[C_BONE]);
    draw_text(fb, right - dw - text_w(T_DAY, 1) - 6, 15, T_DAY, 1, PALETTE[C_DIM]);
}

// ---------------------------------------------------------------- map
static int sector_count(const World *w, int s) {
    int c = 0;
    for (int n = 0; n < NODES_PER; ++n) if (w->node[s][n].active) ++c;
    return c;
}

#define SECTOR_PITCH 63

// The full route is wider than the screen, so the map scrolls. The convoy is
// held about a third from the left: enough road behind to see where you came
// from, most of the view given to what is still ahead.
static int map_cam(const World *w, int fbw) {
    int cam = w->sector * SECTOR_PITCH - fbw / 3;
    int max = (SECTORS - 1) * SECTOR_PITCH + 60 - fbw;
    if (max < 0)   max = 0;
    if (cam > max) cam = max;
    if (cam < 0)   cam = 0;
    return cam;
}

// Each sector's nodes are centred vertically, so the route reads as a spine
// down the middle of the screen rather than a band stuck under the HUD.
static void node_pos(const World *w, int s, int n, int *x, int *y, int cam) {
    int count = sector_count(w, s);
    *x = 26 + s * SECTOR_PITCH - cam;
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

static const char *node_name(int type) {
    switch (type) {
    case NODE_SETTLE: return T_SETTLEMENT;
    case NODE_EVENT:  return T_ENCOUNTER;
    case NODE_HAZARD: return T_STORM;
    case NODE_GREEN:  return T_GREEN_ZONE;
    default:          return T_EMPTY_ROAD;
    }
}

void ui_map(Framebuffer *fb, GameState *gs) {
    World *w = &gs->w;

    int cand[NODES_PER], ncand = world_reachable(w, cand);
    int cam = map_cam(w, fb->w);

    // No scrim behind the route: any band wide enough to help leaves a visible
    // hard edge across the screen, and the nodes carry enough contrast alone
    // now that they have ink backing and drop shadows.

    // Links first, so nodes sit on top of them.
    for (int s = 0; s < SECTORS - 1; ++s) {
        for (int n = 0; n < NODES_PER; ++n) {
            if (!w->node[s][n].active) continue;
            int x0, y0; node_pos(w, s, n, &x0, &y0, cam);
            for (int m = 0; m < NODES_PER; ++m) {
                if (!(w->node[s][n].links & (1u << m))) continue;
                if (!w->node[s + 1][m].active) continue;
                int x1, y1; node_pos(w, s + 1, m, &x1, &y1, cam);
                draw_line(fb, x0 + 10, y0 + 10, x1 + 10, y1 + 10, PALETTE[C_BORDER]);
            }
        }
    }

    for (int s = 0; s < SECTORS; ++s) {
        for (int n = 0; n < NODES_PER; ++n) {
            Node *nd = &w->node[s][n];
            if (!nd->active) continue;
            int x, y; node_pos(w, s, n, &x, &y, cam);
            uint32_t c = node_colour(nd->type);

            if (nd->type == NODE_GREEN) {
                // The goal: a lit block with a growing thing on it, the only
                // green in the world.
                draw_drop(fb, x - 4, y - 4, 28, 28);
                fill_rect(fb, x - 4, y - 4, 28, 28, PALETTE[C_GREEN]);
                fill_rect(fb, x - 4, y + 20, 28, 4, PALETTE[C_ROAD]);
                draw_bevel(fb, x - 4, y - 4, 28, 28, 0);
                fill_rect(fb, x + 9, y + 4, 2, 14, PALETTE[C_BONE]);
                fill_rect(fb, x + 4, y + 6, 12, 2, PALETTE[C_BONE]);
                fill_rect(fb, x + 6, y + 10, 8, 2, PALETTE[C_BONE]);
            } else if (nd->type == NODE_EMPTY) {
                fill_rect(fb, x + 7, y + 7, 6, 6, PALETTE[C_INK]);
                fill_rect(fb, x + 8, y + 8, 4, 4, c);
            } else {
                draw_drop(fb, x, y, 20, 20);
                fill_rect(fb, x, y, 20, 20, PALETTE[C_INK]);
                // Flat badge: the inner mark is drawn in ink, so the field
                // behind it has to stay bright the whole way down.
                fill_rect(fb, x + 2, y + 2, 16, 16, c);
                fill_rect(fb, x + 2, y + 16, 16, 2, PALETTE[C_ROAD]);
                draw_bevel(fb, x, y, 20, 20, 0);

                if (nd->type == NODE_EVENT) {
                    draw_glyph(fb, x + 6, y + 6, G_X, 1, PALETTE[C_INK]);
                } else if (nd->type == NODE_HAZARD) {
                    // Stacked bars: a storm front.
                    fill_rect(fb, x + 4, y + 6,  12, 2, PALETTE[C_INK]);
                    fill_rect(fb, x + 6, y + 10, 10, 2, PALETTE[C_INK]);
                    fill_rect(fb, x + 4, y + 14, 12, 2, PALETTE[C_INK]);
                } else {
                    // Settlement: a roofline.
                    fill_rect(fb, x + 5, y + 10, 10, 6, PALETTE[C_INK]);
                    fill_rect(fb, x + 7, y + 6,   6, 4, PALETTE[C_INK]);
                }
            }

            if (nd->visited) draw_rect(fb, x - 1, y - 1, 22, 22, PALETTE[C_DIM]);
        }
    }

    // Current position marker.
    int cx, cy; node_pos(w, w->sector, w->index, &cx, &cy, cam);
    draw_rect(fb, cx - 5, cy - 5, 30, 30, PALETTE[C_BONE]);
    draw_rect(fb, cx - 4, cy - 4, 28, 28, PALETTE[C_BONE]);

    // Highlight the selected destination.
    if (ncand > 0) {
        int m = cand[gs->map_sel % ncand];
        int nx, ny; node_pos(w, w->sector + 1, m, &nx, &ny, cam);
        draw_rect(fb, nx - 6, ny - 6, 32, 32, PALETTE[C_WARN]);
        draw_line(fb, cx + 10, cy + 10, nx + 10, ny + 10, PALETTE[C_WARN]);
        // Fuel cost of the hop, and the key that commits to it.
        draw_glyph(fb, (cx + nx) / 2 - 6, (cy + ny) / 2 - 20, G_MINUS, 1, PALETTE[C_WARN]);
        draw_icon(fb, (cx + nx) / 2 + 2, (cy + ny) / 2 - 24, ICON_FUEL, 1);
        draw_number(fb, (cx + nx) / 2 + 20, (cy + ny) / 2 - 20, 1, 1, PALETTE[C_WARN]);
        // Name the destination and price the hop, in a strip along the bottom
        // where it cannot collide with the route.
        const Node *dest = &w->node[w->sector + 1][m];
        const char *label = node_name(dest->type);
        int by = fb->h - 54;
        draw_panel(fb, 20, by, fb->w - 40, 42);

        int tx = 34;
        tx += draw_key(fb, tx, by + 10, G_KEY_Z, 2) + 8;
        tx += draw_text(fb, tx, by + 16, T_TRAVEL, 1, PALETTE[C_BONE]) + 14;
        tx += draw_text(fb, tx, by + 16, label, 1,
                        dest->type == NODE_GREEN  ? PALETTE[C_GOOD] :
                        dest->type == NODE_HAZARD ? PALETTE[C_WARN] :
                        dest->type == NODE_EVENT  ? PALETTE[C_BAD]  : PALETTE[C_DIM]);

        // Cost of the hop, right-aligned.
        int rx = fb->w - 40 - 60;
        rx += draw_text(fb, rx, by + 16, T_COST, 1, PALETTE[C_DIM]) + 8;
        draw_glyph(fb, rx, by + 14, G_MINUS, 1, PALETTE[C_WARN]);
        draw_icon(fb, rx + 8, by + 10, ICON_FUEL, 1);
        draw_number(fb, rx + 28, by + 14, dest->type == NODE_HAZARD ? 2 : 1, 1,
                    PALETTE[C_WARN]);

        // Up/down move the selection when there is a choice to make.
        if (ncand > 1) {
            draw_glyph(fb, cx + 44, cy - 4,  G_UP,   2, PALETTE[C_BONE]);
            draw_glyph(fb, cx + 44, cy + 18, G_DOWN, 2, PALETTE[C_BONE]);
        }
    }

    draw_text_c(fb, fb->w / 2, 44, T_ROUTE, 1, PALETTE[C_BONE]);

    // Where this stretch of road sits in the whole journey.
    {
        const int bx = 120, bw = fb->w - 240, by = 60;
        fill_rect(fb, bx, by, bw, 6, PALETTE[C_INK]);
        draw_rect(fb, bx, by, bw, 6, PALETTE[C_BORDER]);
        int done = bw * w->sector / (SECTORS - 1);
        fill_rect(fb, bx, by, done, 6, PALETTE[C_WARN]);
        // The goal marker always sits at the far right.
        fill_rect(fb, bx + bw - 3, by - 2, 3, 10, PALETTE[C_GREEN]);
    }
}

// ---------------------------------------------------------------- trade
void ui_trade(Framebuffer *fb, GameState *gs) {
    World *w = &gs->w;
    Node *nd = &w->node[w->sector][w->index];

    const int x = 26, y = 58, pw = 420, rowh = 32;
    // One extra row below the goods for the depart action, so leaving the
    // market looks like another menu choice rather than a hidden key.
    draw_panel(fb, x, y, pw, 34 + (GOODS_COUNT + 1) * rowh);

    draw_text(fb, x + 12, y + 10, T_MARKET, 2, PALETTE[C_BONE]);
    // Column headings, so a bare number is never left to be guessed at.
    draw_text(fb, x + 150, y + 14, T_PRICE, 1, PALETTE[C_DIM]);
    draw_text(fb, x + pw - 20 - text_w(T_HELD, 1), y + 14, T_HELD, 1, PALETTE[C_DIM]);

    for (int g = 0; g < GOODS_COUNT; ++g) {
        int ry = y + 32 + g * rowh;
        if (g == gs->sel) {
            // Dark fill, not the mid-brown border colour: the row carries
            // small text and needs the contrast underneath it.
            fill_rect(fb, x + 4, ry - 4, pw - 8, rowh - 2, PALETTE[C_ROAD]);
            draw_rect(fb, x + 4, ry - 4, pw - 8, rowh - 2, PALETTE[C_BONE]);
        }
        draw_icon(fb, x + 10, ry + 1, g, 1);
        draw_text(fb, x + 34, ry + 5, GOOD_NAME[g], 1,
                  g == gs->sel ? PALETTE[C_BONE] : PALETTE[C_DIM]);
        draw_number(fb, x + 150, ry + 3, nd->price[g], 2, PALETTE[C_BONE]);

        // On the selected row, the two trade actions appear inline and named.
        if (g == gs->sel) {
            int kx = x + 210;
            kx += draw_key(fb, kx, ry - 1, G_KEY_Z, 2) + 4;
            kx += draw_text(fb, kx, ry + 5, T_BUY, 1, PALETTE[C_BONE]) + 12;
            kx += draw_key(fb, kx, ry - 1, G_X, 2) + 4;
            draw_text(fb, kx, ry + 5, T_SELL, 1, PALETTE[C_BONE]);
        }

        // Held quantity, right side.
        int qw = number_w(w->held[g], 2);
        draw_number(fb, x + pw - 20 - qw, ry + 3, w->held[g], 2,
                    w->held[g] ? PALETTE[C_BONE] : PALETTE[C_BORDER]);
    }

    // Departing the market: its own row, reading as "leave, onward".
    {
        int dy = y + 32 + GOODS_COUNT * rowh;
        fill_rect(fb, x + 4, dy - 4, pw - 8, rowh - 2, PALETTE[C_INK]);
        int dx = x + 10;
        dx += draw_key(fb, dx, dy - 1, G_ENTER, 2) + 8;
        dx += draw_text(fb, dx, dy + 5, T_DEPART, 1, PALETTE[C_BONE]) + 10;
        draw_glyph(fb, dx, dy + 3, G_RIGHT, 2, PALETTE[C_DIM]);
    }

    // What the selected good is actually for -- the single most useful line
    // on the screen for a player who has never seen it before.
    {
        int ty = y + 42 + (GOODS_COUNT + 1) * rowh;
        int tw = text_w(GOOD_USE[gs->sel], 1);
        fill_scrim(fb, x + 6, ty - 4, tw + 16, 18, PALETTE[C_INK], 13);
        draw_text(fb, x + 14, ty, GOOD_USE[gs->sel], 1, PALETTE[C_WARN]);
    }

    // Cargo hold below, the run's health bar.
    const int cell = 20, cols = 15;
    int cy = y + 62 + (GOODS_COUNT + 1) * rowh;
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
static const char *event_title(int kind) {
    switch (kind) {
    case EV_RAID:   return T_RAIDERS;
    case EV_WRECK:  return T_WRECK;
    case EV_SICK:   return T_SICK;
    case EV_BREAK:  return T_BREAKDOWN;
    default:        return T_TRADER;
    }
}
static const char *event_accept(int kind) {
    switch (kind) {
    case EV_RAID:   return T_RAIDERS_A;
    case EV_WRECK:  return T_WRECK_A;
    case EV_SICK:   return T_SICK_A;
    case EV_BREAK:  return T_BREAK_A;
    default:        return T_TRADER_A;
    }
}
static const char *event_decline(int kind) {
    switch (kind) {
    case EV_RAID:   return T_RAIDERS_B;
    case EV_WRECK:  return T_WRECK_B;
    case EV_SICK:   return T_SICK_B;
    case EV_BREAK:  return T_BREAK_B;
    default:        return T_TRADER_B;
    }
}

void ui_event(Framebuffer *fb, GameState *gs) {
    World *w = &gs->w;
    const Event *e = &w->event;
    const int x = 110, y = 88, pw = 420, ph = 280;

    int threat = (e->kind == EV_RAID || e->kind == EV_SICK || e->kind == EV_BREAK);
    uint32_t frame = threat ? PALETTE[C_BAD] : PALETTE[C_GOOD];

    draw_panel(fb, x, y, pw, ph);
    fill_rect(fb, x + 3, y + 3, pw - 6, 10, frame);

    // Name the situation. The icons show the price; only words can say why.
    draw_text_c(fb, x + pw / 2, y + 24, event_title(e->kind), 2, PALETTE[C_BONE]);

    int affordable = world_can_accept(w);

    draw_text(fb, x + 20, y + 52, event_accept(e->kind), 1,
              affordable ? PALETTE[C_BONE] : PALETTE[C_DIM]);
    if (!affordable)
        draw_text(fb, x + 20 + text_w(event_accept(e->kind), 1) + 12, y + 52,
                  T_CANNOT, 1, PALETTE[C_BAD]);

    // --- accept -------------------------------------------------------
    int ay = y + 66;
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
    draw_text(fb, x + 20, y + 156, event_decline(e->kind), 1, PALETTE[C_DIM]);
    int by = y + 170;
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
// A row of "icon xN" summary figures, centred as a group.
static void draw_summary(Framebuffer *fb, const World *w, int cx, int y) {
    const int items[3] = { ICON_WATER, ICON_FUEL, ICON_SCRAP };
    const int vals[3]  = { w->held[G_WATER], w->held[G_FUEL], world_cargo(w) };
    int total = 0;
    for (int i = 0; i < 3; ++i) total += 34 + number_w(vals[i], 2) + 22;

    int x = cx - total / 2;
    for (int i = 0; i < 3; ++i) {
        draw_icon(fb, x, y, items[i], 1);
        x += 22;
        x += draw_number(fb, x, y + 4, vals[i], 2, PALETTE[C_BONE]) + 34;
    }
}

void ui_title(Framebuffer *fb, GameState *gs) {
    scene_draw(fb, gs->tick, 0, 20);

    // The convoy drives the width of the screen and wraps, so the title screen
    // is never still.
    int drive = (int)((gs->tick * 2) % (uint32_t)(fb->w + 260)) - 200;
    draw_convoy(fb, drive, fb->h / 2 - 30, 4, gs->tick, 0);

    // The goal, waiting at the right-hand edge.
    int gx = fb->w - 70, gy = fb->h / 2 - 34;
    draw_drop(fb, gx, gy, 40, 40);
    fill_rect(fb, gx, gy, 40, 40, PALETTE[C_GREEN]);
    fill_rect(fb, gx, gy + 32, 40, 8, PALETTE[C_ROAD]);
    draw_bevel(fb, gx, gy, 40, 40, 0);
    fill_rect(fb, gx + 18, gy + 10, 4, 22, PALETTE[C_BONE]);
    fill_rect(fb, gx + 10, gy + 14, 20, 4, PALETTE[C_BONE]);
    fill_rect(fb, gx + 13, gy + 22, 14, 4, PALETTE[C_BONE]);

    // Wordmark and tagline.
    int cx = fb->w / 2;
    draw_text_c(fb, cx, 44, T_TITLE, 6, PALETTE[C_INK]);
    draw_text_c(fb, cx - 3, 41, T_TITLE, 6, PALETTE[C_BONE]);
    {
        // The sun sits directly behind this line, so it needs its own ground.
        int tw = text_w(T_TAGLINE, 1);
        fill_scrim(fb, cx - tw / 2 - 10, 92, tw + 20, 18, PALETTE[C_INK], 13);
        draw_text_c(fb, cx, 96, T_TAGLINE, 1, PALETTE[C_WARN]);
    }

    // Prompts, pulsing so they read as things to press.
    const int py = fb->h - 78;
    fill_scrim(fb, cx - 200, py - 8, 400, 56, PALETTE[C_INK], 11);
    if ((gs->tick / 24) & 1)
        draw_text_c(fb, cx, py, T_START, 2, PALETTE[C_BONE]);
    draw_text_c(fb, cx, py + 28, T_HELP_HINT, 1, PALETTE[C_WARN]);
}

// The instructions. This exists because the game is otherwise a guessing
// game: icons can show a price but they cannot explain why cargo is health.
void ui_help(Framebuffer *fb, GameState *gs) {
    scene_draw(fb, gs->tick, 3, 40);
    fill_scrim(fb, 0, 0, fb->w, fb->h, PALETTE[C_INK], 12);

    const int x = 40, y = 30, pw = fb->w - 80, ph = fb->h - 60;
    draw_panel(fb, x, y, pw, ph);

    int cx = fb->w / 2;
    draw_text_c(fb, cx, y + 16, T_HELP_TITLE, 3, PALETTE[C_BONE]);

    const char *lines[9] = {
        T_HELP_1, T_HELP_2, T_HELP_3, T_HELP_4, T_HELP_5,
        T_HELP_6, T_HELP_7, T_HELP_8, T_HELP_9
    };
    int ly = y + 56;
    for (int i = 0; i < 9; ++i) {
        // The two lines that state the core rule are lit; the rest is body.
        uint32_t c = (i == 6 || i == 7 || i == 8) ? PALETTE[C_WARN] : PALETTE[C_BONE];
        draw_text(fb, x + 24, ly, lines[i], 1, c);
        ly += 15;
        if (i == 2 || i == 5) ly += 8;
    }

    // The five goods, each with what it is actually for.
    ly += 10;
    for (int g = 0; g < GOODS_COUNT; ++g) {
        draw_icon(fb, x + 24, ly - 4, g, 1);
        draw_text(fb, x + 48, ly, GOOD_NAME[g], 1, PALETTE[C_BONE]);
        draw_text(fb, x + 124, ly, GOOD_USE[g], 1, PALETTE[C_DIM]);
        ly += 20;
    }

    draw_text_c(fb, cx, y + ph - 46, T_HELP_KEYS, 1, PALETTE[C_BONE]);
    draw_text_c(fb, cx, y + ph - 30, T_HELP_KEYS2, 1, PALETTE[C_DIM]);
    if ((gs->tick / 24) & 1)
        draw_text_c(fb, cx, y + ph - 14, T_BACK, 1, PALETTE[C_WARN]);
}

void ui_end(Framebuffer *fb, GameState *gs, int won) {
    World *w = &gs->w;

    scene_draw(fb, gs->tick, won ? SECTORS - 1 : w->sector, won ? 0 : 200);
    // Wash the whole scene toward triumph or toward dust.
    fill_scrim(fb, 0, 0, fb->w, fb->h,
               won ? PALETTE[C_GREEN] : PALETTE[C_INK], won ? 4 : 8);

    int cx = fb->w / 2;

    if (won) {
        // Parked at the Green Zone.
        int gx = cx + 60, gy = fb->h / 2 - 70;
        draw_drop(fb, gx, gy, 46, 46);
        fill_rect(fb, gx, gy, 46, 46, PALETTE[C_GREEN]);
        fill_rect(fb, gx, gy + 38, 46, 8, PALETTE[C_ROAD]);
        draw_bevel(fb, gx, gy, 46, 46, 0);
        fill_rect(fb, gx + 21, gy + 10, 4, 28, PALETTE[C_BONE]);
        fill_rect(fb, gx + 12, gy + 16, 22, 4, PALETTE[C_BONE]);
        draw_convoy(fb, cx - 150, fb->h / 2 - 60, 3, gs->tick, 0);
    } else {
        draw_convoy(fb, cx - 40, fb->h / 2 - 60, 3, gs->tick, 1);
    }

    // What ended the run, as an icon with a cross through it.
    const int py = fb->h - 150;
    draw_panel(fb, cx - 180, py, 360, 108);

    if (!won) {
        int icon = w->death == DEATH_THIRST   ? ICON_WATER :
                   w->death == DEATH_STRANDED ? ICON_FUEL  : ICON_SCRAP;
        draw_icon(fb, cx - 150, py + 18, icon, 2);
        draw_glyph(fb, cx - 112, py + 26, G_X, 3, PALETTE[C_BAD]);
    } else {
        // Arrival is stated with the Green Zone's own mark, not a health icon.
        int bx = cx - 150, by = py + 18;
        fill_rect(fb, bx, by, 32, 32, PALETTE[C_GREEN]);
        fill_rect(fb, bx, by + 26, 32, 6, PALETTE[C_ROAD]);
        draw_bevel(fb, bx, by, 32, 32, 0);
        fill_rect(fb, bx + 14, by + 7, 4, 19, PALETTE[C_BONE]);
        fill_rect(fb, bx + 8,  by + 11, 16, 4, PALETTE[C_BONE]);
        fill_rect(fb, bx + 10, by + 18, 12, 3, PALETTE[C_BONE]);
    }

    // What happened, in words. The icon says which resource; only this says why.
    const char *verdict = won ? T_ARRIVED :
        (w->death == DEATH_THIRST   ? T_DIED_THIRST :
         w->death == DEATH_STRANDED ? T_DIED_FUEL   : T_DIED_STRIP);
    draw_text_c(fb, cx + 24, py + 20, verdict, 1,
                won ? PALETTE[C_GOOD] : PALETTE[C_BAD]);

    // Day reached and credits banked: the score.
    int x = cx - 44;
    draw_text(fb, x, py + 40, T_REACHED, 1, PALETTE[C_DIM]);
    draw_number(fb, x + text_w(T_REACHED, 1) + 8, py + 36, w->day, 2, PALETTE[C_BONE]);
    draw_text(fb, x, py + 58, T_BANKED, 1, PALETTE[C_DIM]);
    draw_number(fb, x + text_w(T_REACHED, 1) + 8, py + 54, w->credits, 2, PALETTE[C_WARN]);

    draw_summary(fb, w, cx, py + 76);

    if ((gs->tick / 24) & 1)
        draw_text_c(fb, cx, py + 94, T_AGAIN, 1, PALETTE[C_BONE]);
}
