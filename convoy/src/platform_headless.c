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

const World *game_world(GameMemory *mem);
void         game_ui(GameMemory *mem, int *sel, int *map_sel, int *title);

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

    printf("wav: %s  %ds  peak=%ld  rms=%.0f  dc=%.1f  clipped=%ld\n",
           path, seconds, peak, rms, dc, clipped);
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
    }

    GameMemory mem = {0};
    mem.permanent_size = 16u << 20;
    mem.permanent = calloc(1, mem.permanent_size);

    uint32_t *pixels = (uint32_t *)calloc((size_t)FB_W * FB_H, sizeof(uint32_t));
    Framebuffer fb = { pixels, FB_W, FB_H };

    game_init(&mem, seed);

    if (wav_secs > 0) {
        char wpath[512];
        snprintf(wpath, sizeof wpath, "%s/audio.wav", outdir);
        if (!write_wav(wpath, &mem, wav_secs)) {
            fprintf(stderr, "convoy: could not write %s\n", wpath);
            return 1;
        }
    }

    Input in = {0};
    int dumped = 0;

    // ---- bot mode ----------------------------------------------------
    // The bot plays through the real UI: it presses the same keys a player
    // would, one per step, and never touches the simulation directly.
    if (bot_mode) {
        Bot bot;
        bot_init(&bot, bot_float);

        int steps = 0;
        const int LIMIT = 4000;      // generous; a run is ~60 decisions
        while (steps++ < LIMIT) {
            const World *w = game_world(&mem);
            if (w->state == ST_DEAD || w->state == ST_WON) break;

            int sel = 0, map_sel = 0, title = 0;
            game_ui(&mem, &sel, &map_sel, &title);
            int btn = bot_step(&bot, w, sel, map_sel, title);
            if (btn < 0) break;

            memset(&in, 0, sizeof in);
            in.down[btn] = in.pressed[btn] = 1;
            game_update(&mem, &in, &fb);
            memset(&in, 0, sizeof in);
            game_update(&mem, &in, &fb);

            if (verbose) trace(steps, '*', game_world(&mem));
            if (every > 0 && (steps % every) == 0) {
                char path[512];
                snprintf(path, sizeof path, "%s/bot_%04d.png", outdir, steps);
                write_png(path, pixels, FB_W, FB_H);
                ++dumped;
            }
        }

        const World *w = game_world(&mem);
        printf("BOT seed=%u %s sector=%d day=%d credits=%d cargo=%d steps=%d\n",
               seed,
               w->state == ST_WON ? "WON" : (w->state == ST_DEAD ? "DEAD" : "STALLED"),
               w->sector, w->day, w->credits, world_cargo(w), steps);
        return 0;
    }

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

    printf("convoy headless: %d ticks, %d frames written to %s/\n", ticks, dumped, outdir);
    return 0;
}
