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
const char *const ARCH_NAME[6] = {
    T_ARCH_WELL, T_ARCH_REFINERY, T_ARCH_ARMOURY,
    T_ARCH_CLINIC, T_ARCH_SCRAPYARD, T_ARCH_GENERAL
};
const char *const UPG_NAME[UPG_COUNT] = {
    T_UPG_HOLD, T_UPG_ECON, T_UPG_ARMOUR, T_UPG_TANKS
};
const char *const UPG_DESC[UPG_COUNT] = {
    T_UPG_HOLD_D, T_UPG_ECON_D, T_UPG_ARMOUR_D, T_UPG_TANKS_D
};
const char *const CREW_NAME[CREW_COUNT] = {
    T_CREW_MECHANIC, T_CREW_GUARD, T_CREW_MEDIC, T_CREW_SCOUT, T_CREW_TRADER
};
const char *const CREW_DESC[CREW_COUNT] = {
    T_CREW_MECHANIC_D, T_CREW_GUARD_D, T_CREW_MEDIC_D,
    T_CREW_SCOUT_D, T_CREW_TRADER_D
};
const char *const ARCH_DESC[6] = {
    T_ARCH_WELL_D, T_ARCH_REFINERY_D, T_ARCH_ARMOURY_D,
    T_ARCH_CLINIC_D, T_ARCH_SCRAPYARD_D, T_ARCH_GENERAL_D
};

static const char *const TAB_NAME[TAB_COUNT] = {
    T_TAB_MARKET, T_TAB_GARAGE, T_TAB_CREW, T_TAB_CONTRACTS
};

int ui_tab_rows(const GameState *gs, int tab) {
    switch (tab) {
    case TAB_MARKET:    return GOODS_COUNT;
    // One row, and only when there is something to press it for.
    case TAB_CONTRACTS: return gs->w.job.state == CONTRACT_OFFERED ? 1 : 0;
    case TAB_GARAGE:    return gs->w.offer_upg  < UPG_COUNT  ? 1 : 0;
    case TAB_CREW:      return gs->w.offer_crew < CREW_COUNT ? 1 : 0;
    default:            return 0;
    }
}

// A tab is shown when it has something in it. Empty tabs are not drawn at all,
// so no placeholder UI ever reaches a player.
static int tab_live(const GameState *gs, int tab) {
    if (tab == TAB_MARKET) return 1;
    if (tab == TAB_CONTRACTS) return gs->w.job.state != CONTRACT_NONE;
    // The garage and the crew board only exist where there is something on
    // them, or where the convoy already carries something worth reviewing.
    if (tab == TAB_GARAGE) {
        if (gs->w.offer_upg < UPG_COUNT) return 1;
        for (int i = 0; i < UPG_COUNT; ++i) if (gs->w.upgrade[i]) return 1;
        return 0;
    }
    if (tab == TAB_CREW) {
        if (gs->w.offer_crew < CREW_COUNT) return 1;
        return world_crew_count(&gs->w) > 0;
    }
    return 0;
}

static int tab_count_live(const GameState *gs) {
    int n = 0;
    for (int t = 0; t < TAB_COUNT; ++t) n += tab_live(gs, t);
    return n;
}

// Draws the tab strip and returns the height it consumed.
static int draw_tabs(Framebuffer *fb, const GameState *gs, int x, int y, int pw) {
    if (tab_count_live(gs) < 2) return 0;
    int tx = x + 10;
    for (int t = 0; t < TAB_COUNT; ++t) {
        if (!tab_live(gs, t)) continue;
        int tw = text_w(TAB_NAME[t], 1) + 16;
        int on = (t == gs->tab);
        fill_rect(fb, tx, y, tw, 20, PALETTE[on ? C_BORDER : C_INK]);
        if (on) draw_rect(fb, tx, y, tw, 20, PALETTE[C_BONE]);
        draw_text(fb, tx + 8, y + 6, TAB_NAME[t], 1,
                  PALETTE[on ? C_BONE : C_DIM]);
        tx += tw + 4;
    }
    // Which keys move between them.
    draw_text(fb, x + pw - 60, y + 6, "< >", 1, PALETTE[C_DIM]);
    return 26;
}

