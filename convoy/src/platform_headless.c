// convoy -- headless platform layer.
//
// This dev box is a headless aarch64 VM with no display and no way to run the
// Windows build, so this harness is the primary iteration loop: it runs the game
// core natively at a fixed timestep, feeds it scripted input, and writes frames
// out as PNG so they can actually be looked at.
//
// PNG is emitted with stored (uncompressed) deflate blocks -- a valid zlib stream
// with no compression -- so there is no libz dependency.
#include "game.h"
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>   // offsetof, for the determinism hash

// ---------------------------------------------------------------- png
static uint32_t crc_table[256];
static int      crc_ready = 0;

static void crc_init(void) {
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : (c >> 1);
        crc_table[n] = c;
    }
    crc_ready = 1;
}

static uint32_t crc32_buf(const uint8_t *buf, size_t len) {
    if (!crc_ready) crc_init();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static uint32_t adler32_buf(const uint8_t *buf, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; ++i) { a = (a + buf[i]) % 65521; b = (b + a) % 65521; }
    return (b << 16) | a;
}

static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static void write_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    uint8_t hdr[4];
    put_be32(hdr, len);
    fwrite(hdr, 1, 4, f);

    uint8_t *tmp = (uint8_t *)malloc(len + 4);
    memcpy(tmp, type, 4);
    if (len) memcpy(tmp + 4, data, len);
    fwrite(tmp, 1, len + 4, f);

    uint8_t crc[4];
    put_be32(crc, crc32_buf(tmp, len + 4));
    fwrite(crc, 1, 4, f);
    free(tmp);
}

static int write_png(const char *path, const uint32_t *pixels, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    fwrite(sig, 1, 8, f);

    uint8_t ihdr[13];
    put_be32(ihdr, (uint32_t)w);
    put_be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;   // bit depth
    ihdr[9] = 2;   // colour type: truecolour RGB
    ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    write_chunk(f, "IHDR", ihdr, 13);

    // Raw scanlines: one filter byte (0 = None) then RGB triples.
    size_t stride = (size_t)w * 3 + 1;
    size_t raw_len = stride * (size_t)h;
    uint8_t *raw = (uint8_t *)malloc(raw_len);
    for (int y = 0; y < h; ++y) {
        uint8_t *row = raw + (size_t)y * stride;
        *row++ = 0;
        const uint32_t *src = pixels + (size_t)y * w;
        for (int x = 0; x < w; ++x) {
            uint32_t p = src[x];
            *row++ = (uint8_t)(p >> 16);
            *row++ = (uint8_t)(p >> 8);
            *row++ = (uint8_t)p;
        }
    }

    // zlib stream: 2-byte header, stored deflate blocks, adler32 trailer.
    size_t nblocks = (raw_len + 65534) / 65535;
    size_t z_len   = 2 + nblocks * 5 + raw_len + 4;
    uint8_t *z = (uint8_t *)malloc(z_len);
    size_t zi = 0;
    z[zi++] = 0x78; z[zi++] = 0x01;
    size_t off = 0;
    while (off < raw_len) {
        size_t n = raw_len - off;
        if (n > 65535) n = 65535;
        int final = (off + n >= raw_len);
        z[zi++] = (uint8_t)(final ? 1 : 0);
        z[zi++] = (uint8_t)(n & 0xFF);
        z[zi++] = (uint8_t)(n >> 8);
        z[zi++] = (uint8_t)(~n & 0xFF);
        z[zi++] = (uint8_t)((~n >> 8) & 0xFF);
        memcpy(z + zi, raw + off, n);
        zi += n; off += n;
    }
    put_be32(z + zi, adler32_buf(raw, raw_len));
    zi += 4;

    write_chunk(f, "IDAT", z, (uint32_t)zi);
    write_chunk(f, "IEND", NULL, 0);

    free(z); free(raw);
    fclose(f);
    return 1;
}

#include "bot.h"
#include "bot_ref.h"
#include "state.h"   // TAB_* for the journal screenshot flag

const World *game_world(GameMemory *mem);
int          audio_mood_of(GameMemory *mem);
void         game_ui(GameMemory *mem, int *sel, int *map_sel, int *tab, int *title);

