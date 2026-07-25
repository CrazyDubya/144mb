// convoy -- shared interface between the pure game core and each platform layer.
// The core touches no OS: it is handed memory, input, a framebuffer and an audio
// buffer, and does nothing else. That is what lets the same game.c build as a
// Windows exe, a native ARM64 binary, and a headless frame-dumping harness.
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define FB_W 640
#define FB_H 480

#define TICK_HZ    60           // fixed timestep; no dt anywhere, so runs are deterministic
#define AUDIO_HZ   44100

typedef struct {
    uint32_t *pixels;           // 0x00RRGGBB, FB_W * FB_H, top-down
    int w, h;
} Framebuffer;

typedef enum {
    BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
    BTN_A, BTN_B, BTN_START, BTN_HELP,
    BTN_COUNT
} Button;

typedef struct {
    uint8_t down[BTN_COUNT];    // held this tick
    uint8_t pressed[BTN_COUNT]; // went down this tick
} Input;

typedef struct {
    int16_t *samples;           // stereo interleaved
    int      frames;            // sample frames requested
} AudioBuffer;

// Platform allocates one block up front; the core sub-allocates and never
// calls malloc. Keeps the Windows build free of CRT heap machinery.
typedef struct {
    void    *permanent;
    uint32_t permanent_size;
    int      initialized;
} GameMemory;

void game_init  (GameMemory *mem, uint32_t seed);
void game_update(GameMemory *mem, const Input *in, Framebuffer *fb);
void game_audio (GameMemory *mem, AudioBuffer *ab);

#endif
