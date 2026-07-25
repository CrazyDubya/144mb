// convoy -- Win32 platform layer. This is the submission target: a single
// standalone .exe with no runtime and no external dependencies, as the contest
// rules require. Window + framebuffer blit + keyboard + fixed timestep.
#include "game.h"
#include <windows.h>

static int      g_running = 1;
static uint32_t g_pixels[FB_W * FB_H];
static BITMAPINFO g_bmi;

// ---------------------------------------------------------------- audio
// One buffer per tick, a few in flight. If the machine has no audio device --
// which is true of CI runners and can be true of a judge's machine -- every
// call below is skipped and the game runs silently rather than failing to start.
#define AUD_FRAMES  (AUDIO_HZ / TICK_HZ)
#define AUD_BUFFERS 4

static HWAVEOUT g_wave;
static int      g_audio_ok;
static WAVEHDR  g_hdr[AUD_BUFFERS];
static int16_t  g_abuf[AUD_BUFFERS][AUD_FRAMES * 2];
static int      g_written[AUD_BUFFERS];

static void audio_open(void) {
    WAVEFORMATEX wf = {0};
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = 2;
    wf.nSamplesPerSec  = AUDIO_HZ;
    wf.wBitsPerSample  = 16;
    wf.nBlockAlign     = (WORD)(wf.nChannels * wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    if (waveOutGetNumDevs() == 0) return;
    if (waveOutOpen(&g_wave, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        return;

    for (int i = 0; i < AUD_BUFFERS; ++i) {
        g_hdr[i].lpData         = (LPSTR)g_abuf[i];
        g_hdr[i].dwBufferLength = sizeof g_abuf[i];
        if (waveOutPrepareHeader(g_wave, &g_hdr[i], sizeof g_hdr[i]) != MMSYSERR_NOERROR) {
            waveOutClose(g_wave);
            return;
        }
    }
    g_audio_ok = 1;
}

static void audio_pump(GameMemory *mem) {
    if (!g_audio_ok) return;
    for (int i = 0; i < AUD_BUFFERS; ++i) {
        if (g_written[i] && !(g_hdr[i].dwFlags & WHDR_DONE)) continue;
        AudioBuffer ab = { g_abuf[i], AUD_FRAMES };
        game_audio(mem, &ab);
        if (waveOutWrite(g_wave, &g_hdr[i], sizeof g_hdr[i]) == MMSYSERR_NOERROR)
            g_written[i] = 1;
        return;   // one buffer per tick is enough to stay ahead
    }
}

static void audio_close(void) {
    if (!g_audio_ok) return;
    waveOutReset(g_wave);
    for (int i = 0; i < AUD_BUFFERS; ++i)
        waveOutUnprepareHeader(g_wave, &g_hdr[i], sizeof g_hdr[i]);
    waveOutClose(g_wave);
    g_audio_ok = 0;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
    case WM_DESTROY:
        g_running = 0;
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) g_running = 0;
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
}

static void read_input(Input *in) {
    static uint8_t prev[BTN_COUNT];
    static const int vk[BTN_COUNT] = {
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, 'Z', 'X', VK_RETURN, 'H'
    };
    for (int i = 0; i < BTN_COUNT; ++i) {
        uint8_t d = (GetAsyncKeyState(vk[i]) & 0x8000) ? 1 : 0;
        in->down[i]    = d;
        in->pressed[i] = (uint8_t)(d && !prev[i]);
        prev[i] = d;
    }
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show) {
    (void)prev; (void)cmdline;

    WNDCLASSA wc = {0};
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = "convoy";
    if (!RegisterClassA(&wc)) return 1;

    RECT r = { 0, 0, FB_W, FB_H };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, "convoy", "convoy",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                r.right - r.left, r.bottom - r.top,
                                NULL, NULL, inst, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    HDC dc = GetDC(hwnd);

    g_bmi.bmiHeader.biSize        = sizeof(g_bmi.bmiHeader);
    g_bmi.bmiHeader.biWidth       = FB_W;
    g_bmi.bmiHeader.biHeight      = -FB_H;   // top-down
    g_bmi.bmiHeader.biPlanes      = 1;
    g_bmi.bmiHeader.biBitCount    = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;

    GameMemory mem = {0};
    mem.permanent_size = 16u << 20;
    mem.permanent = VirtualAlloc(NULL, mem.permanent_size,
                                 MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!mem.permanent) return 1;

    Framebuffer fb = { g_pixels, FB_W, FB_H };
    game_init(&mem, (uint32_t)GetTickCount());
    audio_open();   // failure here is survivable and deliberately ignored

    timeBeginPeriod(1);
    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    const double tick_secs = 1.0 / (double)TICK_HZ;
    double accum = 0.0;

    while (g_running) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        accum += (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
        last = now;
        if (accum > 0.25) accum = 0.25;   // don't spiral after a stall

        int stepped = 0;
        while (accum >= tick_secs) {
            Input in = {0};
            read_input(&in);
            game_update(&mem, &in, &fb);
            audio_pump(&mem);
            accum -= tick_secs;
            stepped = 1;
        }

        if (stepped) {
            RECT cr;
            GetClientRect(hwnd, &cr);
            StretchDIBits(dc, 0, 0, cr.right - cr.left, cr.bottom - cr.top,
                          0, 0, FB_W, FB_H, g_pixels, &g_bmi,
                          DIB_RGB_COLORS, SRCCOPY);
        } else {
            Sleep(1);
        }
    }

    audio_close();
    timeEndPeriod(1);
    return 0;
}