// Renders the synth to a WAV and reports level statistics. There is no way to
// listen to anything on this machine, so the check is numeric: non-silent,
// not clipped, and free of DC offset.
static int write_wav(const char *path, GameMemory *mem, int seconds) {
    int frames = AUDIO_HZ * seconds;
    int16_t *buf = (int16_t *)malloc((size_t)frames * 2 * sizeof(int16_t));
    if (!buf) return 0;

    // Render in tick-sized chunks, exactly as the platform layer does.
    int chunk = AUDIO_HZ / TICK_HZ, done = 0;
    while (done < frames) {
        int n = frames - done < chunk ? frames - done : chunk;
        AudioBuffer ab = { buf + (size_t)done * 2, n };
        game_audio(mem, &ab);
        done += n;
    }

    long peak = 0, clipped = 0; double sumsq = 0, sum = 0;
    for (int i = 0; i < frames * 2; ++i) {
        long v = buf[i], av = v < 0 ? -v : v;
        if (av > peak) peak = av;
        if (av >= 32767) ++clipped;
        sumsq += (double)v * (double)v;
        sum   += (double)v;
    }
    double rms = frames ? __builtin_sqrt(sumsq / (frames * 2)) : 0;
    double dc  = frames ? sum / (frames * 2) : 0;

    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); return 0; }
    uint32_t data_bytes = (uint32_t)frames * 2 * 2;
    uint32_t rate = AUDIO_HZ, byte_rate = rate * 4;
    uint16_t ch = 2, bits = 16, block = 4, fmt = 1;
    uint32_t riff = 36 + data_bytes, fmtsz = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); fwrite(&fmtsz, 4, 1, f);
    fwrite(&fmt, 2, 1, f);   fwrite(&ch, 2, 1, f);   fwrite(&rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f); fwrite(&block, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data_bytes, 4, 1, f);
    fwrite(buf, 1, data_bytes, f);
    fclose(f);

    printf("wav: %s  %ds  peak=%ld  rms=%.0f  dc=%.1f  clipped=%ld  mood=%d\n",
           path, seconds, peak, rms, dc, clipped, audio_mood_of(mem));
    free(buf);
    return 1;
}

static const char *STATE_NAME[] = { "MAP", "TRADE", "EVENT", "DEAD", "WON" };
static const char *DEATH_NAME[] = { "-", "THIRST", "STRANDED", "STRIPPED" };

static void trace(int step, char key, const World *w) {
    printf("%3d '%c'  %-5s sec=%d/%d day=%2d cr=%4d  W%-3d F%-3d A%-3d M%-3d S%-3d "
           "cargo=%2d  %s\n",
           step, key, STATE_NAME[w->state], w->sector, SECTORS - 1, w->day, w->credits,
           w->held[G_WATER], w->held[G_FUEL], w->held[G_AMMO],
           w->held[G_MEDS], w->held[G_SCRAP], world_cargo(w),
           w->state == ST_DEAD ? DEATH_NAME[w->death] : "");
}

// Maps a script character to a button. Each character is delivered as one
// discrete press (one tick held, one tick released) so pressed-edge logic in the
// game core behaves exactly as it would under a real key tap.
static int button_for(char c) {
    switch (c) {
    case 'u': return BTN_UP;
    case 'd': return BTN_DOWN;
    case 'l': return BTN_LEFT;
    case 'r': return BTN_RIGHT;
    case 'a': return BTN_A;
    case 'b': return BTN_B;
    case 's': return BTN_START;
    case 'h': return BTN_HELP;
    default:  return -1;   // '.' or anything else idles a tick
    }
}


// ---------------------------------------------------------------- hashing
// FNV-1a. Used to answer two questions the eye cannot: did this seed generate
// the same route as before, and did this run replay identically.
static uint32_t fnv(const void *p, size_t n, uint32_t h) {
    const uint8_t *b = (const uint8_t *)p;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 16777619u; }
    return h;
}

#ifdef CONVOY_INSTRUMENT
#define WORLD_CORE offsetof(World, in)
#else
#define WORLD_CORE sizeof(World)
#endif

// The route as generated -- layout, links and archetypes, but not prices,
// which move as soon as anyone trades. This is the number that proves the
// point of splitting the generator into separate streams: edit an encounter
// table and every map hash must stay exactly where it was.
static uint32_t map_hash(const World *w) {
    uint32_t h = 2166136261u;
    for (int s = 0; s < SECTORS; ++s)
        for (int n = 0; n < NODES_PER; ++n) {
            const Node *nd = &w->node[s][n];
            uint8_t f[4] = { nd->active, nd->type, nd->links, nd->archetype };
            h = fnv(f, sizeof f, h);
        }
    return h;
}