// Shared layout for the two outfitting boards: what is for sale, what it does,
// what it costs, and what is already aboard.
static void draw_outfit(Framebuffer *fb, GameState *gs, int x, int y, int pw,
                        int offer, int count, const char *const *names,
                        const char *const *descs, const uint8_t *owned,
                        int price, const char *empty_msg,
                        const char *buy_msg, const char *own_msg)
{
    if (offer >= count) {
        draw_text(fb, x + 14, y + 12, empty_msg, 1, PALETTE[C_DIM]);
    } else {
        draw_text(fb, x + 14, y + 10, names[offer], 2, PALETTE[C_BONE]);
        draw_text(fb, x + 14, y + 32, descs[offer], 1, PALETTE[C_WARN]);

        int px = x + 14;
        int can = gs->w.credits >= price;
        px += draw_key(fb, px, y + 52, G_KEY_Z, 2) + 8;
        px += draw_text(fb, px, y + 58, buy_msg, 1,
                        can ? PALETTE[C_BONE] : PALETTE[C_DIM]) + 12;
        draw_number(fb, px, y + 54, price, 2,
                    can ? PALETTE[C_WARN] : PALETTE[C_BAD]);
    }

    // Everything already fitted or aboard, so the tab is a status board too.
    int ly = y + 86, any = 0;
    for (int i = 0; i < count; ++i) {
        if (!owned[i]) continue;
        draw_text(fb, x + 18, ly, own_msg, 1, PALETTE[C_GOOD]);
        draw_text(fb, x + 18 + text_w(own_msg, 1) + 10, ly, names[i], 1,
                  PALETTE[C_BONE]);
        ly += 15;
        any = 1;
    }
    (void)any;
}

// Everything the garage knows that the base panel does not: the condition of
// what is on offer, what it should return, and what is still on the road.
static void draw_garage_extra(Framebuffer *fb, GameState *gs, int x, int y, int pw) {
    const World *w = &gs->w;
    (void)pw;

    if (w->offer_upg < UPG_COUNT) {
        int salv = w->offer_salvaged;
        int cx = x + 210;
        draw_text(fb, cx, y + 12, salv ? T_SALVAGED : T_SOUND, 1,
                  PALETTE[salv ? C_BAD : C_GOOD]);
        draw_text(fb, x + 14, y + 76, salv ? T_SALVAGE_WARN : T_SOUND_NOTE, 1,
                  PALETTE[salv ? C_BAD : C_DIM]);

        // What it is expected to earn back, next to what it costs, so the
        // player can see the bet rather than having to infer it.
        int px = x + 14;
        px += draw_text(fb, px, y + 92, T_PAYS_BACK, 1, PALETTE[C_DIM]) + 8;
        draw_number(fb, px, y + 92, world_upg_payback(w, w->offer_upg), 1,
                    PALETTE[C_WARN]);
    }

    // The road east, counted off the map the player can already see.
    int storms = 0, events = 0;
    world_road_ahead(w, &storms, &events);
    int ry = y + 118;
    draw_text(fb, x + 14, ry, T_ROAD_AHEAD, 1, PALETTE[C_BONE]);
    int rx = x + 14;
    rx += text_w(T_ROAD_AHEAD, 1) + 16;
    rx += draw_number(fb, rx, ry, storms, 1, PALETTE[C_WARN]) + 6;
    rx += draw_text(fb, rx, ry, T_AHEAD_STORMS, 1, PALETTE[C_DIM]) + 14;
    rx += draw_number(fb, rx, ry, events, 1, PALETTE[C_BAD]) + 6;
    draw_text(fb, rx, ry, T_AHEAD_EVENTS, 1, PALETTE[C_DIM]);
}

