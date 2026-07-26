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
const char *const TOWN_A[16] = {
    T_TOWN_A0, T_TOWN_A1, T_TOWN_A2,  T_TOWN_A3,  T_TOWN_A4,  T_TOWN_A5,
    T_TOWN_A6, T_TOWN_A7, T_TOWN_A8,  T_TOWN_A9,  T_TOWN_A10, T_TOWN_A11,
    T_TOWN_A12, T_TOWN_A13, T_TOWN_A14, T_TOWN_A15,
};
const char *const TOWN_B[16] = {
    T_TOWN_B0, T_TOWN_B1, T_TOWN_B2,  T_TOWN_B3,  T_TOWN_B4,  T_TOWN_B5,
    T_TOWN_B6, T_TOWN_B7, T_TOWN_B8,  T_TOWN_B9,  T_TOWN_B10, T_TOWN_B11,
    T_TOWN_B12, T_TOWN_B13, T_TOWN_B14, T_TOWN_B15,
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

// Widest tab strip any frame has drawn this run, for the harness to check.
// Harness-only: the shipped game never asks and never pays for it.
#ifdef CONVOY_INSTRUMENT
#define INSTR_UI(stmt) do { stmt; } while (0)
int ui_strip_worst = 0;     // widest strip drawn this run
int ui_strip_limit = 0;     // where the "< >" hint starts
static void strip_probe(int right, int limit) {
    if (right > ui_strip_worst) ui_strip_worst = right;
    ui_strip_limit = limit;
}
#define UI_PROBE(r, l) strip_probe((r), (l))
#else
#define INSTR_UI(stmt) do { } while (0)
#define UI_PROBE(r, l) do { (void)(r); (void)(l); } while (0)
#endif

static const char *const TAB_NAME[TAB_COUNT] = {
    T_TAB_MARKET, 0 /* the works, named per archetype */, T_TAB_CREW,
    T_TAB_CONTRACTS, T_TAB_JOURNAL
};

const char *const SVC_NAME[6] = {
    T_SVC_WELL, T_SVC_REFINERY, T_SVC_ARMOURY,
    T_SVC_CLINIC, T_SVC_SCRAPYARD, 0,
};
const char *const SVC_DESC[6] = {
    T_SVC_WELL_D, T_SVC_REFINERY_D, T_SVC_ARMOURY_D,
    T_SVC_CLINIC_D, T_SVC_SCRAPYARD_D, 0,
};

const char *const WORKS_NAME[6] = {
    T_WORKS_WELL, T_WORKS_REFINERY, T_WORKS_ARMOURY,
    T_WORKS_CLINIC, T_WORKS_SCRAPYARD, T_WORKS_GENERAL,
};

// What this location is called here. Only the works varies by town, but the
// lookup is by tab so a later phase can name the situation slot the same way
// without every call site learning about a second special case.
static const char *tab_name(const GameState *gs, int t) {
    if (t == TAB_GARAGE)
        return WORKS_NAME[gs->w.node[gs->w.sector][gs->w.index].archetype];
    return TAB_NAME[t];
}

// Who is who, and what they say. Three lines each: a first meeting, a warm
// return and a cold one, picked by the regard the player has earned.
// What each hand offers to do about it. Keyed by role, not by encounter kind:
// fourteen bespoke lines would read better and cost fourteen strings to say
// the same thing five ways.
static const char *const ALT_VERB[CREW_COUNT] = {
    T_ALT_MECHANIC, T_ALT_GUARD, T_ALT_MEDIC, T_ALT_SCOUT, T_ALT_TRADER
};

static const char *const WHO_NAME[CHAR_COUNT] = {
    T_WHO_CHIEF, T_WHO_CAPTAIN, T_WHO_TRADER, T_WHO_DOC, T_WHO_DRIFTER
};
// Eight slots, not three: the first three are the stranger on the road, the
// rest are the same person once they are driving for you. Widening the array
// was a one-token change, which is the whole reason the table idiom is used.
enum { LN_FIRST, LN_WARM, LN_COLD, LN_JOIN, LN_GOOD, LN_BAD, LN_ASK, LN_LEAVE };
static const char *const WHO_LINE[CHAR_COUNT][8] = {
    { T_CHIEF_1,   T_CHIEF_WARM,   T_CHIEF_COLD,
      T_CHIEF_JOIN, T_CHIEF_GOOD, T_CHIEF_BAD, T_CHIEF_ASK, T_CHIEF_LEAVE },
    { T_CAPTAIN_1, T_CAPTAIN_WARM, T_CAPTAIN_COLD,
      T_CAPTAIN_JOIN, T_CAPTAIN_GOOD, T_CAPTAIN_BAD, T_CAPTAIN_ASK, T_CAPTAIN_LEAVE },
    { T_TRADER_1,  T_TRADER_WARM,  T_TRADER_COLD,
      T_TRADER_JOIN, T_TRADER_GOOD, T_TRADER_BAD, T_TRADER_ASK, T_TRADER_LEAVE },
    { T_DOC_1,     T_DOC_WARM,     T_DOC_COLD,
      T_DOC_JOIN, T_DOC_GOOD, T_DOC_BAD, T_DOC_ASK, T_DOC_LEAVE },
    { T_DRIFTER_1, T_DRIFTER_WARM, T_DRIFTER_COLD,
      T_DRIFTER_JOIN, T_DRIFTER_GOOD, T_DRIFTER_BAD, T_DRIFTER_ASK, T_DRIFTER_LEAVE },
};

// Portraits are generated, so a character's face is just a seed -- but with
// only five recurring people, leaving their faces to chance is not worth it.
// Evenly spaced seeds and then a hash both produced three near-identical
// faces, because draw_portrait reads skin, cloth, hat, beard and scar from
// separate bit fields and nothing forces those to differ. These seeds were
// searched for: each yields a deliberately chosen combination, so the chief is
// dark and scarred under a dust wrap, the trader is bare-headed, the doctor
// wears violet, and so on. Distinctness is checked rather than hoped for.
static const uint32_t WHO_SEED[CHAR_COUNT] = { 109, 160, 456, 7, 559 };
static uint32_t who_seed(int who) { return WHO_SEED[who]; }

int ui_tab_rows(const GameState *gs, int tab) {
    switch (tab) {
    case TAB_MARKET:    return GOODS_COUNT;
    // One row, and only when there is something to press it for.
    case TAB_CONTRACTS: return gs->w.job.state == CONTRACT_OFFERED ? 1 : 0;
    case TAB_JOURNAL:   return 0;              // a record, not a menu
    case TAB_GARAGE:    return (gs->w.offer_upg < UPG_COUNT
                                || world_service_kind(&gs->w) != SVC_NONE) ? 1 : 0;
    // A hire offer OR a favour being asked. Only the hire was counted, so a
    // hand asking for something produced a tab whose Z and X were both live
    // and whose row count was zero -- the cursor could not land, and nothing
    // told the player the keys did anything.
    case TAB_CREW:      return (gs->w.offer_crew < CREW_COUNT
                                || gs->w.errand.state == ERR_OFFERED) ? 1 : 0;
    default:            return 0;
    }
}

// A tab is shown when it has something in it. Empty tabs are not drawn at all,
// so no placeholder UI ever reaches a player.
int ui_tab_live(const GameState *gs, int tab) {
    if (tab == TAB_MARKET) return 1;
    if (tab == TAB_CONTRACTS) return gs->w.job.state != CONTRACT_NONE;
    // The garage and the crew board only exist where there is something on
    // them, or where the convoy already carries something worth reviewing.
    if (tab == TAB_GARAGE) {
        if (gs->w.offer_upg < UPG_COUNT) return 1;
        // The works exists when it has a trade to do, even with nothing on the
        // forecourt. Without this the location vanishes exactly when its own
        // speciality is what you came for.
        if (world_service_kind(&gs->w) != SVC_NONE) return 1;
        for (int i = 0; i < UPG_COUNT; ++i) if (gs->w.upgrade[i]) return 1;
        return 0;
    }
    if (tab == TAB_CREW) {
        if (gs->w.offer_crew < CREW_COUNT) return 1;
        return world_crew_count(&gs->w) > 0;
    }
    // The journal is no longer a location. It is a personal record -- it has
    // never had a selectable row and you do not walk to it -- and it was
    // occupying one of five slots in a strip that now names places. It lives in
    // the right column instead, where it is visible at every stop rather than
    // one the player remembered to open.
    if (tab == TAB_JOURNAL) return 0;
    return 0;
}

// Everyone met so far, how often, and where you stand with them. The regard
// line is the point: it tells the player that the choices they made at an
// encounter are still being carried around.
// An errand, and the reason this function exists at all.
//
// Z and X have been bound to accepting and declining an errand since v5
// (game.c), and nothing has ever drawn a prompt for either. Worse,
// ui_tab_rows(TAB_CREW) returned a row only for a HIRE offer, so a hand asking
// a favour produced a tab with two live keys and no visible control -- the same
// fault as the trend legend whose strings existed and were never drawn, and the
// six T_ERR_* strings below had been sitting unreferenced since the day they
// were written.
//
// A binding a player cannot see is not a mechanic. It is a secret.
static int draw_errand(Framebuffer *fb, const World *w, int x, int y, int pw) {
    const Errand *e = &w->errand;
    if (e->state == ERR_NONE || e->state == ERR_DONE) return y;
    (void)pw;

    int who = CHAR_OF_ROLE[e->who];
    int ly  = y;

    // What is being asked, in their words.
    int hx = x + 14;
    hx += draw_text(fb, hx, ly, WHO_NAME[who], 1, PALETTE[C_BONE]) + 6;
    draw_text(fb, hx, ly, e->state == ERR_OFFERED ? T_ERR_ASKS : T_ERR_DOING, 1,
              PALETTE[e->state == ERR_OFFERED ? C_WARN : C_DIM]);
    ly += 16;

    // The terms. A visit costs route, a carry costs hold space -- and the
    // difference is the whole reason both kinds exist, so they do not share
    // a sentence.
    int tx = x + 14;
    if (e->state == ERR_VISIT || (e->state == ERR_OFFERED && e->qty == 0)) {
        tx += draw_text(fb, tx, ly, T_ERR_VISIT, 1, PALETTE[C_BONE]) + 6;
        tx += draw_text(fb, tx, ly, ARCH_NAME[e->arg], 1, PALETTE[C_GOOD]) + 10;
    } else {
        tx += draw_text(fb, tx, ly, T_ERR_CARRY, 1, PALETTE[C_BONE]) + 6;
        tx += draw_number(fb, tx, ly - 2, e->qty, 1, PALETTE[C_GOOD]) + 4;
        tx += draw_text(fb, tx, ly, GOOD_NAME[e->arg], 1, PALETTE[C_GOOD]) + 10;
    }
    tx += draw_text(fb, tx, ly, T_ERR_BY, 1, PALETTE[C_DIM]) + 6;
    draw_number(fb, tx, ly - 2, e->by_sector, 1, PALETTE[C_DIM]);
    ly += 18;

    // The controls. Only when there is something to press them for.
    if (e->state == ERR_OFFERED) {
        int kx = x + 14;
        kx += draw_key(fb, kx, ly - 4, G_KEY_Z, 2) + 4;
        kx += draw_text(fb, kx, ly, T_JOB_ACCEPT, 1, PALETTE[C_BONE]) + 14;
        kx += draw_key(fb, kx, ly - 4, G_X, 2) + 4;
        draw_text(fb, kx, ly, T_JOB_DECLINE, 1, PALETTE[C_DIM]);
    } else if (w->warned) {
        // The desertion warning was a flag nobody could read.
        int qx = x + 14;
        qx += draw_text(fb, qx, ly, WHO_NAME[who], 1, PALETTE[C_BAD]) + 6;
        draw_text(fb, qx, ly, T_ERR_QUIT, 1, PALETTE[C_BAD]);
    }
    return ly + 26;
}

static void draw_journal(Framebuffer *fb, GameState *gs, int x, int y, int pw) {
    const World *w = &gs->w;
    (void)pw;
    int any = 0, ly = y + 8;

    for (int i = 0; i < CHAR_COUNT; ++i) {
        if (!w->met[i]) continue;
        any = 1;
        draw_portrait(fb, x + 14, ly, 2, who_seed(i),
                      w->regard[i] > 0 ? 1 : (w->regard[i] < 0 ? -1 : 0));
        draw_text(fb, x + 56, ly + 4, WHO_NAME[i], 1, PALETTE[C_BONE]);

        int rx = x + 56;
        rx += draw_number(fb, rx, ly + 20, w->met[i], 1, PALETTE[C_DIM]) + 6;
        draw_text(fb, rx, ly + 20, w->met[i] == 1 ? T_TIME : T_TIMES, 1,
                  PALETTE[C_DIM]);

        const char *r = w->regard[i] > 0 ? T_REGARD_GOOD
                      : w->regard[i] < 0 ? T_REGARD_BAD : T_REGARD_NEUT;
        draw_text(fb, x + 190, ly + 12, r, 1,
                  PALETTE[w->regard[i] > 0 ? C_GOOD
                        : w->regard[i] < 0 ? C_BAD : C_DIM]);
        ly += 40;
    }
    if (!any) draw_text(fb, x + 14, y + 12, T_NOBODY_YET, 1, PALETTE[C_DIM]);
}

static int tab_count_live(const GameState *gs) {
    int n = 0;
    for (int t = 0; t < TAB_COUNT; ++t) n += ui_tab_live(gs, t);
    return n;
}

// Draws the tab strip and returns the height it consumed.
static int draw_tabs(Framebuffer *fb, const GameState *gs, int x, int y, int pw) {
    if (tab_count_live(gs) < 2) return 0;
    int tx = x + 10;
    for (int t = 0; t < TAB_COUNT; ++t) {
        if (!ui_tab_live(gs, t)) continue;
        int tw = text_w(tab_name(gs, t), 1) + 16;
        int on = (t == gs->tab);
        fill_rect(fb, tx, y, tw, 20, PALETTE[on ? C_BORDER : C_INK]);
        if (on) draw_rect(fb, tx, y, tw, 20, PALETTE[C_BONE]);
        draw_text(fb, tx + 8, y + 6, tab_name(gs, t), 1,
                  PALETTE[on ? C_BONE : C_DIM]);
        tx += tw + 4;
    }
    // Which keys move between them.
    draw_text(fb, x + pw - 60, y + 6, "< >", 1, PALETTE[C_DIM]);

    // How wide the strip actually got, measured rather than promised.
    //
    // The locations are named per town and a later phase adds a fifth for the
    // situation, so the worst case is not something anyone can eyeball from the
    // string table -- it depends on which archetype and which condition a seed
    // rolled. The harness reads this and fails the sweep if any frame runs the
    // strip into the "< >" hint at x+pw-60, which is the collision that would
    // otherwise be found by a person looking at the one screenshot in a
    // thousand where a QUARANTINE happened to sit next to a PUMP HOUSE.
    UI_PROBE(tx - x, pw - 60);
    return 26;
}

// Shared layout for the two outfitting boards: what is for sale, what it does,
// what it costs, and what is already aboard.
// Returns the y at which the owned list ended, so callers can draw beneath it
// rather than through it: the garage's payback and road-ahead lines were fixed
// at y+92 and y+118 while this list runs from y+86 at a 15px pitch, so
// "FITTED TUNED ENGINE" was printed straight over "SHOULD RETURN 60".
static int draw_outfit(Framebuffer *fb, GameState *gs, int x, int y, int pw,
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
    //
    // Bounded, because this list is the growing thing that everything else in
    // the column has had to be clamped away from. With a full crew and a
    // favour on the table it ran through the depart row -- two frames in seven
    // hundred and fifty seeds, which is a margin no screenshot was ever going
    // to catch. Bounding it here fixes the class rather than the instance:
    // whatever is drawn below is now safe by construction instead of by an
    // offset somebody has to keep correct.
    const int floor_y = 58 + 62 + 26 + GOODS_COUNT * 32 - 26;   // above DEPART
    int ly = y + 86, any = 0;
    for (int i = 0; i < count; ++i) {
        if (!owned[i]) continue;
        if (ly > floor_y) break;
        draw_text(fb, x + 18, ly, own_msg, 1, PALETTE[C_GOOD]);
        draw_text(fb, x + 18 + text_w(own_msg, 1) + 10, ly, names[i], 1,
                  PALETTE[C_BONE]);
        ly += 15;
        any = 1;
    }
    (void)any;
    return ly;
}

// Everything the garage knows that the base panel does not: the condition of
// what is on offer, what it should return, and what is still on the road.
static int draw_garage_extra(Framebuffer *fb, GameState *gs, int x, int y,
                              int pw, int below) {
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
        px += draw_text(fb, px, below, T_PAYS_BACK, 1, PALETTE[C_DIM]) + 8;
        draw_number(fb, px, below, world_upg_payback(w, w->offer_upg), 1,
                    PALETTE[C_WARN]);
    }

    // The road east moved to the right column -- see draw_road_ahead.
    //
    // It was the last block in this one, and the forecourt column simply had
    // more in it than it had height: fitted list, then the local trade, then
    // payback, then this. Every arrangement collided with something, because
    // there was no arrangement that fit. It is route information rather than
    // forecourt information, and it now sits with the other standing context
    // where there is room for it, which is a better home on the merits as well
    // as the only one with space.
    return below + 12;
}