// Everything the simulation actually is, excluding the measurement block --
// counters are a product of the run, not part of it, and hashing them would
// make the determinism check partly tautological.
static uint32_t state_hash(const World *w) {
    return fnv(w, WORLD_CORE, 2166136261u);
}

// ---------------------------------------------------------------- exploit
// Can the convoy buy a unit and immediately sell it back for a profit?
//
// This shipped once already: buying nudged the local price up and you could
// sell into your own nudge, and a bot found 4,141 credits at sector 0 on day
// one. A 20% bid-ask spread fixed it, but the margin is thinner than it looks
// -- with the trader aboard the spread narrows to 10%, and the round trip
// comes to 0.9*(p + p/16 + 1) - p, which is positive below about p=21 in
// exact arithmetic. Only integer truncation saves it, and water and scrap
// both sit inside that window.
//
// So this is not a historical check. It is the guard rail for any future
// change to either side of the spread, and it runs over the whole crew and
// upgrade cross-product because the trader's discount is what moves it.
static int exploit_probe(void) {
    int worst = -9999, worst_p = 0, worst_trader = 0;
    for (int trader = 0; trader < 2; ++trader) {
        for (int g = 0; g < GOODS_COUNT; ++g) {
            for (int p = 1; p <= 200; ++p) {
                World w;
                world_init(&w, 1234u, DIFF_NORMAL);
                w.crew[CREW_TRADER] = (uint8_t)trader;
                Node *nd = &w.node[w.sector][w.index];
                nd->type = NODE_SETTLE;
                nd->price[g] = (int16_t)p;

                // Buy one, then sell it straight back at the same stall.
                int before = w.credits = 10000;
                int held   = w.held[g];
                world_buy(&w, g);
                if (w.held[g] != held + 1) continue;   // could not buy; not a trade
                world_sell(&w, g);

                int net = w.credits - before;
                if (net > worst) { worst = net; worst_p = p; worst_trader = trader; }
            }
        }
    }
    // Strictly positive is the failure. Break-even happens at a list price of
    // 1, where the sell clamp floors the take at 1 credit -- you end with the
    // same credits and the same goods, and the buy nudges the price up so the
    // next cycle loses. Nothing is farmed, so nothing is wrong.
    if (worst > 0) {
        fprintf(stderr, "EXPLOIT: round trip nets %+d at price %d%s\n",
                worst, worst_p, worst_trader ? " with the trader aboard" : "");
        return 1;
    }
    printf("EXPLOIT clean: best round trip nets %+d (price %d%s)\n",
           worst, worst_p, worst_trader ? ", trader aboard" : "");
    return 0;
}

// ---------------------------------------------------------------- one run
typedef struct {
    int   bot_float, refuse_all, journal_at, end_shot, every, verbose;
    const char *outdir;
    int   trace_hash;      // -Z: accumulate a hash of every step
    int   use_ref;         // -A ref: play with the frozen agent instead
    int   daily;           // --daily: take today's fixed map, via the menu
    int   force_upg;       // -U n: fit upgrade n from the start, or -1
    int   feats;           // -M n: which human pressures the bot feels
    int   shot_tab;        // -S n: photograph the first frame showing tab n
} RunOpts;

typedef struct {
    uint32_t seed, map, trace;
    int      steps, journal_tab;
    int      shot_done;
    World    w;            // the whole thing, copied at the end
} RunResult;

