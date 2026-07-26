// convoy -- game core: state machine and input.
//
// Every screen lives in ui.c; this file decides what happens and never draws.
// The core still touches no OS: it is handed memory, input, a framebuffer and
// an audio buffer, and does nothing else.
#include "game.h"
#include "state.h"
#include "ui.h"
#include "world.h"
#include "audio.h"
#include "cutscene.h"
#include "render.h"

// Today's date, arriving as a number. The platform layer is the only part of
// the program that knows what day it is; keeping it on that side means the
// core still makes no OS calls, and a daily run stays reproducible from its
// seed like any other.
void game_daily(GameMemory *mem, uint32_t seed) {
    GameState *gs = (GameState *)mem->permanent;
    gs->daily_seed = seed ? seed : 1u;
}

// Harness-only accessors. Compiled out of the Windows target entirely: they
// are never called there, but "never called" is not "not present" without
// link-time garbage collection, so all three were being carried in the
// contest binary as dead weight.
#ifdef CONVOY_INSTRUMENT

// Exposed so the headless harness can trace the simulation.
const World *game_world(GameMemory *mem) {
    return &((GameState *)mem->permanent)->w;
}

// Harness-only: which theme is playing, so mood switching can be verified.
// Harness-only: which theme is playing, so mood switching can be verified
// without listening to anything.
int audio_mood_of(GameMemory *mem) {
    return ((GameState *)mem->permanent)->audio.mood;
}

// The test bot drives the real UI rather than calling the simulation directly,
// so it has to see where the cursor is. Read-only, harness-only.
void game_ui(GameMemory *mem, int *sel, int *map_sel, int *tab, int *title) {
    GameState *gs = (GameState *)mem->permanent;
    if (sel)     *sel     = gs->sel;
    if (map_sel) *map_sel = gs->map_sel;
    if (tab)     *tab     = gs->tab;
    if (title)   *title   = gs->title;
}

#endif  // CONVOY_INSTRUMENT

// ---------------------------------------------------------------- helpers
static void restart(GameState *gs, uint32_t seed) {
    world_init(&gs->w, seed, gs->diff);
    gs->sel = 0;
    gs->map_sel = 0;
    gs->tab = TAB_MARKET;
    gs->trans = 0;
    gs->travel = 0;
    gs->from_sector = 0;
    gs->from_index = 0;
    for (int i = 0; i < VIG_COUNT; ++i) gs->vignette_seen[i] = 0;
    cutscene_begin(&gs->cut, &CS_OPENING, gs->tick);
}

void game_init(GameMemory *mem, uint32_t seed) {
    GameState *gs = (GameState *)mem->permanent;
    gs->tick  = 0;
    gs->seed  = seed ? seed : 1u;
    gs->title = 1;
    gs->help  = 0;
    gs->diff  = DIFF_NORMAL;
    gs->daily = 0;
    gs->menu_row = 0;
    // Until the platform layer says otherwise there is no date to work from,
    // so the daily run is simply the ordinary seed.
    gs->daily_seed = gs->seed;
    restart(gs, gs->seed);
    // restart() begins the opening cut scene, which is right when a run starts
    // and wrong here: the title has not been shown yet. Left running, the
    // opening played before the title and then again the moment the player
    // pressed start, and any key aimed at the title menu was swallowed by it.
    // The world is still built so the title has something behind it.
    gs->cut.running = 0;
    audio_init(&gs->audio, gs->seed);
    mem->initialized = 1;
}

// Tabs skip over any that currently have nothing in them, so the player never
// lands on an empty panel.
static void cycle_tab(GameState *gs, int dir) {
    for (int i = 0; i < TAB_COUNT; ++i) {
        gs->tab = (gs->tab + dir + TAB_COUNT) % TAB_COUNT;
        // Selectable-row count was the wrong test. The journal is a record
        // rather than a menu, so it has no rows and was skipped every time --
        // it could not be opened at all. The tab strip already knows which
        // tabs exist; ask it the same question it answers when drawing.
        if (ui_tab_live(gs, gs->tab)) break;
    }
    gs->sel = 0;
}

