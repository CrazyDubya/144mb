#include "audio.h"
#include "game.h"   // AUDIO_HZ

// Three chromatic octaves from A1, in tenths of a hertz so the phase increment
// can be derived with integer maths only.
static const uint16_t NOTE_DHZ[37] = {
      550,   583,   617,   654,   693,   734,   778,   824,
      873,   925,   980,  1038,  1100,  1165,  1235,  1308,
     1386,  1468,  1556,  1648,  1746,  1850,  1960,  2077,
     2200,  2331,  2469,  2616,  2772,  2937,  3111,  3296,
     3492,  3700,  3920,  4153,  4400,
};
#define NOTE_COUNT ((int)(sizeof NOTE_DHZ / sizeof NOTE_DHZ[0]))

// ---------------------------------------------------------------- instruments
enum { I_BASS, I_LEAD, I_PAD, I_DRUM, I_PLUCK, I_BELL, I_COUNT };

static const Instrument INSTRUMENT[I_COUNT] = {
    /*                wave        duty  atk   decay  sus    rel   vibD vibR vol */
    [I_BASS]  = { WAVE_SAW,     128,   900, 65500, 26000, 65100,  0,  0, 200 },
    [I_LEAD]  = { WAVE_SQUARE,   96,  1800, 65450, 20000, 65200, 12, 40, 170 },
    [I_PAD]   = { WAVE_TRI,     128,   240, 65530, 30000, 65400,  5, 18, 130 },
    [I_DRUM]  = { WAVE_NOISE,   128, 20000, 64200,     0, 64000,  0,  0, 210 },
    [I_PLUCK] = { WAVE_SQUARE,   64,  6000, 65200,  8000, 65000, 20, 55, 150 },
    [I_BELL]  = { WAVE_TRI,     128,  4000, 65350, 12000, 65250,  8, 28, 160 },
};

// ---------------------------------------------------------------- patterns
// -1 rests. Values index NOTE_DHZ, so 0 is A1 and 12 is A2.
#define R -1
static const Pattern P_BASS_SLOW = {
    { 0, R, R, R,  7, R, R, R,  5, R, R, R,  3, R, R, R }, I_BASS };
static const Pattern P_BASS_DRIVE = {
    { 0, R, 0, R,  7, R, 7, R,  5, R, 5, R,  3, R, 10, R }, I_BASS };
static const Pattern P_BASS_LOW = {
    { 0, R, R, R,  R, R, R, R,  3, R, R, R,  R, R, R, R }, I_BASS };

static const Pattern P_LEAD_OPEN = {
    { 24, R, R, 19,  R, 22, R, R,  20, R, 17, R,  R, R, 15, R }, I_LEAD };
static const Pattern P_LEAD_ROAD = {
    { 19, R, 22, R,  24, R, R, 22,  R, 19, R, 17,  R, R, R, R }, I_LEAD };
static const Pattern P_LEAD_TENSE = {
    { 25, 24, R, R,  25, R, 23, R,  R, 22, R, R,  20, R, R, R }, I_PLUCK };
static const Pattern P_LEAD_WARM = {
    { 22, R, 24, R,  27, R, 24, R,  22, R, 20, R,  19, R, R, R }, I_BELL };
static const Pattern P_PAD_HOLD = {
    { 12, R, R, R,  R, R, R, R,  15, R, R, R,  R, R, R, R }, I_PAD };
static const Pattern P_PAD_RISE = {
    { 12, R, R, R,  14, R, R, R,  15, R, R, R,  17, R, R, R }, I_PAD };

static const Pattern P_DRUM_SOFT = {
    { 12, R, R, R,  R, R, 12, R,  12, R, R, R,  R, R, R, R }, I_DRUM };
static const Pattern P_DRUM_DRIVE = {
    { 12, R, 8, R,  12, R, 8, R,  12, R, 8, R,  12, 8, 12, 8 }, I_DRUM };
static const Pattern P_DRUM_NONE = {
    { R, R, R, R,  R, R, R, R,  R, R, R, R,  R, R, R, R }, I_DRUM };
#undef R