// Plays one seed to completion. Extracted from main so a sweep can run many
// seeds in a single process: every documented sweep used to be a shell loop
// re-invoking the binary per seed, which is exactly how a sweep ends up
// straddling a rebuild and reporting half of one build and half of another.
static int run_one(GameMemory *mem, Framebuffer *fb, uint32_t *pixels,
                   uint32_t seed, int diff, const RunOpts *o, RunResult *res) {
    // A fresh arena per seed. game_init assigns individual fields and never
    // clears GameState, so without this the transition timer, cut-scene state
    // and audio phase would carry over from the previous run and the sweep
    // would quietly disagree with a shell loop.
    memset(mem->permanent, 0, sizeof(GameState));
    mem->initialized = 0;
    game_init(mem, seed);

    Input in = {0};
    memset(res, 0, sizeof *res);
    res->seed = seed;
    res->journal_tab = -1;
    res->trace = 2166136261u;

    // The daily run had no coverage at all: nothing in the harness ever called
    // game_daily or touched the title's second row, so both the date-derived
    // seed and the branch that chooses it were shipped untested.
    if (o->daily) {
        game_daily(mem, seed * 2654435761u);
        const int keys[] = { BTN_DOWN, BTN_RIGHT, BTN_UP };
        for (int k = 0; k < 3; ++k) {
            memset(&in, 0, sizeof in);
            in.down[keys[k]] = in.pressed[keys[k]] = 1;
            game_update(mem, &in, fb);
            memset(&in, 0, sizeof in);
            game_update(mem, &in, fb);
        }
    }

    // Pick the difficulty the way a player does, by pressing left or right on
    // the title menu. Setting the field directly would be shorter and would
    // also stop testing whether the menu works.
    for (int n = diff - DIFF_NORMAL; n != 0; n += (n > 0 ? -1 : 1)) {
        int btn = (n > 0) ? BTN_RIGHT : BTN_LEFT;
        memset(&in, 0, sizeof in);
        in.down[btn] = in.pressed[btn] = 1;
        game_update(mem, &in, fb);
        memset(&in, 0, sizeof in);
        game_update(mem, &in, fb);
    }

    // Two agents, one loop. The reference agent is the frozen v4-entry bot:
    // when the working bot is edited, running both over the same seeds says
    // whether a moved number came from the game or from the observer.
    Bot bot; BotRef ref;
    bot_init(&bot, o->bot_float);       bot.refuse_all = o->refuse_all;
    bot.feats = o->feats;
    botref_init(&ref, o->bot_float);    ref.refuse_all = o->refuse_all;

    int steps = 0;
    const int LIMIT = 4000;      // generous; a run is ~60 decisions
    while (steps++ < LIMIT) {
        const World *w = game_world(mem);
        if (w->state == ST_DEAD || w->state == ST_WON) break;

        int sel = 0, map_sel = 0, tab = 0, title = 0;
        game_ui(mem, &sel, &map_sel, &tab, &title);
        // Only once the title is gone. game_init builds a world for the title
        // to sit in front of, and pressing start builds the real one -- for an
        // ordinary run those are the same map, so hashing too early looked
        // correct and silently reported the wrong world for a daily run.
        if (!title && !res->map) {
            res->map = map_hash(w);
            // Forced-policy runs. Some questions cannot be asked by watching a
            // bot decide -- "is armour worth its price" needs runs that have it
            // and runs that do not, over the same seeds.
            //
            // Applied here, on the first frame after the title, and not before:
            // game_init builds a world for the title to sit in front of, and
            // pressing start builds the real one from scratch. Set any earlier
            // and world_init zeroes it, which is exactly what happened -- the
            // first armour A/B came back 107 crates against 107.
            if (o->force_upg >= 0 && o->force_upg < UPG_COUNT) {
                World *mw = (World *)(uintptr_t)w;   // harness only
                mw->upgrade[o->force_upg] = 1;
            }
        }
        int btn = o->use_ref ? botref_step(&ref, w, sel, map_sel, tab, title)
                             : bot_step(&bot, w, sel, map_sel, tab, title);
        if (btn < 0) break;

        // Checked here, between the bot's observation and the world's next
        // one, because that is the only point at which both have seen exactly
        // the same set of markets. world.h claimed for three releases that
        // "both reason from identical information"; they did not, because the
        // bot sampled once per keypress and the game once per arrival, so the
        // bot's average was weighted by how long it loitered in each shop.
        // Now that both sample on arrival they must agree exactly.
        if (o->trace_hash && !o->use_ref) {
            for (int g = 0; g < GOODS_COUNT; ++g) {
                if (bot.seen[g] == 0 || w->seen_n[g] == 0) continue;
                if (bot.seen[g] != w->seen_n[g]) {
                    fprintf(stderr, "sample counts differ, seed %u good %d: "
                            "bot %d vs world %d\n", seed, g, bot.seen[g], w->seen_n[g]);
                    return 0;
                }
                int ba = (int)(bot.sum[g] / bot.seen[g]);
                int wa = (int)(w->seen_sum[g] / w->seen_n[g]);
                if (ba != wa) {
                    fprintf(stderr, "price average disagrees, seed %u good %d: "
                            "bot %d vs world %d\n", seed, g, ba, wa);
                    return 0;
                }
            }
        }

        memset(&in, 0, sizeof in);
        in.down[btn] = in.pressed[btn] = 1;
        game_update(mem, &in, fb);
        memset(&in, 0, sizeof in);
        game_update(mem, &in, fb);

        if (o->trace_hash) {
            res->trace = fnv(&res->trace, 0, state_hash(game_world(mem)));
        }

        // Photograph a named tab the first time it is open. The journal shot
        // below was hardcoded to one tab at one step, which meant finding the
        // right step by hand for every screen -- and a panel that is only ever
        // looked at by luck is a panel whose overflow nobody notices.
        if (o->shot_tab >= 0 && !res->shot_done && w->state == ST_TRADE) {
            for (int k = 0; k < TAB_COUNT; ++k) {
                int t = 0;
                game_ui(mem, NULL, NULL, &t, NULL);
                if (t == o->shot_tab) break;
                memset(&in, 0, sizeof in);
                in.down[BTN_RIGHT] = in.pressed[BTN_RIGHT] = 1;
                game_update(mem, &in, fb);
                memset(&in, 0, sizeof in);
                game_update(mem, &in, fb);
            }
            int t = 0;
            game_ui(mem, NULL, NULL, &t, NULL);
            if (t == o->shot_tab) {
                char path[512];
                snprintf(path, sizeof path, "%s/tab%d.png", o->outdir, o->shot_tab);
                write_png(path, pixels, FB_W, FB_H);
                res->shot_done = 1;
            }
        }

        if (o->journal_at && steps == o->journal_at && w->state == ST_TRADE) {
            for (int k = 0; k < TAB_COUNT; ++k) {
                int t = 0;
                game_ui(mem, NULL, NULL, &t, NULL);
                if (t == TAB_JOURNAL) break;
                memset(&in, 0, sizeof in);
                in.down[BTN_RIGHT] = in.pressed[BTN_RIGHT] = 1;
                game_update(mem, &in, fb);
                memset(&in, 0, sizeof in);
                game_update(mem, &in, fb);
            }
            game_ui(mem, NULL, NULL, &res->journal_tab, NULL);
            char path[512];
            snprintf(path, sizeof path, "%s/journal.png", o->outdir);
            write_png(path, pixels, FB_W, FB_H);
        }

        if (o->verbose) trace(steps, '*', game_world(mem));
        if (o->every > 0 && (steps % o->every) == 0) {
            char path[512];
            snprintf(path, sizeof path, "%s/bot_%04d.png", o->outdir, steps);
            write_png(path, pixels, FB_W, FB_H);
        }
    }

    // The run is over but the ending cut scene still owns the screen, and the
    // bot stops before dismissing it. Press through to the summary, which is
    // the screen that actually reports the score.
    if (o->end_shot) {
        for (int k = 0; k < 240; ++k) {
            memset(&in, 0, sizeof in);
            if ((k % 30) == 0) in.down[BTN_A] = in.pressed[BTN_A] = 1;
            game_update(mem, &in, fb);
        }
        char path[512];
        snprintf(path, sizeof path, "%s/end.png", o->outdir);
        write_png(path, pixels, FB_W, FB_H);
    }

    res->steps = steps;
    res->w = *game_world(mem);

    // The menu selection only reaches the World when the run starts, so this
    // is the first point it can be checked -- and it is worth checking,
    // because a sweep that silently ran the default difficulty would produce
    // three identical tables and look like a balance result.
    if (res->w.diff != diff) {
        fprintf(stderr, "seed %u ran difficulty %d, wanted %d\n",
                seed, res->w.diff, diff);
        return 0;
    }
    return 1;
}