// What the map already shows, counted. Right column, under the watchlist.
static void draw_road_ahead(Framebuffer *fb, const World *w, int x, int y, int cw) {
    int storms = 0, events = 0;
    world_road_ahead(w, &storms, &events);

    // What this town will do for you, if anything. Standing context, which is
    // what this whole column is.
    int k = world_service_kind(w);
    int h = 44 + (k != SVC_NONE ? 26 : 0);
    draw_panel(fb, x, y, cw, h);
    if (k != SVC_NONE) {
        draw_text(fb, x + 10, y + h - 20, SVC_DESC[k], 1, PALETTE[C_DIM]);
    }
    draw_text(fb, x + 10, y + 8, T_ROAD_AHEAD, 1, PALETTE[C_BONE]);
    int rx = x + 10;
    rx += draw_number(fb, rx, y + 24, storms, 1, PALETTE[C_WARN]) + 6;
    rx += draw_text(fb, rx, y + 24, T_AHEAD_STORMS, 1, PALETTE[C_DIM]) + 12;
    rx += draw_number(fb, rx, y + 24, events, 1, PALETTE[C_BAD]) + 6;
    draw_text(fb, rx, y + 24, T_AHEAD_EVENTS, 1, PALETTE[C_DIM]);
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
        kx += draw_text(fb, kx, ky, T_JOB_ACCEPT, 1, PALETTE[C_BONE]) + 20;
        // Refusing is a thing you can now do, so it has to be a thing you can
        // see. A binding with no prompt is a binding nobody presses.
        kx += draw_key(fb, kx, ky - 6, G_X, 2) + 8;
        draw_text(fb, kx, ky, T_JOB_DECLINE, 1, PALETTE[C_DIM]);
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

    // Crew, and what the next hop will cost in water.
    //
    // Two faults here. The figure was world_water_burn(w), the burn for the
    // day already paid; the hop about to be taken charges for w->day + 1, and
    // with crew aboard or tanks fitted those differ half the time. And the
    // whole readout was hidden unless crew were aboard, so a solo driver was
    // never told that water is spent per day at all -- while thirst is one of
    // the two things that end a run.
    if (world_crew_count(w) > 0) {
        x += draw_text(fb, x, 15, T_CREW_COUNT, 1, PALETTE[C_DIM]) + 6;
        x += draw_number(fb, x, 13, world_crew_count(w), 2, PALETTE[C_BONE]) + 8;
    }
    draw_glyph(fb, x, 13, G_MINUS, 2, PALETTE[C_BAD]); x += glyph_w(2);
    draw_icon(fb, x, 9, ICON_WATER, 1); x += 18;
    x += draw_number(fb, x, 13, world_water_burn_on(w, w->day + 1), 2, PALETTE[C_BAD]);

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


// The watchlist: everyone met, and what standing with them is *for*.
//
// The journal has shown "REGARD GOOD" since v3, which is a stat. What a player
// needs is the plan -- whether this person would drive for them, and what it
// would take. Specified in v5 and not built; the right column is where it goes,
// because 180px of the trade screen has been empty since the game had one.
//
// Drawn every stop rather than behind a tab nobody opened.
#ifdef CONVOY_INSTRUMENT
int ui_watchlist_rows = 0;   // the harness checks this instead of tab reachability
#endif
static int draw_watchlist(Framebuffer *fb, const World *w, int x, int y, int cw) {
    int any = 0;
    for (int i = 0; i < CHAR_COUNT; ++i) if (w->met[i]) { any = 1; break; }
    if (!any) return y;

    // A panel behind it. Without one the column is drawn straight onto the
    // sky, and the standing lines -- which are coloured, and the point of the
    // whole block -- were unreadable against a sunset. Sized to the people
    // actually met, so it never leaves an empty box on screen.
    int n = 0;
    for (int i = 0; i < CHAR_COUNT; ++i) if (w->met[i]) ++n;
    draw_panel(fb, x, y - 8, cw, 28 + n * 32);

    draw_text(fb, x + 10, y, T_WATCH_TITLE, 1, PALETTE[C_DIM]);
    int ly = y + 16;

    for (int i = 0; i < CHAR_COUNT; ++i) {
        if (!w->met[i]) continue;
        int role   = ROLE_OF_CHAR[i];
        int aboard = (role >= 0) && w->crew[role];
        int r      = w->regard[i];

        draw_portrait(fb, x + 8, ly, 1, who_seed(i), r > 0 ? 1 : (r < 0 ? -1 : 0));
        draw_text(fb, x + 30, ly + 2, WHO_NAME[i], 1, PALETTE[C_BONE]);

        // The gate as a sentence. world_can_recruit owns the rule; this asks it
        // rather than restating it, because a second copy of a gate is how this
        // project has repeatedly ended up with two answers to one question.
        const char *line; int col;
        if (aboard)                            { line = T_WATCH_ABOARD; col = C_GOOD; }
        else if (world_can_recruit(w, i))      { line = T_WATCH_READY;  col = C_GOOD; }
        else if (world_char_is_enemy(i) && r < 1) { line = T_WATCH_ENEMY; col = C_WARN; }
        else if (r < 0)                        { line = T_WATCH_COLD;   col = C_BAD;  }
        else                                   { line = T_WATCH_CLOSE;  col = C_WARN; }
        draw_text(fb, x + 30, ly + 14, line, 1, PALETTE[col]);

        ly += 32;
        INSTR_UI(ui_watchlist_rows++);
        if (ly > y + cw + 180) break;   // never run off the panel
    }
    return ly + 4;
}

// ---------------------------------------------------------------- trade
void ui_trade(Framebuffer *fb, GameState *gs) {
    World *w = &gs->w;
    Node *nd = &w->node[w->sector][w->index];

    const int x = 26, y = 58, pw = 420, rowh = 32;
    // The right column. 180px wide starting at 452, empty since the game had a
    // trade screen at all.
    int rcol = draw_watchlist(fb, w, 452, y + 4, 180);
    draw_road_ahead(fb, w, 452, rcol + 8, 180);
    // One extra row below the goods for the depart action, so leaving the
    // market looks like another menu choice rather than a hidden key.
    // 64, not 50: the row block moved down 14px to give PRICE and HELD their
    // own line, and the depart row went with it. Left at 50 the panel ended
    // above its own last row and DEPART was drawn on the desert.
    draw_panel(fb, x, y, pw, 64 + (GOODS_COUNT + 1) * rowh);

    // The town's name at scale 2, its kind demoted to the line beneath. The
    // heading used to be the archetype, which meant every well in a run was
    // called WELL and there was nothing to remember any of them by.
    {
        int hx = x + 12;
        hx += draw_text(fb, hx, y + 10, TOWN_A[nd->name >> 4], 2,
                        PALETTE[C_BONE]) + 8;
        draw_text(fb, hx, y + 10, TOWN_B[nd->name & 15], 2, PALETTE[C_BONE]);
    }
    {
        int sx = x + 12;
        sx += draw_text(fb, sx, y + 30, ARCH_NAME[nd->archetype], 1,
                        PALETTE[C_WARN]) + 8;
        draw_text(fb, sx, y + 30, ARCH_DESC[nd->archetype], 1, PALETTE[C_DIM]);
    }

    int th = draw_tabs(fb, gs, x, y + 44, pw);

    // Arrival notices, right-aligned on the title line.
    //
    // Both used to be drawn at y+32, two pixels below the archetype line at
    // y+30 -- six pixels of ink each, so they overlapped it completely and
    // rendered as a red-on-grey mush. Nobody caught it in a screenshot because
    // it needs the right seed AND the right step: kit has to fail, or a job has
    // to pay out, on the frame you happened to photograph. The text-overlap
    // probe found 41 instances of it in a hundred seeds.
    //
    // They go on the title line because that is where the space actually is:
    // the town name ends around x+180 and the panel runs to x+420.
    {
        int ny = y + 14;
        if (w->kit_failed >= 0) {
            int tw = text_w(UPG_NAME[w->kit_failed], 1) + 8 + text_w(T_KIT_BROKE, 1);
            int px = x + pw - 12 - tw;
            px += draw_text(fb, px, ny, UPG_NAME[w->kit_failed], 1, PALETTE[C_BAD]) + 8;
            draw_text(fb, px, ny, T_KIT_BROKE, 1, PALETTE[C_BAD]);
            ny += 12;   // if both fire, they stack rather than share a line
        }
        if (w->job_paid > 0) {
            int tw = text_w(T_JOB_DONE, 1) + 8 + number_w(w->job_paid, 1);
            int px = x + pw - 12 - tw;
            px += draw_text(fb, px, ny, T_JOB_DONE, 1, PALETTE[C_GOOD]) + 8;
            draw_number(fb, px, ny, w->job_paid, 1, PALETTE[C_WARN]);
        }
    }

    if (gs->tab != TAB_MARKET) {
        if (gs->tab == TAB_CONTRACTS) draw_contracts(fb, gs, x, y + 44 + th, pw);
        else if (gs->tab == TAB_JOURNAL) draw_journal(fb, gs, x, y + 44 + th, pw);
        else if (gs->tab == TAB_GARAGE) {
            // The local trade goes FIRST, above the forecourt list.
            //
            // It is what this place does -- the reason the location is called
            // THE STILLS and not GARAGE -- so it reads as the headline rather
            // than a footnote. It is also the only position that cannot break:
            // everything else in this column grows (the fitted list by what is
            // aboard, the payback line by whether there is an offer), and a
            // fixed block underneath a growing one has to be clamped, which is
            // what drove it into the list, into DEPART, into the payback line
            // and into the road-ahead line in turn. Nothing grows above the
            // top of the panel.
            int gy = y + 44 + th;
            int k  = world_service_kind(&gs->w);
            if (k != SVC_NONE) {
                int kx = x + 14;
                kx += draw_key(fb, kx, gy, G_KEY_Z, 2) + 6;
                kx += draw_text(fb, kx, gy + 4, SVC_NAME[k], 1,
                                PALETTE[world_can_service(&gs->w) ? C_BONE : C_DIM]) + 10;
                draw_number(fb, kx, gy + 4, world_service_price(&gs->w), 1,
                            PALETTE[world_can_service(&gs->w) ? C_WARN : C_BAD]);
                gy += 28;
            }
            int below = draw_outfit(fb, gs, x, gy, pw, gs->w.offer_upg, UPG_COUNT,
                        UPG_NAME, UPG_DESC, gs->w.upgrade,
                        gs->w.offer_upg < UPG_COUNT
                            ? world_upg_price(&gs->w, gs->w.offer_upg,
                                              gs->w.offer_salvaged) : 0,
                        T_NO_GARAGE, T_BUY_UPGRADE, T_OWNED);
            draw_garage_extra(fb, gs, x, gy, pw, below + 6);
        }
        else if (gs->tab == TAB_CREW) {
            // Same shape as the works tab, for the same reason: the thing
            // being asked of you goes above the roster, because the roster
            // grows and a block clamped underneath a growing list ends up
            // inside it. This one collided with the crew names on exactly two
            // frames in six hundred seeds -- the kind of margin that is found
            // by a probe and not by a person.
            int cy2 = y + 44 + th;
            const Errand *er = &gs->w.errand;
            int has_err = (er->state != ERR_NONE && er->state != ERR_DONE);
            if (has_err) cy2 = draw_errand(fb, &gs->w, x, cy2, pw);

            int below = draw_outfit(fb, gs, x, cy2, pw, gs->w.offer_crew,
                        CREW_COUNT, CREW_NAME, CREW_DESC, gs->w.crew,
                        gs->w.offer_crew < CREW_COUNT
                            ? world_crew_price(&gs->w, gs->w.offer_crew) : 0,
                        T_NO_CREW, T_HIRE, T_HIRED);
            // The standing water cost, from the returned bottom. Shown only
            // when no favour is on the table: the warning explains what a hand
            // costs before you take one, and once somebody is aboard asking
            // something of you, that is what the screen is for.
            if (!has_err)
                draw_text(fb, x + 14, below + 12, T_CREW_WARN, 1, PALETTE[C_BAD]);
        }
        // Departing is always available, whatever tab is open.
        int dy = y + 62 + th + GOODS_COUNT * rowh;
        fill_rect(fb, x + 4, dy - 4, pw - 8, rowh - 2, PALETTE[C_INK]);
        int dx = x + 10;
        dx += draw_key(fb, dx, dy - 1, G_ENTER, 2) + 8;
        dx += draw_text(fb, dx, dy + 5, T_DEPART, 1, PALETTE[C_BONE]) + 10;
        draw_glyph(fb, dx, dy + 3, G_RIGHT, 2, PALETTE[C_DIM]);
        return;
    }

    // Column headings, so a bare number is never left to be guessed at.
    // Column headings sit above the first row, not up on the title line.
    //
    // They used to be at y+14, which was clear of the old heading because that
    // heading was the archetype -- SCRAPYARD, ten characters. A town name at
    // the same scale runs to fourteen (GLASS CROSSING) and drew straight
    // through PRICE. Anchored to the rows they label, which is where they
    // belonged anyway, and which cannot collide with a heading of any length.
    // The row block starts 14px lower than it used to, purely to give these
    // two their own line. They sat at y+14, beside the heading, which was
    // clear while the heading was an archetype -- SCRAPYARD, ten characters.
    // A town name runs to fourteen (GLASS CROSSING) and drew straight through
    // PRICE. Anchoring them to the rows they label instead put them under the
    // selected row's highlight, because there were only four pixels between
    // the tab strip and the first row. So the rows moved.
    int hy = y + 48 + th;
    draw_text(fb, x + 150, hy, T_PRICE, 1, PALETTE[C_DIM]);
    draw_text(fb, x + pw - 20 - text_w(T_HELD, 1), hy, T_HELD, 1, PALETTE[C_DIM]);

    for (int g = 0; g < GOODS_COUNT; ++g) {
        int ry = y + 62 + th + g * rowh;
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

        // What they still have, left of what you are carrying.
        //
        // Two quantities on one row needs the difference to be obvious at a
        // glance, so they do not share a treatment: theirs is a small depth
        // bar, yours is a number. A player reads "how much is left here"
        // positionally and "how much do I have" numerically, and never has to
        // work out which column is which.
        //
        // Sits between the name and the price, at x+72. The first attempt put
        // it at pw-104, reasoning that the selected row's inline BUY/SELL keys
        // ended around x+320 and the held number began near x+380. They do not:
        // the screenshot showed the pips drawn straight through SELL and the
        // sell price. The gap on the left is real and measurable -- the longest
        // good name is SCRAP at about 30px from x+34 -- so the bar goes where
        // nothing else has a claim, and the row now reads left to right as
        // name, what is left, price, trend, what you carry.
        {
            int st = nd->stock[g], sx = x + 72;
            if (st <= 0) {
                draw_text(fb, sx, ry + 5, T_SOLD_OUT, 1, PALETTE[C_BAD]);
            } else {
                // Six pips, one per two units, so a deep shelf and a thin one
                // are told apart without reading a number.
                for (int i = 0; i < 6; ++i) {
                    int on = st > i * 2;
                    fill_rect(fb, sx + i * 6, ry + 4, 4, 10,
                              PALETTE[on ? (st <= 2 ? C_WARN : C_GOOD) : C_BORDER]);
                }
            }
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
        int dy = y + 62 + th + GOODS_COUNT * rowh;
        fill_rect(fb, x + 4, dy - 4, pw - 8, rowh - 2, PALETTE[C_INK]);
        int dx = x + 10;
        dx += draw_key(fb, dx, dy - 1, G_ENTER, 2) + 8;
        dx += draw_text(fb, dx, dy + 5, T_DEPART, 1, PALETTE[C_BONE]) + 10;
        draw_glyph(fb, dx, dy + 3, G_RIGHT, 2, PALETTE[C_DIM]);
    }

    // What the selected good is actually for -- the single most useful line
    // on the screen for a player who has never seen it before.
    {
        int ty = y + 72 + th + (GOODS_COUNT + 1) * rowh;
        int tw = text_w(GOOD_USE[gs->sel], 1);
        // The trend arrow is the signal the whole trade route is built from,
        // and it was drawn bare -- no legend anywhere outside the help screen.
        // Both strings for it existed and had never been drawn. Placed here,
        // on the line that already explains the selected good, rather than
        // beside the arrow where it sat on the row border.
        int bias = world_price_bias(w, gs->sel);
        const char *note = bias < 0 ? T_CHEAP_HERE : bias > 0 ? T_DEAR_HERE : 0;
        int nw = note ? text_w(note, 1) + 14 : 0;
        fill_scrim(fb, x + 6, ty - 4, tw + nw + 16, 18, PALETTE[C_INK], 13);
        draw_text(fb, x + 14, ty, GOOD_USE[gs->sel], 1, PALETTE[C_WARN]);
        if (note)
            draw_text(fb, x + 14 + tw + 14, ty, note, 1,
                      PALETTE[bias < 0 ? C_GOOD : C_WARN]);
    }

    // Cargo hold below, the run's health bar.
    //
    // The seed goes first, and it goes here at all because until now it was
    // drawn nowhere in the game. The HUD counts it -- world_cargo includes the
    // payload -- while this grid iterated held[] and did not, so the two never
    // agreed, and the difference between them was precisely the six crates the
    // whole run is about. A player could reach the Green Zone having never
    // seen the thing they were carrying.
    //
    // The cell count is world_cargo_cap, not CARGO_CAP: with racks fitted the
    // grid drew thirty cells for a forty-slot hold and ten units of cargo
    // simply did not appear.
    const int cell = 20, cols = 15;
    int cap  = world_cargo_cap(w);
    int rows = (cap + cols - 1) / cols;
    int cy = y + 92 + th + (GOODS_COUNT + 1) * rowh;
    draw_panel(fb, x, cy, cols * cell + 12, rows * cell + 12);
    int slot = 0;
    for (int n = 0; n < world_payload(w) && slot < cap; ++n, ++slot) {
        int sx = x + 6 + (slot % cols) * cell;
        int sy = cy + 6 + (slot / cols) * cell;
        fill_rect(fb, sx + 1, sy + 1, cell - 2, cell - 2, PALETTE[C_GREEN]);
        draw_glyph(fb, sx + 6, sy + 5, G_PLUS, 2, PALETTE[C_BONE]);
    }
    for (int g = 0; g < GOODS_COUNT; ++g) {
        for (int n = 0; n < w->held[g] && slot < cap; ++n, ++slot) {
            int sx = x + 6 + (slot % cols) * cell;
            int sy = cy + 6 + (slot / cols) * cell;
            fill_rect(fb, sx + 1, sy + 1, cell - 2, cell - 2, PALETTE[C_INK]);
            draw_icon(fb, sx + 2, sy + 2, g, 1);
        }
    }
    for (; slot < cap; ++slot) {
        int sx = x + 6 + (slot % cols) * cell;
        int sy = cy + 6 + (slot / cols) * cell;
        draw_rect(fb, sx + 1, sy + 1, cell - 2, cell - 2, PALETTE[C_BORDER]);
    }

    // Name the green cells once, so they are not just an unexplained colour.
    if (world_payload(w) > 0) {
        int lx = x + 6;
        lx += draw_number(fb, lx, cy + rows * cell + 14, world_payload(w), 1,
                          PALETTE[C_GOOD]) + 6;
        draw_text(fb, lx, cy + rows * cell + 14, T_PAYLOAD_SAFE, 1, PALETTE[C_DIM]);
    }

    // Somebody aboard has something to say. Placed after the hold, in the
    // clear space under it -- the first attempt put the line four pixels below
    // the goods tooltip and the portrait on top of the water row.
    {
        const Errand *er = &w->errand;
        int say = -1, slot = LN_GOOD;
        if (er->state == ERR_OFFERED) { say = CHAR_OF_ROLE[er->who]; slot = LN_ASK; }
        else {
            for (int k = 0; k < CREW_COUNT; ++k) {
                if (!w->crew[k]) continue;
                int c = CHAR_OF_ROLE[k];
                // Whoever is unhappiest speaks; if all are content, the first
                // hand aboard does.
                if (say < 0 || w->regard[c] < w->regard[say]) say = c;
            }
            if (say >= 0)
                slot = (w->regard[say] <= -2) ? LN_LEAVE
                     : (w->regard[say] <  0)  ? LN_BAD : LN_GOOD;
        }
        if (say >= 0) {
            int vy = cy + rows * cell + 34;
            draw_portrait(fb, x + 6, vy - 6, 1, who_seed(say),
                          w->regard[say] > 0 ? 1 : (w->regard[say] < 0 ? -1 : 0));
            draw_text(fb, x + 30, vy, WHO_LINE[say][slot], 1,
                      PALETTE[(slot == LN_BAD || slot == LN_LEAVE) ? C_BAD : C_WARN]);
        }
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


void ui_event(Framebuffer *fb, GameState *gs) {
    World *w = &gs->w;
    const Event *e = &w->event;
    // Taller than it was: the third branch needs a block of its own, and the
    // panel had ~84px of slack below it in a 480-tall frame.
    const int x = 96, y = 62, pw = 448, ph = 396;

    int threat = world_event_is_threat(e->kind);
    uint32_t frame = threat ? PALETTE[C_BAD] : PALETTE[C_GOOD];

    draw_panel(fb, x, y, pw, ph);
    // Red for a threat, green for an offer. Kept to a hairline: at ten pixels
    // this was a saturated slab across the widest panel in the game, and the
    // palette is warm everywhere else, so it read as a rendering fault rather
    // than as a signal. Three pixels says the same thing and stays out of the
    // way of the words underneath.
    fill_rect(fb, x + 3, y + 3, pw - 6, 3, frame);

    // Name the situation. The icons show the price; only words can say why.
    draw_text_c(fb, x + pw / 2, y + 22, EV_TITLE[e->kind], 2, PALETTE[C_BONE]);

    // And give it a face. Which line you get depends on whether you have met
    // before and how you left it, so an encounter carries the history of the
    // run rather than resetting each time.
    int who = world_event_char(e->kind);
    if (who != CHAR_NONE) {
        draw_portrait(fb, x + 16, y + 44, 2, who_seed(who),
                      w->regard[who] > 0 ? 1 : (w->regard[who] < 0 ? -1 : 0));

        int slot = 0;                                    // first meeting
        if (w->met[who] > 1) slot = (w->regard[who] >= 0) ? 1 : 2;

        draw_text(fb, x + 58, y + 46, WHO_NAME[who], 1, PALETTE[C_WARN]);
        if (w->met[who] > 1)
            draw_text(fb, x + 58 + text_w(WHO_NAME[who], 1) + 12, y + 46,
                      T_MET_BEFORE, 1, PALETTE[C_DIM]);
        draw_text(fb, x + 58, y + 60, WHO_LINE[who][slot], 1, PALETTE[C_BONE]);
    }

    int affordable = world_can_accept(w);

    draw_text(fb, x + 20, y + 94, EV_ACCEPT[e->kind], 1,
              affordable ? PALETTE[C_BONE] : PALETTE[C_DIM]);
    if (!affordable) {
        // Two unrelated refusals used to share one message, so a full hold
        // read as an empty purse.
        int why = world_accept_block(&gs->w);
        draw_text(fb, x + 20 + text_w(EV_ACCEPT[e->kind], 1) + 12, y + 94,
                  why == 2 ? T_NO_ROOM : T_CANNOT, 1, PALETTE[C_BAD]);
    }

    // --- accept -------------------------------------------------------
    int ay = y + 108;
    fill_rect(fb, x + 10, ay - 6, pw - 20, 62, PALETTE[C_INK]);
    draw_rect(fb, x + 10, ay - 6, pw - 20, 62, affordable ? PALETTE[C_BONE] : PALETTE[C_DIM]);

    draw_key(fb, x + 20, ay + 6, G_KEY_Z, 2);
    int ax = x + 20 + key_w(2) + 18;
    if (e->pay_good >= 0 && e->pay_qty > 0) {
        ax += draw_stack(fb, ax, ay, -1, e->pay_good, e->pay_qty,
                         affordable ? PALETTE[C_BONE] : PALETTE[C_BAD]) + 24;
    } else if (e->pay_good >= 0) {
        // The right crew aboard makes an encounter free. That was drawn as
        // "- icon x0", which reads as a cost of nothing rather than as no
        // cost -- and it is the only on-screen evidence that a hire is
        // earning its keep.
        ax += draw_text(fb, ax, ay + 10, T_FREE, 1, PALETTE[C_GOOD]) + 24;
    }

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
    draw_text(fb, x + 20, y + 186, EV_DECLINE[e->kind], 1, PALETTE[C_DIM]);
    int by = y + 200;
    fill_rect(fb, x + 10, by - 6, pw - 20, 62, PALETTE[C_INK]);
    draw_rect(fb, x + 10, by - 6, pw - 20, 62, PALETTE[C_DIM]);

    draw_key(fb, x + 20, by + 6, G_X, 2);
    int bx = x + 20 + key_w(2) + 18;
    if (e->lose_qty > 0) {
        if (e->lose_good == -2) {
            // The seed itself. This is the highest-stakes branch in the game --
            // a crate is worth 500 score against a win's 1000 -- and it used to
            // render identically to losing a few units of random cargo,
            // because the test below only recognised a named good.
            int lx = bx;
            draw_glyph(fb, lx, by + 10, G_MINUS, 2, PALETTE[C_BAD]);
            lx += glyph_w(2) + 6;
            for (int i = 0; i < e->lose_qty && i < 6; ++i) {
                fill_rect(fb, lx, by + 4, 16, 16, PALETTE[C_GREEN]);
                draw_glyph(fb, lx + 4, by + 7, G_PLUS, 2, PALETTE[C_BONE]);
                lx += 20;
            }
            draw_text(fb, lx + 4, by + 10, T_PAYLOAD, 1, PALETTE[C_BAD]);
        } else if (e->lose_good >= 0) {
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

    // --- let them try it ------------------------------------------------
    // The only place in the game a face appears on *your* side of a deal.
    if (e->alt_who >= 0) {
        int can = world_can_attempt(w);
        int cy = y + 288;
        fill_rect(fb, x + 10, cy - 6, pw - 20, 74, PALETTE[C_INK]);
        draw_rect(fb, x + 10, cy - 6, pw - 20, 74,
                  can ? PALETTE[C_WARN] : PALETTE[C_DIM]);

        draw_portrait(fb, x + 18, cy - 2, 2, who_seed(e->alt_who >= 0 ? e->alt_who : 0), 1);

        int px = x + 58;
        draw_key(fb, px, cy + 4, G_ENTER, 2);
        px += key_w(2) + 12;
        px += draw_text(fb, px, cy + 10, ALT_VERB[e->alt_who], 1,
                        can ? PALETTE[C_BONE] : PALETTE[C_DIM]) + 16;

        // The odds, plainly. A gamble the player cannot price is not a choice.
        px += draw_number(fb, px, cy + 8, e->alt_odds, 2, PALETTE[C_WARN]) + 4;
        draw_glyph(fb, px, cy + 8, G_PCT, 2, PALETTE[C_WARN]);
        px += glyph_w(2) + 8;
        draw_text(fb, px, cy + 10, T_ALT_ODDS, 1, PALETTE[C_DIM]);

        if (e->alt_pay_good >= 0 && e->alt_pay_qty > 0)
            draw_stack(fb, x + 58, cy + 34, -1, e->alt_pay_good, e->alt_pay_qty,
                       PALETTE[C_DIM]);
        else
            draw_text(fb, x + 58, cy + 40, T_ALT_RISK, 1, PALETTE[C_DIM]);
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

    // ---- the two choices before a run starts ---------------------------
    static const char *const DIFF_NAME[DIFF_COUNT] = {
        T_DIFF_EASY, T_DIFF_NORMAL, T_DIFF_HARD
    };
    static const char *const DIFF_BLURB[DIFF_COUNT] = {
        T_DIFF_EASY_D, T_DIFF_NORM_D, T_DIFF_HARD_D
    };

    const int mx = cx - 210, mw = 420, my = fb->h - 150;
    fill_scrim(fb, mx, my - 10, mw, 96, PALETTE[C_INK], 12);
    draw_rect(fb, mx, my - 10, mw, 96, PALETTE[C_BORDER]);

    for (int row = 0; row < 2; ++row) {
        int ry  = my + row * 24;
        int on  = (gs->menu_row == row);
        const char *label = row ? T_M_MODE : T_M_DIFF;
        const char *value = row ? (gs->daily ? T_MODE_DAILY : T_MODE_STD)
                                : DIFF_NAME[gs->diff];

        // Only the selected row gets arrows, so it is obvious which one the
        // left and right keys are pointed at.
        if (on) {
            draw_glyph(fb, mx + 148, ry, G_MINUS, 1, PALETTE[C_WARN]);
            draw_glyph(fb, mx + 262, ry, G_PLUS,  1, PALETTE[C_WARN]);
        }
        draw_text(fb, mx + 16, ry, label, 1, PALETTE[on ? C_BONE : C_DIM]);
        draw_text(fb, mx + 164, ry, value, 1,
                  PALETTE[on ? C_WARN : C_DIM]);
    }

    // Which keys do what. T_M_ARROWS was defined and never drawn, so nothing
    // on screen said that up and down move between the two rows.
    draw_text_c(fb, cx, my + 44, T_M_ARROWS, 1, PALETTE[C_DIM]);

    // One line explaining whatever the cursor is currently on.
    draw_text_c(fb, cx, my + 56,
                gs->menu_row ? (gs->daily ? T_MODE_DAILY_D : T_MODE_STD_D)
                             : DIFF_BLURB[gs->diff],
                1, PALETTE[C_DIM]);

    // Prompts, pulsing so they read as things to press.
    const int py = fb->h - 44;
    if ((gs->tick / 24) & 1)
        draw_text_c(fb, cx, py, T_START, 2, PALETTE[C_BONE]);
    draw_text_c(fb, cx, py + 24, T_HELP_HINT, 1, PALETTE[C_WARN]);
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
    // Tint the scene toward triumph or toward dust. Kept light: at a quarter
    // coverage the green swallowed the sky, the sand and the convoy itself,
    // and the arrival read as a rendering fault rather than as arrival. The
    // Green Zone marker and the convoy parked beside it carry the moment; the
    // wash only has to agree with them.
    fill_scrim(fb, 0, 0, fb->w, fb->h,
               won ? PALETTE[C_GREEN] : PALETTE[C_INK], won ? 2 : 8);

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

    // One number for the whole run, and the seed that produced it. Together
    // these are what makes a daily run worth comparing: same map, same rules,
    // one figure to argue about.
    {
        int sx = cx + 40;
        draw_text(fb, sx, py + 40, T_SCORE, 1, PALETTE[C_DIM]);
        draw_number(fb, sx + text_w(T_SCORE, 1) + 8, py + 36,
                    world_score(w), 2, PALETTE[C_GOOD]);
        draw_text(fb, sx, py + 58, T_SEED, 1, PALETTE[C_DIM]);
        draw_number(fb, sx + text_w(T_SCORE, 1) + 8, py + 58,
                    (int)(w->seed % 100000u), 1, PALETTE[C_DIM]);
    }

    if ((gs->tick / 24) & 1)
        draw_text_c(fb, cx, py + 94, T_AGAIN, 1, PALETTE[C_BONE]);
}