// ---------------------------------------------------------------- songs
static const Song SONG[MOOD_COUNT] = {
    [MOOD_TITLE]     = { { &P_BASS_SLOW,  &P_LEAD_OPEN,  &P_PAD_HOLD, &P_DRUM_SOFT  }, 220 },
    [MOOD_ROAD]      = { { &P_BASS_DRIVE, &P_LEAD_ROAD,  &P_PAD_HOLD, &P_DRUM_DRIVE }, 300 },
    [MOOD_MARKET]    = { { &P_BASS_SLOW,  &P_LEAD_WARM,  &P_PAD_RISE, &P_DRUM_SOFT  }, 250 },
    [MOOD_TENSE]     = { { &P_BASS_DRIVE, &P_LEAD_TENSE, &P_PAD_RISE, &P_DRUM_DRIVE }, 340 },
    [MOOD_ENCOUNTER] = { { &P_BASS_LOW,   &P_LEAD_TENSE, &P_PAD_HOLD, &P_DRUM_NONE  }, 320 },
    [MOOD_ENDING]    = { { &P_BASS_LOW,   &P_LEAD_OPEN,  &P_PAD_RISE, &P_DRUM_NONE  }, 190 },
};

// ---------------------------------------------------------------- helpers
static uint32_t inc_for(uint16_t dhz) {
    return (uint32_t)(((uint64_t)dhz << 32) / ((uint64_t)AUDIO_HZ * 10));
}