// ---------------------------------------------------------------- contracts
static void draw_contracts(Framebuffer *fb, GameState *gs, int x, int y, int pw) {
    const Contract *j = &gs->w.job;

    if (j->state == CONTRACT_NONE) {
        draw_text(fb, x + 14, y + 12, T_NO_WORK, 1, PALETTE[C_DIM]);
        return;
    }

    int offered = (j->state == CONTRACT_OFFERED);
    draw_text(fb, x + 14, y + 10, offered ? T_JOB_OFFER : T_JOB_TAKEN, 2,
              PALETTE[offered ? C_BONE : C_WARN]);

    // What, and how much of it.
    int ry = y + 40;
    draw_icon(fb, x + 16, ry, j->good, 2);
    int tx = x + 56;
    draw_glyph(fb, tx, ry + 10, G_X, 2, PALETTE[C_BONE]);
    tx += glyph_w(2) + 4;
    tx += draw_number(fb, tx, ry + 10, j->qty, 2, PALETTE[C_BONE]) + 16;
    draw_text(fb, tx, ry + 14, GOOD_NAME[j->good], 1, PALETTE[C_DIM]);

    // Where, and what it pays.
    draw_text(fb, x + 14, ry + 44, T_JOB_DELIVER, 1, PALETTE[C_DIM]);
    draw_number(fb, x + 14 + text_w(T_JOB_DELIVER, 1) + 8, ry + 44,
                j->by_sector, 1, PALETTE[C_BONE]);

    int py = ry + 62;
    int px = x + 14;
    px += draw_text(fb, px, py, T_JOB_PAYS, 1, PALETTE[C_DIM]) + 8;
    draw_number(fb, px, py - 4, j->reward, 2, PALETTE[C_WARN]);

    if (offered) {
        int ky = py + 22;
        int kx = x + 14;
        kx += draw_key(fb, kx, ky - 6, G_KEY_Z, 2) + 8;
        draw_text(fb, kx, ky, T_JOB_ACCEPT, 1, PALETTE[C_BONE]);
    } else {
        // Progress, and a reminder that this cargo is spoken for.
        int ky = py + 22;
        int kx = x + 14;
        kx += draw_text(fb, kx, ky, T_JOB_HOLDING, 1, PALETTE[C_DIM]) + 8;
        kx += draw_number(fb, kx, ky, gs->w.held[j->good], 1,
                          gs->w.held[j->good] >= j->qty
                              ? PALETTE[C_GOOD] : PALETTE[C_BAD]);
        draw_glyph(fb, kx + 2, ky, G_SLASH, 1, PALETTE[C_DIM]);
        draw_number(fb, kx + glyph_w(1) + 4, ky, j->qty, 1, PALETTE[C_DIM]);
        draw_text(fb, x + 14, ky + 16, T_JOB_LOCKED, 1, PALETTE[C_DIM]);
    }
}