static const char *const OUT_NAME[] = {
    "DEAD", "EMPTY", "PARTIAL", "INTACT", "EXEMPLARY"
};

#ifdef CONVOY_INSTRUMENT
static const char *const EV_NAME[EV_KINDS] = {
    "RAID", "WRECK", "SICK", "BREAK", "TRADER", "TOLL", "CACHE",
    "BRIDGE", "RIVAL", "PLAGUE", "CHECKPOINT", "LEAK", "REFUGEE", "SIGNAL"
};

// Per-kind totals across a whole sweep. A single encounter kind fires about a
// quarter of a time per run, so its entire contribution sits far below the
// resolution of any feasible win-rate sample -- tuning one by watching the win
// rate produces a confident wrong answer. This is the instrument that can see
// them: how often each came up, how often it was taken, and how often it was
// refused because it could not be paid rather than because it was a bad deal.
typedef struct { long fired, acc, forced; } KindTotals;

static void kind_add(KindTotals *t, const World *w) {
    for (int k = 0; k < EV_KINDS; ++k) {
        t[k].fired  += w->in.ev_fired[k];
        t[k].acc    += w->in.ev_accepted[k];
        t[k].forced += w->in.ev_forced[k];
    }
}

static void kind_report(const KindTotals *t, int runs) {
    printf("KIND  %-11s %8s %8s %8s %8s %8s\n",
           "encounter", "fired", "per_run", "accept%", "refuse%", "forced%");
    for (int k = 0; k < EV_KINDS; ++k) {
        long f = t[k].fired;
        if (f == 0) { printf("KIND  %-11s %8d %8s %8s %8s %8s\n",
                             EV_NAME[k], 0, "-", "-", "-", "-"); continue; }
        long refused = f - t[k].acc;
        printf("KIND  %-11s %8ld %8.2f %7ld%% %7ld%% %7ld%%\n",
               EV_NAME[k], f, (double)f / (runs ? runs : 1),
               t[k].acc * 100 / f, refused * 100 / f, t[k].forced * 100 / f);
    }
}
#endif

