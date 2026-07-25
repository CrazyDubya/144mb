// convoy -- procedural audio.
//
// A four-channel pattern sequencer with ADSR instruments. No samples anywhere:
// the whole soundtrack is oscillators and tables, which costs bytes in the low
// thousands rather than the hundreds of thousands.
//
// All integer maths -- no floats, no libm -- so the Windows build pulls in no
// maths runtime and the output is bit-identical on every machine. That also
// means it can be verified numerically, which matters because nothing here can
// be listened to on the machine it is built on.
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

enum {
    SFX_NONE = 0,
    SFX_BUY, SFX_SELL, SFX_TRAVEL, SFX_HIT, SFX_DEATH, SFX_WIN,
    SFX_COUNT
};

// What the music is for. Switching crossfades rather than cuts.
enum {
    MOOD_TITLE,      // the road ahead, unhurried
    MOOD_ROAD,       // travelling: the default
    MOOD_MARKET,     // a settlement, warmer and busier
    MOOD_TENSE,      // low on water or fuel, deep east
    MOOD_ENCOUNTER,  // something on the road
    MOOD_ENDING,     // arrival or the lack of it
    MOOD_COUNT
};

#define AUD_CHANNELS 4
#define PATTERN_ROWS 16

enum { WAVE_SQUARE, WAVE_SAW, WAVE_TRI, WAVE_NOISE };

typedef struct {
    uint8_t  wave;
    uint8_t  duty;        // square only: 0..255 through the period
    uint16_t attack;      // 16.16 rise per sample
    uint16_t decay;       // 16.16 multiplier toward sustain
    uint16_t sustain;     // 16.16 level held while the note lasts
    uint16_t release;     // 16.16 multiplier once the note is over
    uint8_t  vib_depth;
    uint8_t  vib_rate;
    uint8_t  vol;         // 0..255 channel trim
} Instrument;

typedef struct {
    int8_t  note[PATTERN_ROWS];   // -1 rests, else an index into NOTE_DHZ
    uint8_t inst;
} Pattern;

typedef struct {
    const Pattern *ch[AUD_CHANNELS];
    uint16_t rows_per_min;        // tempo, in pattern rows per minute
} Song;

typedef struct {
    uint32_t phase, inc;
    uint32_t vib_phase;
    int32_t  env;         // 16.16
    uint8_t  stage;       // 0 idle, 1 attack, 2 sustain, 3 release
    uint8_t  inst;
} Voice;

typedef struct {
    uint32_t rng;
    uint32_t sample;
    int      row;                 // position in the pattern
    uint32_t row_len;             // samples per row, from the tempo

    Voice    ch[AUD_CHANNELS];
    Voice    sfx;

    uint32_t noise;               // LFSR, shared by wind and percussion
    int32_t  wind_lp;
    int32_t  wind_amp;

    uint8_t  mood, mood_next;
    int32_t  fade;                // 16.16, dips through a mood change
    int      tension;             // 0..255: thickens the wind
} AudioState;

void audio_init   (AudioState *a, uint32_t seed);
void audio_trigger(AudioState *a, int sfx);
void audio_tension(AudioState *a, int t);
void audio_mood   (AudioState *a, int mood);
void audio_render (AudioState *a, int16_t *out, int frames);

#endif
