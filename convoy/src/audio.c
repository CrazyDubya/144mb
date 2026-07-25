#include "audio.h"
#include "game.h"   // AUDIO_HZ

// A minor pentatonic set, two octaves. Stored as tenths of a hertz so the
// phase increment can be derived with integer maths only.
static const uint16_t NOTE_DHZ[] = {
    550,  654,  734,  824,  980,       // A1 C2 D2 E2 G2
    1100, 1308, 1468, 1648, 1960,      // A2 C3 D3 E3 G3
    2200, 2616, 2936, 3296, 3920,      // A3 C4 D4 E4 G4
};
#define NOTE_COUNT ((int)(sizeof NOTE_DHZ / sizeof NOTE_DHZ[0]))

// A sparse 16-step figure. -1 rests. Sparseness is the point: this is meant to
// sit under the game for twenty minutes without wearing a hole in the player.
static const int8_t PATTERN[16] = {
    0, -1, -1, 4,  -1, 2, -1, -1,
    5, -1, 3, -1,  -1, -1, 1, -1
};

#define STEP_SAMPLES (AUDIO_HZ / 4)   // 4 steps a second

static uint32_t inc_for(uint16_t dhz) {
    // inc = f * 2^32 / rate, computed as (dhz * 2^32) / (rate * 10).
    return (uint32_t)(((uint64_t)dhz << 32) / ((uint64_t)AUDIO_HZ * 10));
}

void audio_init(AudioState *a, uint32_t seed) {
    for (int i = 0; i < (int)sizeof *a; ++i) ((uint8_t *)a)[i] = 0;
    a->rng     = seed ? seed : 0x1234567u;
    a->noise   = 0xACE1u;
    a->drone.inc = inc_for(NOTE_DHZ[0]);
    a->drone.env = 1 << 14;
    a->wind_amp  = 1 << 15;
}

void audio_tension(AudioState *a, int t) {
    a->tension = t < 0 ? 0 : (t > 255 ? 255 : t);
}

void audio_trigger(AudioState *a, int sfx) {
    // Each effect is a pitch and a decay rate; that is the entire sound design.
    static const uint16_t PITCH[SFX_COUNT] = {
        0, 1960, 1308, 880, 440, 330, 2616
    };
    static const int32_t DECAY[SFX_COUNT] = {
        0, 64400, 64400, 65100, 65300, 65450, 65200
    };
    if (sfx <= 0 || sfx >= SFX_COUNT) return;
    a->sfx.inc   = inc_for(PITCH[sfx]);
    a->sfx.env   = 1 << 16;
    a->sfx.decay = DECAY[sfx];
}

// 16.16 multiply, saturating nothing -- inputs are kept well inside range.
static inline int32_t mul16(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

static inline int32_t square(uint32_t phase) {
    return (phase & 0x80000000u) ? 32767 : -32768;
}
static inline int32_t saw(uint32_t phase) {
    return (int32_t)(phase >> 16) - 32768;
}

void audio_render(AudioState *a, int16_t *out, int frames) {
    for (int i = 0; i < frames; ++i) {
        // ---- sequencer -------------------------------------------------
        if ((a->sample % STEP_SAMPLES) == 0) {
            int8_t n = PATTERN[a->step & 15];
            if (n >= 0) {
                int oct = (a->tension > 128) ? 5 : 0;   // tension lifts the melody
                int idx = n + oct;
                if (idx >= NOTE_COUNT) idx = NOTE_COUNT - 1;
                a->lead.inc   = inc_for(NOTE_DHZ[idx]);
                a->lead.env   = 1 << 15;
                // ~0.2s ring. 65380 decays inside 50ms, which reads as a click
                // rather than a note.
                a->lead.decay = 65519;
            }
            a->step++;
        }
        a->sample++;

        // ---- voices ----------------------------------------------------
        a->drone.phase += a->drone.inc;
        int32_t mix = mul16(saw(a->drone.phase), a->drone.env) >> 2;

        a->lead.phase += a->lead.inc;
        mix += mul16(square(a->lead.phase), a->lead.env) >> 2;
        a->lead.env = mul16(a->lead.env, a->lead.decay);

        if (a->sfx.env > 64) {
            a->sfx.phase += a->sfx.inc;
            mix += mul16(square(a->sfx.phase), a->sfx.env) >> 2;
            a->sfx.env = mul16(a->sfx.env, a->sfx.decay);
        }

        // ---- wind ------------------------------------------------------
        // 16-bit LFSR through a one-pole lowpass: dust, not hiss.
        a->noise = (a->noise >> 1) ^ (uint32_t)(-(int32_t)(a->noise & 1u) & 0xB400u);
        int32_t n = (int32_t)(a->noise & 0xFFFF) - 32768;
        a->wind_lp += (n - a->wind_lp) >> 6;
        int32_t wind = mul16(a->wind_lp, a->wind_amp + (a->tension << 6)) >> 2;
        mix += wind;

        // Master gain. The voices are deliberately mixed low to leave room for
        // a stacked sfx hit; this brings the result up to a sane output level.
        mix *= 4;

        if (mix >  32767) mix =  32767;
        if (mix < -32768) mix = -32768;

        out[i * 2 + 0] = (int16_t)mix;
        out[i * 2 + 1] = (int16_t)mix;
    }
}