// One line per run, key=value so it can be parsed without counting columns.
// The previous positional format had grown to sixteen fields and every
// consumer -- the docs included -- was already out of date with it.
static void print_run(const RunResult *r) {
    const World *w = &r->w;
    int upg = 0, crew = 0;
    for (int i = 0; i < UPG_COUNT; ++i)  upg  += w->upgrade[i] ? 1 : 0;
    for (int i = 0; i < CREW_COUNT; ++i) crew += w->crew[i] ? 1 : 0;
    int met = 0, regard = 0;
    for (int i = 0; i < CHAR_COUNT; ++i) {
        met    += w->met[i] ? 1 : 0;
        regard += w->regard[i];
    }
    printf("BOT seed=%u result=%s death=%s sector=%d day=%d credits=%d "
           "cargo=%d seed_left=%d outcome=%s upg=%d crew=%d met=%d regard=%d "
           "enc=%d diff=%d score=%d steps=%d map=%08x",
           r->seed,
           w->state == ST_WON ? "WON" : (w->state == ST_DEAD ? "DEAD" : "STALLED"),
           DEATH_NAME[w->death],
           w->sector, w->day, w->credits, world_cargo(w), world_payload(w),
           OUT_NAME[world_outcome(w)], upg, crew, met, regard,
           w->encounters, w->diff, world_score(w), r->steps, r->map);
#ifdef CONVOY_INSTRUMENT
    const Metrics *m = &w->in;
    long ev_fired = 0, ev_acc = 0, ev_forced = 0;
    for (int k = 0; k < EV_KINDS; ++k) {
        ev_fired  += m->ev_fired[k];
        ev_acc    += m->ev_accepted[k];
        ev_forced += m->ev_forced[k];
    }
    printf(" acc=%ld forced=%ld c_off=%u c_acc=%u c_done=%u c_dec=%u c_lap=%u c_for=%u"
           " pl_storm=%u pl_dem=%u pl_rand=%u"
           " bought=%u sold=%u cr_in=%d cr_out=%d headline=%d stack=%u"
           " hold_mean=%d hold_peak=%u minw=%u minf=%u thin=%u",
           ev_acc, ev_forced, m->c_offered, m->c_accepted, m->c_completed,
           m->c_declined, m->c_lapsed, m->c_forfeit,
           m->pl_storm, m->pl_demand, m->pl_random,
           m->units_bought, m->units_sold, m->credits_in, m->credits_out,
           m->sold_headline, m->biggest_stack,
           m->cargo_samples ? (int)(m->cargo_sum * 100u / m->cargo_samples) : 0,
           m->peak_cargo, m->min_water, m->min_fuel, m->days_thin);
    (void)ev_fired;
#endif
    printf("\n");
}

