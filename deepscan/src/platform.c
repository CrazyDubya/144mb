#include "game.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include "audio_win.h"
#include "onboarding_win.h"
static uint32_t pixels[FB_W*FB_H];
static int running=1;
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_CLOSE||m==WM_DESTROY){running=0;return 0;}if(m==WM_KEYDOWN&&w==VK_ESCAPE){running=0;return 0;}return DefWindowProcA(h,m,w,l);}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show){
    (void)hp;(void)cmd; WNDCLASSA wc={0};wc.lpfnWndProc=proc;wc.hInstance=hi;wc.hCursor=LoadCursorA(0,IDC_ARROW);wc.lpszClassName=game_name();if(!RegisterClassA(&wc))return 1;
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;RECT r={0,0,FB_W,FB_H};AdjustWindowRect(&r,style,0);HWND w=CreateWindowExA(0,game_name(),game_name(),style|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,0,0,hi,0);if(!w)return 2;
    ShowWindow(w,show);HDC dc=GetDC(w);BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=FB_W;bi.bmiHeader.biHeight=-FB_H;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    Framebuffer fb={pixels,FB_W,FB_H};uint8_t held[10]={0},pending[10]={0};int vk[10]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,'Z','X','C','V',VK_RETURN,'H'};game_init(GetTickCount());release_audio_open();int started=0,muted=0;char status[256];LARGE_INTEGER fq,next,now;QueryPerformanceFrequency(&fq);QueryPerformanceCounter(&next);LONGLONG step=fq.QuadPart/60;
    while(running){MSG m;while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}for(int i=0;i<10;i++){uint8_t raw=(GetAsyncKeyState(vk[i])&0x8000)!=0;pending[i]|=raw&&!held[i];held[i]=raw;}if(GetAsyncKeyState('M')&1){muted=!muted;release_audio_mute(muted);}QueryPerformanceCounter(&now);int loops=0;while(now.QuadPart>=next.QuadPart&&loops++<5){Input in={0};memcpy(in.down,held,sizeof held);memcpy(in.pressed,pending,sizeof pending);memset(pending,0,sizeof pending);if(!started){if(in.pressed[START])started=1;}else if(!in.down[HELP]&&GetForegroundWindow()==w)game_tick(&in);next.QuadPart+=step;}if(loops>5)next=now;release_audio_pump();game_draw(&fb);if(!started)release_title_frame(pixels);StretchDIBits(dc,0,0,FB_W,FB_H,0,0,FB_W,FB_H,pixels,&bi,DIB_RGB_COLORS,SRCCOPY);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(220,240,255));game_status(status,sizeof status);TextOutA(dc,12,10,status,(int)strlen(status));TextOutA(dc,12,456,game_help(),(int)strlen(game_help()));if(!started||held[HELP])release_onboarding(dc,!started);Sleep(1);}
    release_audio_close();return 0;
}
#else
#include <stdlib.h>
int main(int argc,char**argv){
    int seed=1,runs=1,ticks=12000,loss=0;for(int i=1;i<argc;i++){if(!strcmp(argv[i],"-s")&&i+1<argc)seed=atoi(argv[++i]);else if(!strcmp(argv[i],"-N")&&i+1<argc)runs=atoi(argv[++i]);else if(!strcmp(argv[i],"-t")&&i+1<argc)ticks=atoi(argv[++i]);else if(!strcmp(argv[i],"-L"))loss=1;}
    uint32_t*p=calloc(FB_W*FB_H,4);if(!p)return 2;Framebuffer fb={p,FB_W,FB_H};int wins=0;
    for(int r=0;r<runs;r++){game_init((uint32_t)(seed+r));int t=0;for(;t<ticks&&!game_result();t++){Input in=loss?game_careless(t):game_autoplay(t);game_tick(&in);}game_draw(&fb);uint32_t h=2166136261u;for(int i=0;i<FB_W*FB_H;i++){h^=p[i];h*=16777619u;}h^=game_hash();int16_t aud[1470];game_audio(aud,735);int peak=0;for(int i=0;i<1470;i++){int v=aud[i]<0?-aud[i]:aud[i];if(v>peak)peak=v;}int ok=(loss?game_result()==-1:game_result()==1)&&peak>100;char s[256];game_status(s,sizeof s);printf("%s %s seed=%d ticks=%d hash=%08x audio=%d %s\n",ok?"PASS":"FAIL",game_name(),seed+r,t,h,peak,s);wins+=ok;}
    printf("SWEEP %s %d/%d completed\n",game_name(),wins,runs);free(p);return wins==runs?0:3;
}
#endif