// ---------------------------------------------------------------- backdrop
void ui_backdrop(Framebuffer *fb, GameState *gs) {
    const World *w = &gs->w;
    int thirst = w->held[G_WATER] < 4 ? (4 - w->held[G_WATER]) * 30 : 0;

    // The whole run is one day: first light at the start, dark by the Green
    // Zone. A glance at the sky says how far you have come.
    int phase = w->sector * 255 / (SECTORS - 1);

    int weather = WX_CLEAR;
    if (w->node[w->sector][w->index].type == NODE_HAZARD) weather = WX_STORM;
    else if (w->sector >= (SECTORS - 1) / 2)              weather = WX_HAZE;

    scene_draw(fb, gs->tick, w->sector, w->sector * 20 + thirst, phase, weather);
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

    // Cargo occupancy: held / capacity, which racks can raise.
    x += draw_text(fb, x, 15, T_HOLD, 1, PALETTE[C_DIM]) + 6;
    x += draw_number(fb, x, 13, world_cargo(w), 2, PALETTE[C_BONE]);
    draw_glyph(fb, x, 13, G_SLASH, 2, PALETTE[C_DIM]); x += glyph_w(2);
    x += draw_number(fb, x, 13, world_cargo_cap(w), 2, PALETTE[C_DIM]) + 16;

    // Crew, and what they cost in water every day.
    if (world_crew_count(w) > 0) {
        x += draw_text(fb, x, 15, T_CREW_COUNT, 1, PALETTE[C_DIM]) + 6;
        x += draw_number(fb, x, 13, world_crew_count(w), 2, PALETTE[C_BONE]) + 8;
        draw_glyph(fb, x, 13, G_MINUS, 2, PALETTE[C_BAD]); x += glyph_w(2);
        draw_icon(fb, x, 9, ICON_WATER, 1); x += 18;
        x += draw_number(fb, x, 13, world_water_burn(w), 2, PALETTE[C_BAD]);
    }

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
                    // Settlement: a roofline, and for a specialist the good it
                    // deals in, so the map reads as an economy rather than a
                    // row of identical shops.
                    int spec = world_arch_good(nd->archetype);
                    if (spec >= 0) {
                        fill_rect(fb, x + 2, y + 2, 16, 16, PALETTE[C_INK]);
                        draw_icon(fb, x + 2, y + 2, spec, 1);
                    } else {
                        fill_rect(fb, x + 5, y + 10, 10, 6, PALETTE[C_INK]);
                        fill_rect(fb, x + 7, y + 6,   6, 4, PALETTE[C_INK]);
                    }
                }
            }

            if (nd->visited) draw_rect(fb, x - 1, y - 1, 22, 22, PALETTE[C_DIM]);
        }
    }

    // Current position marker. During a hop the convoy is drawn part-way along
    // the link it took, which is the difference between travelling and
    // teleporting.
    int cx, cy; node_pos(w, w->sector, w->index, &cx, &cy, cam);
    if (gs->travel > 0) {
        int fx, fy; node_pos(w, gs->from_sector, gs->from_index, &fx, &fy, cam);
        int t = 26 - gs->travel;                 // 0..26 along the way
        int dx = fx + (cx - fx) * t / 26;
        int dy = fy + (cy - fy) * t / 26;
        draw_convoy(fb, dx - 20, dy - 6, 1, gs->tick, 0);
    }
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
    draw_panel(fb, x, y, pw, 50 + (GOODS_COUNT + 1) * rowh);

    draw_text(fb, x + 12, y + 10, ARCH_NAME[nd->archetype], 2, PALETTE[C_BONE]);
    draw_text(fb, x + 12, y + 30, ARCH_DESC[nd->archetype], 1, PALETTE[C_DIM]);

    int th = draw_tabs(fb, gs, x, y + 44, pw);

    // Salvaged kit giving out is announced, not silently accounted.
    if (w->kit_failed >= 0) {
        int px = x + 12;
        px += draw_text(fb, px, y + 32, UPG_NAME[w->kit_failed], 1, PALETTE[C_BAD]) + 8;
        draw_text(fb, px, y + 32, T_KIT_BROKE, 1, PALETTE[C_BAD]);
    }

    // A delivery that paid out on arrival says so before anything else.
    if (w->job_paid > 0) {
        int px = x + pw - 150;
        px += draw_text(fb, px, y + 32, T_JOB_DONE, 1, PALETTE[C_GOOD]) + 8;
        draw_number(fb, px, y + 32, w->job_paid, 1, PALETTE[C_WARN]);
    }

    if (gs->tab != TAB_MARKET) {
        if (gs->tab == TAB_CONTRACTS) draw_contracts(fb, gs, x, y + 44 + th, pw);
        else if (gs->tab == TAB_GARAGE) {
            draw_outfit(fb, gs, x, y + 44 + th, pw, gs->w.offer_upg, UPG_COUNT,
                        UPG_NAME, UPG_DESC, gs->w.upgrade,
                        gs->w.offer_upg < UPG_COUNT
                            ? world_upg_price(&gs->w, gs->w.offer_upg,
                                              gs->w.offer_salvaged) : 0,
                        T_NO_GARAGE, T_BUY_UPGRADE, T_OWNED);
            draw_garage_extra(fb, gs, x, y + 44 + th, pw);
        }
        else if (gs->tab == TAB_CREW) {
            draw_outfit(fb, gs, x, y + 44 + th, pw, gs->w.offer_crew, CREW_COUNT,
                        CREW_NAME, CREW_DESC, gs->w.crew,
                        gs->w.offer_crew < CREW_COUNT
                            ? world_crew_price(&gs->w, gs->w.offer_crew) : 0,
                        T_NO_CREW, T_HIRE, T_HIRED);
            // The standing cost of every hand aboard, stated where it is felt.
            // Sits directly under the hire prompt: any lower and it collides
            // with the depart row at the foot of the panel.
            draw_text(fb, x + 14, y + 44 + th + 82, T_CREW_WARN, 1, PALETTE[C_BAD]);
        }
        // Departing is always available, whatever tab is open.
        int dy = y + 48 + th + GOODS_COUNT * rowh;
        fill_rect(fb, x + 4, dy - 4, pw - 8, rowh - 2, PALETTE[C_INK]);
        int dx = x + 10;
        dx += draw_key(fb, dx, dy - 1, G_ENTER, 2) + 8;
        dx += draw_text(fb, dx, dy + 5, T_DEPART, 1, PALETTE[C_BONE]) + 10;
        draw_glyph(fb, dx, dy + 3, G_RIGHT, 2, PALETTE[C_DIM]);
        return;
    }

    // Column headings, so a bare number is never left to be guessed at.
    draw_text(fb, x + 150, y + 14, T_PRICE, 1, PALETTE[C_DIM]);
    draw_text(fb, x + pw - 20 - text_w(T_HELD, 1), y + 14, T_HELD, 1, PALETTE[C_DIM]);

    for (int g = 0; g < GOODS_COUNT; ++g) {
        int ry = y + 48 + th + g * rowh;
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
        // Cheap or dear against every price this player has seen.
        draw_trend(fb, x + 186, ry + 3, world_price_bias(w, g), 2);

        // On the selected row, the two trade actions appear inline and named.
        if (g == gs->sel) {
            int kx = x + 210;
            kx += draw_key(fb, kx, ry - 1, G_KEY_Z, 2) + 4;
            kx += draw_text(fb, kx, ry + 5, T_BUY, 1, PALETTE[C_BONE]) + 12;
            kx += draw_key(fb, kx, ry - 1, G_X, 2) + 4;
            kx += draw_text(fb, kx, ry + 5, T_SELL, 1, PALETTE[C_BONE]) + 6;
            // A stall pays less than it charges. Hiding that behind the listed
            // price would make every sale a small unpleasant surprise.
            draw_number(fb, kx, ry + 5, world_sell_price(w, g), 1, PALETTE[C_WARN]);
        }

        // Held quantity, right side.
        int qw = number_w(w->held[g], 2);
        int committed = world_committed(w, g);
        draw_number(fb, x + pw - 20 - qw, ry + 3, w->held[g], 2,
                    committed ? PALETTE[C_WARN]
                              : (w->held[g] ? PALETTE[C_BONE] : PALETTE[C_BORDER]));
    }

    // Departing the market: its own row, reading as "leave, onward".
    {
        int dy = y + 48 + th + GOODS_COUNT * rowh;
        fill_rect(fb, x + 4, dy - 4, pw - 8, rowh - 2, PALETTE[C_INK]);
        int dx = x + 10;
        dx += draw_key(fb, dx, dy - 1, G_ENTER, 2) + 8;
        dx += draw_text(fb, dx, dy + 5, T_DEPART, 1, PALETTE[C_BONE]) + 10;
        draw_glyph(fb, dx, dy + 3, G_RIGHT, 2, PALETTE[C_DIM]);
    }

    // What the selected good is actually for -- the single most useful line
    // on the screen for a player who has never seen it before.
    {
        int ty = y + 58 + th + (GOODS_COUNT + 1) * rowh;
        int tw = text_w(GOOD_USE[gs->sel], 1);
        fill_scrim(fb, x + 6, ty - 4, tw + 16, 18, PALETTE[C_INK], 13);
        draw_text(fb, x + 14, ty, GOOD_USE[gs->sel], 1, PALETTE[C_WARN]);
    }

    // Cargo hold below, the run's health bar.
    const int cell = 20, cols = 15;
    int cy = y + 78 + th + (GOODS_COUNT + 1) * rowh;
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
// Encounter copy, indexed by kind. Tables rather than switches: adding a kind
// should be a row of data, not an edit in three places.
static const char *const EV_TITLE[EV_KINDS] = {
    T_RAIDERS, T_WRECK, T_SICK, T_BREAKDOWN, T_TRADER,
    T_TOLL, T_CACHE, T_BRIDGE, T_RIVAL, T_PLAGUE,
    T_CHECKPOINT, T_LEAK, T_REFUGEE, T_SIGNAL
};
static const char *const EV_ACCEPT[EV_KINDS] = {
    T_RAIDERS_A, T_WRECK_A, T_SICK_A, T_BREAK_A, T_TRADER_A,
    T_TOLL_A, T_CACHE_A, T_BRIDGE_A, T_RIVAL_A, T_PLAGUE_A,
    T_CHECK_A, T_LEAK_A, T_REFUGEE_A, T_SIGNAL_A
};
static const char *const EV_DECLINE[EV_KINDS] = {
    T_RAIDERS_B, T_WRECK_B, T_SICK_B, T_BREAK_B, T_TRADER_B,
    T_TOLL_B, T_CACHE_B, T_BRIDGE_B, T_RIVAL_B, T_PLAGUE_B,
    T_CHECK_B, T_LEAK_B, T_REFUGEE_B, T_SIGNAL_B
};

