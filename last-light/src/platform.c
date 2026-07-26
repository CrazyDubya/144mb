#include "game.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include "audio_win.h"
#include "onboarding_win.h"
static uint32_t pixels[FB_W * FB_H];
static int running = 1;
typedef struct { uint32_t magic; int muted, wins; } ReleaseSettings;
static char settings_path[512];
static ReleaseSettings settings = { 0x31434d42u, 0, 0 };

static void settings_open(void) {
    char base[400];
    DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", base, sizeof base);
    if (!n || n >= sizeof base) return;
    snprintf(settings_path, sizeof settings_path, "%s\\144mb", base);
    CreateDirectoryA(settings_path, 0);
    snprintf(settings_path, sizeof settings_path, "%s\\144mb\\%s.cfg",
             base, game_name());
    FILE *f = fopen(settings_path, "rb");
    if (f) {
        ReleaseSettings loaded;
        if (fread(&loaded, sizeof loaded, 1, f) == 1 &&
            loaded.magic == settings.magic) settings = loaded;
        fclose(f);
    }
}
static void settings_save(void) {
    if (!settings_path[0]) return;
    FILE *f = fopen(settings_path, "wb");
    if (f) { fwrite(&settings, sizeof settings, 1, f); fclose(f); }
}
static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CLOSE || m == WM_DESTROY) { running = 0; return 0; }
    if (m == WM_KEYDOWN && w == VK_ESCAPE) { running = 0; return 0; }
    return DefWindowProcA(h, m, w, l);
}
static void text_panel(HDC dc, const RECT *rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, rect, brush);
    DeleteObject(brush);
}
static void draw_release_text(HDC dc, HFONT small, HFONT large,
                              int started, const char *status) {
    if (!started) return;
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, small);
    SetTextColor(dc, RGB(220, 240, 255));
    RECT top = { 0, 0, 640, 52 }, top_text = { 8, 3, 632, 51 };
    RECT foot = { 0, 448, 640, 480 }, foot_text = { 5, 453, 635, 477 };
    text_panel(dc, &top, RGB(8, 12, 18));
    text_panel(dc, &foot, RGB(8, 12, 18));
    DrawTextA(dc, status, -1, &top_text, DT_CENTER | DT_WORDBREAK);
    DrawTextA(dc, game_help(), -1, &foot_text,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (game_result()) {
        RECT end = { 110, 150, 530, 330 }, body = { 125, 165, 515, 315 };
        text_panel(dc, &end, RGB(8, 12, 18));
        SelectObject(dc, large);
        SetTextColor(dc, RGB(245, 240, 210));
        DrawTextA(dc, game_ending(), -1, &body, DT_CENTER | DT_WORDBREAK);
    }
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show){
    (void)hp; (void)cmd;
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = proc; wc.hInstance = hi;
    wc.hCursor = LoadCursorA(0, IDC_ARROW); wc.lpszClassName = game_name();
    if (!RegisterClassA(&wc)) return 1;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = { 0, 0, FB_W, FB_H };
    AdjustWindowRect(&r, style, 0);
    HWND w = CreateWindowExA(0, game_name(), game_name(), style | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right-r.left, r.bottom-r.top,
        0, 0, hi, 0);
    if (!w) return 2;
    ShowWindow(w, show);
    HDC dc = GetDC(w);
    HFONT small = CreateFontA(-13,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
        FIXED_PITCH|FF_MODERN,"Consolas");
    HFONT large = CreateFontA(-18,0,0,0,FW_BOLD,0,0,0,ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
        DEFAULT_PITCH|FF_SWISS,"Segoe UI");
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=FB_W;
    bi.bmiHeader.biHeight=-FB_H;bi.bmiHeader.biPlanes=1;
    bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    Framebuffer fb = { pixels, FB_W, FB_H };
    uint8_t held[10]={0}, pending[10]={0};
    int vk[10]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,'Z','X','C','V',VK_RETURN,'H'};
    SYSTEMTIME date; GetLocalTime(&date);
    uint32_t daily_seed=(uint32_t)date.wYear*10000u+
        (uint32_t)date.wMonth*100u+date.wDay;
    settings_open(); game_init(daily_seed); release_audio_open();
    release_audio_mute(settings.muted);
    int started=0,recorded=0;char status[256];
    LARGE_INTEGER fq,next,now;QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&next);LONGLONG step=fq.QuadPart/60;
    while(running){
        MSG m;
        while(PeekMessageA(&m,0,0,0,PM_REMOVE)){
            TranslateMessage(&m);DispatchMessageA(&m);
        }
        for(int i=0;i<10;i++){
            uint8_t raw=(GetAsyncKeyState(vk[i])&0x8000)!=0;
            pending[i]|=raw&&!held[i];held[i]=raw;
        }
        if(GetAsyncKeyState('M')&1){
            settings.muted=!settings.muted;release_audio_mute(settings.muted);
            settings_save();
        }
        QueryPerformanceCounter(&now);int loops=0;
        while(now.QuadPart>=next.QuadPart&&loops++<5){
            Input in={0};memcpy(in.down,held,sizeof held);
            memcpy(in.pressed,pending,sizeof pending);
            memset(pending,0,sizeof pending);
            if(!started){if(in.pressed[START])started=1;}
            else if(!in.down[HELP]&&GetForegroundWindow()==w)game_tick(&in);
            int result=game_result();
            if(result==1&&!recorded){
                settings.wins++;settings_save();recorded=1;
            }
            if(!result)recorded=0;
            next.QuadPart+=step;
        }
        if(loops>5)next=now;
        release_audio_pump();game_draw(&fb);
        if(!started)release_title_frame(pixels);
        StretchDIBits(dc,0,0,FB_W,FB_H,0,0,FB_W,FB_H,pixels,&bi,
                      DIB_RGB_COLORS,SRCCOPY);
        game_status(status,sizeof status);
        draw_release_text(dc,small,large,started,status);
        SelectObject(dc,small);
        if(!started||held[HELP])release_onboarding(dc,!started);
        Sleep(1);
    }
    release_audio_close();SelectObject(dc,GetStockObject(SYSTEM_FONT));
    DeleteObject(small);DeleteObject(large);ReleaseDC(w,dc);return 0;
}
#else
#include <stdlib.h>
static void put_u16(unsigned char*p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void put_u32(unsigned char*p,uint32_t v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static int write_bmp(const char*path,const uint32_t*p){
    FILE*f=fopen(path,"wb");if(!f)return 0;unsigned char h[54]={0};int stride=(FB_W*3+3)&~3;uint32_t bytes=(uint32_t)(54+stride*FB_H);
    h[0]='B';h[1]='M';put_u32(h+2,bytes);put_u32(h+10,54);put_u32(h+14,40);put_u32(h+18,FB_W);put_u32(h+22,FB_H);put_u16(h+26,1);put_u16(h+28,24);put_u32(h+34,(uint32_t)(stride*FB_H));fwrite(h,sizeof h,1,f);
    for(int y=FB_H-1;y>=0;y--){for(int x=0;x<FB_W;x++){uint32_t c=p[y*FB_W+x];unsigned char bgr[3]={(unsigned char)c,(unsigned char)(c>>8),(unsigned char)(c>>16)};fwrite(bgr,3,1,f);}for(int x=FB_W*3;x<stride;x++)fputc(0,f);}
    int ok=!ferror(f);fclose(f);return ok;
}
int main(int argc,char**argv){
    int seed=1,runs=1,ticks=36000,loss=0,fuzz=0,snapshot=0;const char*output=0;for(int i=1;i<argc;i++){if(!strcmp(argv[i],"-s")&&i+1<argc)seed=atoi(argv[++i]);else if(!strcmp(argv[i],"-N")&&i+1<argc)runs=atoi(argv[++i]);else if(!strcmp(argv[i],"-t")&&i+1<argc)ticks=atoi(argv[++i]);else if(!strcmp(argv[i],"-o")&&i+1<argc)output=argv[++i];else if(!strcmp(argv[i],"-L"))loss=1;else if(!strcmp(argv[i],"-F"))fuzz=1;else if(!strcmp(argv[i],"-Q"))snapshot=1;}
    uint32_t*p=calloc(FB_W*FB_H,4);if(!p)return 2;Framebuffer fb={p,FB_W,FB_H};int wins=0;
    for(int r=0;r<runs;r++){game_init((uint32_t)(seed+r));int t=0;for(;t<ticks&&!game_result();t++){Input in;if(fuzz){memset(&in,0,sizeof in);if(t%7==0)in.pressed[(seed+r+t/7)%8]=1;if(t%5==0)in.down[(seed+r+t/5)%4]=1;}else in=loss?game_careless(t):game_autoplay(t);game_tick(&in);}game_draw(&fb);if(output&&r==runs-1&&!write_bmp(output,p)){fprintf(stderr,"could not write %s\n",output);free(p);return 4;}uint32_t h=2166136261u;for(int i=0;i<FB_W*FB_H;i++){h^=p[i];h*=16777619u;}h^=game_hash();int16_t aud[1470];game_audio(aud,735);int peak=0;for(int i=0;i<1470;i++){int v=aud[i]<0?-aud[i]:aud[i];if(v>peak)peak=v;}int ok=((fuzz||snapshot)?peak>100:(loss?game_result()==-1:game_result()==1)&&peak>100);char s[256];game_status(s,sizeof s);printf("%s %s seed=%d ticks=%d hash=%08x audio=%d %s\n",ok?"PASS":"FAIL",game_name(),seed+r,t,h,peak,s);wins+=ok;}
    printf("SWEEP %s %d/%d completed\n",game_name(),wins,runs);free(p);return wins==runs?0:3;
}
#endif