static inline int32_t mul16(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

static void row_len_for(AudioState *a, int mood) {
    uint32_t rpm = SONG[mood].rows_per_min;
    if (!rpm) rpm = 240;
    a->row_len = (uint32_t)((uint64_t)AUDIO_HZ * 60 / rpm);
}

void audio_init(AudioState *a, uint32_t seed) {
    for (int i = 0; i < (int)sizeof *a; ++i) ((uint8_t *)a)[i] = 0;
    a->rng      = seed ? seed : 0x1234567u;
    a->noise    = 0xACE1u;
    a->wind_amp = 1 << 15;
    a->mood     = MOOD_TITLE;
    a->mood_next = MOOD_TITLE;
    a->fade     = 1 << 16;
    row_len_for(a, MOOD_TITLE);
}

void audio_tension(AudioState *a, int t) {
    a->tension = t < 0 ? 0 : (t > 255 ? 255 : t);
}

// A mood change dips the mix, swaps the song at the bottom of the dip, and
// brings it back. Cutting mid-phrase is the single most obvious way to make a
// soundtrack sound cheap.
void audio_mood(AudioState *a, int mood) {
    if (mood < 0 || mood >= MOOD_COUNT) return;
    if (mood == a->mood_next) return;
    a->mood_next = (uint8_t)mood;
}

void audio_trigger(AudioState *a, int sfx) {
    static const uint16_t PITCH[SFX_COUNT] = { 0, 1960, 1308, 880, 440, 330, 2616 };
    static const uint8_t  INST [SFX_COUNT] = { 0, I_PLUCK, I_PLUCK, I_DRUM,
                                               I_BASS, I_BASS, I_BELL };
    if (sfx <= 0 || sfx >= SFX_COUNT) return;
    a->sfx.inc   = inc_for(PITCH[sfx]);
    a->sfx.env   = 1 << 16;
    a->sfx.stage = 3;                 // straight to release: one-shots decay
    a->sfx.inst  = INST[sfx];
}

// ---------------------------------------------------------------- oscillators
static int32_t osc(uint8_t wave, uint32_t phase, uint8_t duty, uint32_t *noise) {
    switch (wave) {
    case WAVE_SAW:
        return (int32_t)(phase >> 16) - 32768;
    case WAVE_TRI: {
        int32_t t = (int32_t)(phase >> 15) & 0x1FFFF;   // 0..131071
        return (t < 65536 ? t : 131071 - t) - 32768;
    }
    case WAVE_NOISE:
        *noise = (*noise >> 1) ^ (uint32_t)(-(int32_t)(*noise & 1u) & 0xB400u);
        return (int32_t)(*noise & 0xFFFF) - 32768;
    default: {
        // Duty-compensated square. A plain +/-full square at anything other
        // than 50% duty has a DC offset proportional to how lopsided it is --
        // at 37% that measured -8520, which shows up as a thump on every note
        // and eats headroom that the music needs. Scaling each half by the
        // other's share puts the mean back at zero for any duty.
        uint32_t d = (uint32_t)duty << 24;
        int32_t hi = 32767 - (int32_t)duty * 128;
        int32_t lo = -(int32_t)duty * 128;
        return (phase < d) ? hi : lo;
    }
    }
}

// Attack ramps up, then the envelope decays toward the instrument's sustain
// level and holds; release takes over when the note is done.
static void env_step(Voice *v) {
    const Instrument *in = &INSTRUMENT[v->inst];
    switch (v->stage) {
    case 1:
        v->env += in->attack;
        if (v->env >= (1 << 16)) { v->env = 1 << 16; v->stage = 2; }
        break;
    case 2:
        if (v->env > in->sustain) {
            v->env = mul16(v->env, in->decay);
            if (v->env < in->sustain) v->env = in->sustain;
        }
        break;
    case 3:
        v->env = mul16(v->env, in->release);
        if (v->env < 64) { v->env = 0; v->stage = 0; }
        break;
    default:
        break;
    }
}

static int32_t voice_out(Voice *v, uint32_t *noise) {
    if (v->stage == 0 && v->env <= 0) return 0;
    const Instrument *in = &INSTRUMENT[v->inst];

    uint32_t inc = v->inc;
    if (in->vib_depth) {
        v->vib_phase += (uint32_t)in->vib_rate << 18;
        int32_t s = osc(WAVE_TRI, v->vib_phase, 128, noise);
        inc = (uint32_t)((int32_t)inc + (s / 256) * in->vib_depth / 64);
    }
    v->phase += inc;

    int32_t s = osc(in->wave, v->phase, in->duty, noise);
    s = mul16(s, v->env);
    return s * in->vol / 255;
}

static void note_on(Voice *v, int note, uint8_t inst) {
    if (note < 0) { v->stage = 3; return; }          // rest releases
    if (note >= NOTE_COUNT) note = NOTE_COUNT - 1;
    v->inst  = inst;
    v->inc   = inc_for(NOTE_DHZ[note]);
    v->env   = 0;
    v->stage = 1;
}

// ---------------------------------------------------------------- render
void audio_render(AudioState *a, int16_t *out, int frames) {
    for (int i = 0; i < frames; ++i) {

        // ---- sequencer -------------------------------------------------
        if (a->row_len && (a->sample % a->row_len) == 0) {
            // A pending mood change lands at the top of a phrase, which is
            // where a listener expects music to turn.
            if (a->mood_next != a->mood && (a->row & 7) == 0) {
                a->mood = a->mood_next;
                row_len_for(a, a->mood);
                a->row = 0;
            }
            const Song *sg = &SONG[a->mood];
            for (int c = 0; c < AUD_CHANNELS; ++c) {
                const Pattern *p = sg->ch[c];
                if (!p) continue;
                int n = p->note[a->row & (PATTERN_ROWS - 1)];
                if (n >= 0) note_on(&a->ch[c], n, p->inst);
            }
            a->row = (a->row + 1) & (PATTERN_ROWS - 1);
        }
        a->sample++;

        // Ease the mix toward full whenever no change is pending.
        int32_t want = (a->mood_next != a->mood) ? (12 << 12) : (1 << 16);
        if (a->fade < want) { a->fade += 48; if (a->fade > want) a->fade = want; }
        if (a->fade > want) { a->fade -= 48; if (a->fade < want) a->fade = want; }

        // ---- voices ----------------------------------------------------
        int32_t mix = 0;
        for (int c = 0; c < AUD_CHANNELS; ++c) {
            env_step(&a->ch[c]);
            mix += voice_out(&a->ch[c], &a->noise) >> 2;
        }
        mix = mul16(mix, a->fade);

        // Effects sit above the music and ignore the fade, so feedback stays
        // crisp through a mood change.
        env_step(&a->sfx);
        mix += voice_out(&a->sfx, &a->noise) >> 2;

        // ---- wind ------------------------------------------------------
        a->noise = (a->noise >> 1) ^ (uint32_t)(-(int32_t)(a->noise & 1u) & 0xB400u);
        int32_t n = (int32_t)(a->noise & 0xFFFF) - 32768;
        a->wind_lp += (n - a->wind_lp) >> 6;
        mix += mul16(a->wind_lp, a->wind_amp + (a->tension << 6)) >> 3;

        mix = mix * 2;
        if (mix >  32767) mix =  32767;
        if (mix < -32768) mix = -32768;

        out[i * 2 + 0] = (int16_t)mix;
        out[i * 2 + 1] = (int16_t)mix;
    }
}
