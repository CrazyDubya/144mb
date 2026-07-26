#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include <stddef.h>

#define FB_W 640
#define FB_H 480

typedef struct { uint32_t *pixels; int w, h; } Framebuffer;
typedef struct { uint8_t down[10], pressed[10]; } Input;
enum { UP, DOWN, LEFT, RIGHT, A, B, C, D, START, HELP };

static inline void pixel(Framebuffer *f, int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)f->w && (unsigned)y < (unsigned)f->h)
        f->pixels[y * f->w + x] = c;
}
static inline void clear(Framebuffer *f, uint32_t c) {
    for (int i = 0; i < f->w * f->h; ++i) f->pixels[i] = c;
}
static inline void rect(Framebuffer *f, int x, int y, int w, int h, uint32_t c) {
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > f->w) w = f->w - x;
    if (y + h > f->h) h = f->h - y;
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx) f->pixels[yy*f->w+xx] = c;
}
static inline void line(Framebuffer *f, int x0, int y0, int x1, int y1, uint32_t c) {
    int dx=x1>x0?x1-x0:x0-x1, sx=x0<x1?1:-1, dy=-(y1>y0?y1-y0:y0-y1), sy=y0<y1?1:-1, e=dx+dy;
    for (;;) { pixel(f,x0,y0,c); if(x0==x1&&y0==y1)break; int e2=2*e; if(e2>=dy){e+=dy;x0+=sx;} if(e2<=dx){e+=dx;y0+=sy;} }
}
static inline void circle(Framebuffer *f, int cx, int cy, int r, uint32_t c) {
    int x=r,y=0,e=0; while(x>=y){pixel(f,cx+x,cy+y,c);pixel(f,cx+y,cy+x,c);pixel(f,cx-y,cy+x,c);pixel(f,cx-x,cy+y,c);pixel(f,cx-x,cy-y,c);pixel(f,cx-y,cy-x,c);pixel(f,cx+y,cy-x,c);pixel(f,cx+x,cy-y,c);y++;if(e<=0)e+=2*y+1;if(e>0){x--;e-=2*x+1;}}
}

const char *game_name(void);
const char *game_help(void);
const char *game_goal(void);
void game_init(uint32_t seed);
void game_tick(const Input *in);
void game_draw(Framebuffer *fb);
void game_status(char *dst, size_t n);
Input game_autoplay(int tick);
Input game_careless(int tick);
uint32_t game_hash(void);
int game_result(void);
void game_audio(int16_t *samples,int frames);
#endif