// ---------------------------------------------------------------- harness
int main(int argc, char **argv) {
    int      ticks    = 120;
    int      every    = 30;
    uint32_t seed     = 1;
    const char *outdir = "out";
    const char *script = NULL;
    int         verbose = 0;
    int         wav_secs = 0;
    int         skip_title = 0;
    int         bot_mode = 0;
    int         bot_float = 30;
    // Screenshot the journal at a given bot step. It is driven with real tab
    // presses rather than by reaching into GameState, so the shot also proves
    // the tab can actually be reached -- which for a while it could not.
    int         journal_at = 0;
    int         diff = DIFF_NORMAL;   // -D selects a difficulty for a sweep
    int         refuse_all = 0;       // -R makes the bot decline every encounter
    int         end_shot = 0;         // -E dumps the summary screen after the run
    int         sweep_n = 0;          // -N runs seeds 1..N in this one process
    int         determinism = 0;      // -Z replays each seed and compares
    int         exploit = 0;          // -X hunts a profitable market round trip
    int         use_ref = 0;          // -A ref plays with the frozen agent
    int         daily = 0;            // --daily plays today's fixed map
    int         force_upg = -1;       // -U n fits an upgrade regardless of choice
    int         feats = BOT_ALL;      // -M n selects which pressures the bot feels
    int         kinds = 0;            // -K reports per-encounter-kind behaviour
    int         shot_tab = -1;        // -S n photographs the first frame of tab n
    int         quick = 0;            // -Q shrinks the drawn area for sweeps

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-t") && i + 1 < argc) ticks = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-e") && i + 1 < argc) every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) outdir = argv[++i];
        else if (!strcmp(argv[i], "-i") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "-v")) verbose = 1;
        else if (!strcmp(argv[i], "-w") && i + 1 < argc) wav_secs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-T")) skip_title = 1;
        else if (!strcmp(argv[i], "-B")) bot_mode = 1;
        else if (!strcmp(argv[i], "-F") && i + 1 < argc) bot_float = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-J") && i + 1 < argc) journal_at = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-D") && i + 1 < argc) diff = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-R")) refuse_all = 1;
        else if (!strcmp(argv[i], "-E")) end_shot = 1;
        else if (!strcmp(argv[i], "-N") && i + 1 < argc) sweep_n = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-Z")) determinism = 1;
        else if (!strcmp(argv[i], "-X")) exploit = 1;
        else if (!strcmp(argv[i], "-A") && i + 1 < argc) use_ref = !strcmp(argv[++i], "ref");
        else if (!strcmp(argv[i], "--daily")) daily = 1;
        else if (!strcmp(argv[i], "-U") && i + 1 < argc) force_upg = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-M") && i + 1 < argc) feats = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-K")) kinds = 1;
        else if (!strcmp(argv[i], "-S") && i + 1 < argc) shot_tab = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-Q")) quick = 1;
    }

    GameMemory mem = {0};
    mem.permanent_size = 16u << 20;
    mem.permanent = calloc(1, mem.permanent_size);

    uint32_t *pixels = (uint32_t *)calloc((size_t)FB_W * FB_H, sizeof(uint32_t));
    Framebuffer fb = { pixels, FB_W, FB_H };

    // A balance sweep never looks at a pixel, but the core draws a full
    // 640x480 frame twice per step regardless -- about 25 billion pixel writes
    // for a 400-seed arm, which is where nearly all the wall clock goes.
    //
    // -Q shrinks the *logical* size while keeping the full allocation, so
    // every primitive clips almost everything away and nothing can write out
    // of bounds. It is only sound because no game logic reads the framebuffer
    // dimensions: game.c touches fb->w exactly once, in a draw call. The
    // acceptance test is that a -Q sweep produces byte-identical BOT lines to
    // a full-size one -- which also proves the render path cannot influence a
    // balance number.
    if (quick && every <= 0 && shot_tab < 0 && !journal_at && !end_shot) {
        fb.w = 32; fb.h = 32;
    }

    if (exploit) return exploit_probe();

    RunOpts opt = { bot_float, refuse_all, journal_at, end_shot,
                    every, verbose, outdir, determinism, use_ref, daily,
                    force_upg, feats, shot_tab };

    // ---- bot mode ----------------------------------------------------
    // The bot plays through the real UI: it presses the same keys a player
    // would, one per step, and never touches the simulation directly.
    if (bot_mode || sweep_n > 0) {
        int lo = 1, hi = sweep_n;
        if (sweep_n <= 0) { lo = (int)seed; hi = (int)seed; }

        int won = 0, dead = 0, stalled = 0, runs = 0;
#ifdef CONVOY_INSTRUMENT
        KindTotals kt[EV_KINDS];
        memset(kt, 0, sizeof kt);
#endif
        for (int sd = lo; sd <= hi; ++sd) {
            RunResult r;
            if (!run_one(&mem, &fb, pixels, (uint32_t)sd, diff, &opt, &r)) return 2;

            if (determinism) {
                // Same seed, same everything: a second run must produce an
                // identical step-by-step hash. This catches uninitialised
                // reads and state leaking between seeds, neither of which a
                // win-rate sweep would ever show.
                RunResult again;
                if (!run_one(&mem, &fb, pixels, (uint32_t)sd, diff, &opt, &again)) return 2;
                if (again.trace != r.trace || again.map != r.map) {
                    fprintf(stderr, "NOT DETERMINISTIC: seed %d "
                            "trace %08x vs %08x, map %08x vs %08x\n",
                            sd, r.trace, again.trace, r.map, again.map);
                    return 3;
                }
            }

            if (journal_at && r.journal_tab != TAB_JOURNAL) {
                fprintf(stderr, "journal unreachable on seed %d: tab=%d wanted %d\n",
                        sd, r.journal_tab, TAB_JOURNAL);
                return 4;
            }

            print_run(&r);
            INSTR(kind_add(kt, &r.w));
            ++runs;
            if      (r.w.state == ST_WON)  ++won;
            else if (r.w.state == ST_DEAD) ++dead;
            else                           ++stalled;
        }

        if (sweep_n > 0) {
            printf("SWEEP n=%d diff=%d won=%d dead=%d stalled=%d win_pct=%d\n",
                   runs, diff, won, dead, stalled, runs ? won * 100 / runs : 0);
            if (kinds) INSTR(kind_report(kt, runs));
        }
        // A stall is always a bug, never a balance result, so it fails the run
        // rather than quietly appearing in a column.
        if (stalled) {
            fprintf(stderr, "%d run(s) stalled\n", stalled);
            return 5;
        }
        if (wav_secs > 0) {
            // Audio used to be unreachable in bot mode: the bot branch
            // returned before the wav path, so the only way to render sound
            // was a hand-written key script -- and a previous debugging
            // session was lost to exactly that blind spot.
            char path[512];
            snprintf(path, sizeof path, "%s/bot.wav", outdir);
            write_wav(path, &mem, wav_secs);
        }
        return 0;
    }

    game_init(&mem, seed);

    Input in = {0};
    int dumped = 0;

    // Dismiss the title screen so scripts describe the run itself.
    if (skip_title) {
        memset(&in, 0, sizeof in);
        in.down[BTN_START] = in.pressed[BTN_START] = 1;
        game_update(&mem, &in, &fb);
        memset(&in, 0, sizeof in);
        game_update(&mem, &in, &fb);
    }

    if (script) {
        int n = (int)strlen(script);
        for (int i = 0; i < n; ++i) {
            int btn = button_for(script[i]);

            memset(&in, 0, sizeof in);
            if (btn >= 0) { in.down[btn] = 1; in.pressed[btn] = 1; }
            game_update(&mem, &in, &fb);

            memset(&in, 0, sizeof in);          // release
            game_update(&mem, &in, &fb);

            if (verbose) trace(i, script[i], game_world(&mem));

            if (every > 0 && (i % every) == 0) {
                char path[512];
                snprintf(path, sizeof path, "%s/step_%04d.png", outdir, i);
                if (!write_png(path, pixels, FB_W, FB_H)) {
                    fprintf(stderr, "convoy: could not write %s\n", path);
                    return 1;
                }
                ++dumped;
            }
        }
        if (wav_secs > 0) {
            char wpath[512];
            snprintf(wpath, sizeof wpath, "%s/audio.wav", outdir);
            if (!write_wav(wpath, &mem, wav_secs)) return 1;
        }
        printf("convoy headless: %d scripted steps, %d frames written to %s/\n",
               n, dumped, outdir);
        return 0;
    }

    for (int t = 0; t < ticks; ++t) {
        memset(&in, 0, sizeof in);
        game_update(&mem, &in, &fb);

        if (every > 0 && (t % every) == 0) {
            char path[512];
            snprintf(path, sizeof path, "%s/frame_%04d.png", outdir, t);
            if (!write_png(path, pixels, FB_W, FB_H)) {
                fprintf(stderr, "convoy: could not write %s\n", path);
                return 1;
            }
            ++dumped;
        }
    }

    // Audio is rendered last so a script can drive the game into whatever
    // state is being listened for -- the title theme is not the only one worth
    // measuring.
    if (wav_secs > 0) {
        char wpath[512];
        snprintf(wpath, sizeof wpath, "%s/audio.wav", outdir);
        if (!write_wav(wpath, &mem, wav_secs)) {
            fprintf(stderr, "convoy: could not write %s\n", wpath);
            return 1;
        }
    }

    printf("convoy headless: %d ticks, %d frames written to %s/\n", ticks, dumped, outdir);
    return 0;
}