// ---------------------------------------------------------------- update
void game_update(GameMemory *mem, const Input *in, Framebuffer *fb) {
    GameState *gs = (GameState *)mem->permanent;
    World *w = &gs->w;
    gs->tick++;

    // A cut scene owns the screen while it runs. Any key advances it, Esc-like
    // skipping is handled by the platform layer quitting; the opening is three
    // panels and the endings are one.
    if (gs->cut.running) {
        int advance = 0;
        for (int i = 0; i < BTN_COUNT; ++i) if (in->pressed[i]) advance = 1;
        cutscene_update(&gs->cut, advance, gs->tick);
        cutscene_draw(fb, &gs->cut, gs->tick);
        return;
    }

    if (gs->help) {
        for (int i = 0; i < BTN_COUNT; ++i)
            if (in->pressed[i]) { gs->help = 0; break; }
        ui_help(fb, gs);
        return;
    }

    if (gs->title) {
        if (in->pressed[BTN_HELP]) {
            gs->help = 1;
        } else if (in->pressed[BTN_UP] || in->pressed[BTN_DOWN]) {
            gs->menu_row ^= 1;
        } else if (in->pressed[BTN_LEFT] || in->pressed[BTN_RIGHT]) {
            int d = in->pressed[BTN_RIGHT] ? +1 : -1;
            if (gs->menu_row == 0)
                gs->diff = (gs->diff + d + DIFF_COUNT) % DIFF_COUNT;
            else
                gs->daily ^= 1;
        } else if (in->pressed[BTN_START] || in->pressed[BTN_A]) {
            gs->title = 0;
            // Deterministic from the seed handed in at init, so a given seed
            // always produces the same run for testing. Replays from the end
            // screen vary instead. A daily run ignores both and takes the
            // date-derived seed, so everyone plays the same map today.
            restart(gs, gs->daily ? gs->daily_seed : gs->seed);
        }
        audio_mood(&gs->audio, MOOD_TITLE);
        ui_title(fb, gs);
        return;
    }

    // Help is reachable at any time, not just from the title.
    if (in->pressed[BTN_HELP]) { gs->help = 1; ui_help(fb, gs); return; }

    // Snapshot enough to tell whether an action actually did anything, so
    // sounds only fire on real state changes rather than on every keypress.
    int prev_state  = w->state;
    int prev_cargo  = world_cargo(w);
    int prev_credit = w->credits;
    int prev_sector = w->sector;
    int prev_index  = w->index;

    switch (w->state) {
    case ST_TRADE: {
        if (in->pressed[BTN_LEFT])  cycle_tab(gs, -1);
        if (in->pressed[BTN_RIGHT]) cycle_tab(gs, +1);

        int rows = ui_tab_rows(gs, gs->tab);
        if (rows > 0) {
            if (in->pressed[BTN_UP]   && gs->sel > 0)        gs->sel--;
            if (in->pressed[BTN_DOWN] && gs->sel < rows - 1) gs->sel++;
        }

        if (gs->tab == TAB_MARKET) {
            if (in->pressed[BTN_A]) world_buy(w, gs->sel);
            if (in->pressed[BTN_B]) world_sell(w, gs->sel);
        } else if (gs->tab == TAB_CONTRACTS) {
            if (in->pressed[BTN_A]) world_contract_accept(w);
            // X refuses, as it does on the market screen. The help screen has
            // advertised "X SELL / REFUSE" from the start, but the contracts
            // tab was one of three where X silently did nothing.
            if (in->pressed[BTN_B]) world_contract_decline(w);
        } else if (gs->tab == TAB_GARAGE) {
            if (in->pressed[BTN_A]) world_buy_upgrade(w);
        } else if (gs->tab == TAB_CREW) {
            if (in->pressed[BTN_A]) world_hire_crew(w);
        }
        if (in->pressed[BTN_START]) { w->state = ST_MAP; gs->map_sel = 0; }
        break;
    }

    case ST_MAP: {
        int cand[NODES_PER], n = world_reachable(w, cand);
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
        // Enter is unbound during an encounter, so the third branch needs no
        // cursor and no reshuffling of the two keys players already know.
        if (in->pressed[BTN_START]) world_attempt(w);
        break;

    case ST_DEAD:
    case ST_WON:
        if (in->pressed[BTN_START]) {
            // A daily run replays the same map. It used to take a fresh seed
            // regardless, so pressing "run it again" on today's map silently
            // gave a different one.
            restart(gs, gs->daily ? gs->daily_seed : gs->seed + gs->tick);
        }
        break;
    }

    // A hop starts the convoy driving the link it just took, and any change of
    // screen starts a wipe. Both are cosmetic and run on top of a simulation
    // that has already moved.
    if (w->sector != prev_sector) {
        gs->from_sector = (uint8_t)prev_sector;
        gs->from_index  = (uint8_t)prev_index;
        gs->travel = 26;
    }
    // Only the jarring transitions get a wipe. Running one on every state
    // change means a player spends a noticeable share of the run looking at a
    // dither pattern instead of the game.
    if (w->state != prev_state &&
        (w->state == ST_EVENT || prev_state == ST_EVENT)) gs->trans = 4;
    if (gs->trans  > 0) gs->trans--;
    if (gs->travel > 0) gs->travel--;

    // Arriving somewhere new can be worth a beat. Each kind fires at most once.
    if (w->sector != prev_sector && w->state != ST_DEAD && w->state != ST_WON) {
        int kind = -1;
        const Cutscene *v = cutscene_vignette(w, &kind);
        if (v && kind >= 0 && !gs->vignette_seen[kind]) {
            gs->vignette_seen[kind] = 1;
            cutscene_begin(&gs->cut, v, gs->tick);
            cutscene_draw(fb, &gs->cut, gs->tick);
            // Deliberately falls through to the arrival housekeeping below
            // rather than returning. Returning here skipped the tab reset and
            // the travel sound, so arriving through a vignette left the cursor
            // on whatever tab was last used and made no noise -- the two
            // things that tell a player they have arrived somewhere.
        }
    }

    // The run ending plays its scene once, then hands over to the summary.
    if ((w->state == ST_DEAD || w->state == ST_WON) && prev_state != w->state) {
        cutscene_begin(&gs->cut, &CS_ENDING[world_outcome(w)], gs->tick);
        cutscene_draw(fb, &gs->cut, gs->tick);
        return;
    }

    // Arriving somewhere new resets the cursor to the top of the market.
    if (w->sector != prev_sector) { gs->tab = TAB_MARKET; gs->sel = 0; }

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
        int tension = depth + thirst + dry;
        audio_tension(&gs->audio, tension);

        // The music follows what the player is doing, and gives way to the
        // tense theme when the convoy is genuinely in trouble rather than
        // merely far east.
        int mood;
        if (w->state == ST_DEAD || w->state == ST_WON) mood = MOOD_ENDING;
        else if (w->state == ST_EVENT)                 mood = MOOD_ENCOUNTER;
        else if (thirst + dry > 40)                    mood = MOOD_TENSE;
        else if (w->state == ST_TRADE)                 mood = MOOD_MARKET;
        else                                           mood = MOOD_ROAD;
        audio_mood(&gs->audio, mood);
    }

    // ------------------------------------------------------------ render
    if (w->state == ST_DEAD || w->state == ST_WON) {
        ui_end(fb, gs, w->state == ST_WON);
        return;
    }

    ui_backdrop(fb, gs);
    switch (w->state) {
    case ST_MAP:   ui_map(fb, gs);   break;
    case ST_TRADE: ui_trade(fb, gs); break;
    case ST_EVENT: ui_event(fb, gs); break;
    default: break;
    }
    ui_hud(fb, w);

    // The wipe sits over everything, including the HUD.
    // A brief dip rather than a full-screen dither sweep. The sweep was
    // striking in motion and awful in a still, and since it covered the whole
    // frame it was a large share of what a player actually looked at -- twice
    // it turned up in screenshots taken at random and made the game look
    // broken.
    // Peaks at about a third coverage for a tenth of a second. Any heavier and
    // it dominates the frame; any longer and it is what the player remembers.
    if (gs->trans > 0)
        fill_scrim(fb, 0, 0, fb->w, fb->h, PALETTE[C_INK], gs->trans);
}

void game_audio(GameMemory *mem, AudioBuffer *ab) {
    GameState *gs = (GameState *)mem->permanent;
    audio_render(&gs->audio, ab->samples, ab->frames);
}