// Red frames a threat, green an opportunity: readable before any number is.
static int event_is_threat(int kind) {
    switch (kind) {
    case EV_WRECK: case EV_CACHE: case EV_RIVAL:
    case EV_TRADER: case EV_REFUGEE: case EV_SIGNAL:
        return 0;
    default:
        return 1;
    }
}

void ui_event(Framebuffer *fb, GameState *gs) {
    World *w = &gs->w;
    const Event *e = &w->event;
    const int x = 110, y = 88, pw = 420, ph = 280;

    int threat = event_is_threat(e->kind);
    uint32_t frame = threat ? PALETTE[C_BAD] : PALETTE[C_GOOD];

    draw_panel(fb, x, y, pw, ph);
    fill_rect(fb, x + 3, y + 3, pw - 6, 10, frame);

    // Name the situation. The icons show the price; only words can say why.
    draw_text_c(fb, x + pw / 2, y + 24, EV_TITLE[e->kind], 2, PALETTE[C_BONE]);

    int affordable = world_can_accept(w);

    draw_text(fb, x + 20, y + 52, EV_ACCEPT[e->kind], 1,
              affordable ? PALETTE[C_BONE] : PALETTE[C_DIM]);
    if (!affordable)
        draw_text(fb, x + 20 + text_w(EV_ACCEPT[e->kind], 1) + 12, y + 52,
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
    draw_text(fb, x + 20, y + 156, EV_DECLINE[e->kind], 1, PALETTE[C_DIM]);
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
    scene_draw(fb, gs->tick, 0, 20, 30, WX_CLEAR);

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
    scene_draw(fb, gs->tick, 3, 40, 70, WX_CLEAR);
    fill_scrim(fb, 0, 0, fb->w, fb->h, PALETTE[C_INK], 12);

    const int x = 40, y = 30, pw = fb->w - 80, ph = fb->h - 60;
    draw_panel(fb, x, y, pw, ph);

    int cx = fb->w / 2;
    draw_text_c(fb, cx, y + 16, T_HELP_TITLE, 3, PALETTE[C_BONE]);

    const char *lines[15] = {
        T_HELP_1, T_HELP_2, T_HELP_3, T_HELP_4, T_HELP_5, T_HELP_6,
        T_HELP_10, T_HELP_11, T_HELP_12, T_HELP_13, T_HELP_14, T_HELP_15,
        T_HELP_7, T_HELP_8, T_HELP_9
    };
    int ly = y + 44;
    for (int i = 0; i < 15; ++i) {
        // The closing lines state the core rule, so they are lit; the rest
        // is body copy.
        uint32_t c = (i >= 12) ? PALETTE[C_WARN] : PALETTE[C_BONE];
        draw_text(fb, x + 24, ly, lines[i], 1, c);
        ly += 14;
        if (i == 2 || i == 5 || i == 11) ly += 6;
    }

    // The five goods, each with what it is actually for.
    ly += 8;
    for (int g = 0; g < GOODS_COUNT; ++g) {
        draw_icon(fb, x + 24, ly - 4, g, 1);
        draw_text(fb, x + 48, ly, GOOD_NAME[g], 1, PALETTE[C_BONE]);
        draw_text(fb, x + 124, ly, GOOD_USE[g], 1, PALETTE[C_DIM]);
        ly += 17;
    }

    draw_text_c(fb, cx, y + ph - 46, T_HELP_KEYS, 1, PALETTE[C_BONE]);
    draw_text_c(fb, cx, y + ph - 30, T_HELP_KEYS2, 1, PALETTE[C_DIM]);
    if ((gs->tick / 24) & 1)
        draw_text_c(fb, cx, y + ph - 14, T_BACK, 1, PALETTE[C_WARN]);
}

void ui_end(Framebuffer *fb, GameState *gs, int won) {
    World *w = &gs->w;

    scene_draw(fb, gs->tick, won ? SECTORS - 1 : w->sector, won ? 0 : 200,
               won ? 235 : 210, WX_CLEAR);
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
