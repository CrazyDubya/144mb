// convoy -- procedural audio. No samples anywhere: the whole soundtrack is a
// few oscillators and a step sequencer, which costs bytes in the hundreds
// rather than the hundreds of thousands.
//
// All integer maths -- no floats, no libm -- so the Windows build pulls in no
// maths runtime and the output is bit-identical across machines.
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

enum {
    SFX_NONE = 0,
    SFX_BUY, SFX_SELL, SFX_TRAVEL, SFX_HIT, SFX_DEATH, SFX_WIN,
    SFX_COUNT
};

typedef struct {
    uint32_t phase;
    uint32_t inc;
    int32_t  env;     // 16.16 amplitude envelope
    int32_t  decay;   // multiplied per sample block
} Voice;

typedef struct {
    uint32_t rng;
    uint32_t sample;      // running sample counter, drives the sequencer
    int      step;        // current sequencer step

    Voice    drone;
    Voice    lead;
    Voice    sfx;

    uint32_t noise;       // LFSR state for wind
    int32_t  wind_lp;     // one-pole lowpass memory
    int32_t  wind_amp;

    int      tension;     // 0..255, raises with danger; drives the mix
} AudioState;

void audio_init   (AudioState *a, uint32_t seed);
void audio_trigger(AudioState *a, int sfx);
void audio_tension(AudioState *a, int t);
void audio_render (AudioState *a, int16_t *out, int frames);

#endif
