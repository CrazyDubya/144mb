#include "game.h"
#include <stdio.h>
#include <string.h>

typedef struct { int x,y,vx,vy; } Thing;
static struct {
    Thing sub, beast[7];
    int battery, oxygen, hull, ping, silent, samples, required;
    int black_x, black_y, blackbox, contract, won, lost, tick;
    uint32_t rng;
} g;
static uint32_t rnd(void){g.rng=g.rng*1664525u+1013904223u;return g.rng;}
static int absi(int x){return x<0?-x:x;}
const char *game_name(void){return "DEEPSCAN BETA";}
const char *game_help(void){return "ARROWS THRUST   Z SONAR   X SILENT RUNNING   ENTER RESTART   ESC QUIT";}
const char *game_goal(void){return "TAG THE CONTRACT SPECIMENS, RECOVER THE BLACK BOX, THEN RETURN TO THE SURFACE.";}
void game_init(uint32_t seed){
    memset(&g,0,sizeof g);g.rng=seed;g.sub.x=320;g.sub.y=42;g.battery=100;g.oxygen=100;g.hull=100;
    g.contract=(int)(seed%3);g.required=4+g.contract;g.black_x=80+(int)(rnd()%480);g.black_y=350+(int)(rnd()%90);
    for(int i=0;i<7;i++){g.beast[i].x=50+(int)(rnd()%540);g.beast[i].y=120+i*48;}
}
void game_tick(const Input *in){
    if(in->pressed[START]){game_init(g.rng+1);return;} if(g.won||g.lost)return;g.tick++;
    if(in->down[LEFT])g.sub.vx--;
    if(in->down[RIGHT])g.sub.vx++;
    if(in->down[UP])g.sub.vy--;
    if(in->down[DOWN])g.sub.vy++;
    if(in->pressed[B])g.silent=!g.silent;
    if(in->pressed[A]&&g.battery>=5){g.ping=1;g.battery-=5;}
    if(g.silent){g.sub.vx=g.sub.vx*3/4;g.sub.vy=g.sub.vy*3/4;}else if(g.tick%90==0&&g.battery>0)g.battery--;
    if(g.sub.vx>3)g.sub.vx=3;
    if(g.sub.vx<-3)g.sub.vx=-3;
    if(g.sub.vy>3)g.sub.vy=3;
    if(g.sub.vy<-3)g.sub.vy=-3;
    g.sub.x+=g.sub.vx;g.sub.y+=g.sub.vy;g.sub.vx=g.sub.vx*7/8;g.sub.vy=g.sub.vy*7/8;
    if(g.sub.x<20){g.sub.x=20;g.hull--;}if(g.sub.x>620){g.sub.x=620;g.hull--;}if(g.sub.y<25)g.sub.y=25;if(g.sub.y>455){g.sub.y=455;g.hull--;}
    if(g.ping){g.ping+=4;if(g.ping>260)g.ping=0;}
    if(g.ping&&absi(g.black_x-g.sub.x)+absi(g.black_y-g.sub.y)<30)g.blackbox=1;
    for(int i=0;i<7;i++){
        Thing *b=&g.beast[i];int d=absi(b->x-g.sub.x)+absi(b->y-g.sub.y);
        if(g.ping||(!g.silent&&(absi(g.sub.vx)+absi(g.sub.vy)>1))){b->x+=(g.sub.x>b->x)?1:-1;b->y+=(g.sub.y>b->y)?1:-1;}
        else if((g.tick+i*17)%80==0)b->x+=(rnd()&1)?2:-2;
        if(d<18){g.hull--;b->x=40+(int)(rnd()%560);b->y=120+(int)(rnd()%330);}
        if(d<24&&g.ping){if(g.samples<g.required)g.samples++;b->y=-100-i*10;}
    }
    if(g.tick%120==0)g.oxygen--;
    if(g.sub.y>410&&g.tick%90==0)g.hull--;
    if(g.battery<=0||g.oxygen<=0||g.hull<=0)g.lost=1;
    if(g.samples>=g.required&&g.blackbox&&g.sub.y<45)g.won=1;
}
void game_draw(Framebuffer *f){
    clear(f,0x00030b16);for(int y=80;y<FB_H;y+=40)rect(f,0,y,FB_W,1,0x00071425);
    for(int i=0;i<18;i++){int x=(i*97+g.tick/5)%640;int y=70+(i*61)%400;pixel(f,x,y,0x00305a69);}
    rect(f,0,465,FB_W,15,0x00131e25);for(int x=0;x<640;x+=31)line(f,x,465,x+16,450-(x%43),0x001b3036);
    if(g.ping)circle(f,g.sub.x,g.sub.y,g.ping,0x0049cfe8);
    for(int i=0;i<7;i++){Thing*b=&g.beast[i];int d=absi(b->x-g.sub.x)+absi(b->y-g.sub.y);int visible=g.ping&&absi(d-g.ping)<18;if(visible){circle(f,b->x,b->y,8,0x00ee5366);line(f,b->x-12,b->y,b->x+12,b->y,0x00ee5366);}}
    if(!g.blackbox){rect(f,g.black_x-8,g.black_y-5,16,10,0x00d9a441);line(f,g.black_x-5,g.black_y-8,g.black_x+5,g.black_y-8,0x00d9a441);}
    uint32_t sc=g.silent?0x006bd6bd:0x00f2ca52;rect(f,g.sub.x-10,g.sub.y-4,20,9,sc);line(f,g.sub.x+10,g.sub.y,g.sub.x+17,g.sub.y-5,sc);line(f,g.sub.x+10,g.sub.y,g.sub.x+17,g.sub.y+5,sc);
    rect(f,10,34,g.battery*2,6,0x00f2ca52);rect(f,10,44,g.oxygen*2,6,0x0056b4e9);rect(f,10,54,g.hull*2,6,0x00ee5366);
    for(int i=0;i<g.samples&&i<6;i++)circle(f,510+i*20,45,6,0x00a8f0d0);
    if(g.won)rect(f,150,190,340,100,0x001b6b4b);
    if(g.lost)rect(f,150,190,340,100,0x00752038);
}
void game_status(char *d,size_t n){static const char*c[]={"SURVEY","SALVAGE","ABYSS"};snprintf(d,n,"%s  BAT %d  O2 %d  HULL %d  SPECIMENS %d/%d  BLACK BOX %s  DEPTH %d%s%s",c[g.contract],g.battery,g.oxygen,g.hull,g.samples,g.required,g.blackbox?"FOUND":"MISSING",g.sub.y,g.won?"  ASCENT COMPLETE":g.lost?"  LOST":"",g.silent?"  SILENT":"");}
Input game_autoplay(int t){
    (void)t;Input i={0};
    if(g.samples>=g.required&&!g.blackbox){int d=absi(g.black_x-g.sub.x)+absi(g.black_y-g.sub.y);if(g.black_x<g.sub.x-4)i.down[LEFT]=1;if(g.black_x>g.sub.x+4)i.down[RIGHT]=1;if(g.black_y<g.sub.y-4)i.down[UP]=1;if(g.black_y>g.sub.y+4)i.down[DOWN]=1;if(d<35&&g.ping==0)i.pressed[A]=1;return i;}
    if(g.samples>=g.required&&g.blackbox){i.down[UP]=1;return i;}
    int best=-1,bd=100000;for(int k=0;k<7;k++)if(g.beast[k].y>=0){int d=absi(g.beast[k].x-g.sub.x)+absi(g.beast[k].y-g.sub.y);if(d<bd){bd=d;best=k;}}
    if(best>=0){Thing*b=&g.beast[best];if(b->x<g.sub.x-4)i.down[LEFT]=1;if(b->x>g.sub.x+4)i.down[RIGHT]=1;if(b->y<g.sub.y-4)i.down[UP]=1;if(b->y>g.sub.y+4)i.down[DOWN]=1;if(bd<35&&g.ping==0)i.pressed[A]=1;}
    return i;
}
Input game_careless(int t){(void)t;Input i={0};return i;}
uint32_t game_hash(void){return (uint32_t)(g.sub.x*3+g.sub.y*5+g.battery*7+g.hull*11+g.samples*13+g.blackbox*17+g.contract*19+g.tick);}
int game_result(void){return g.won?1:g.lost?-1:0;}
void game_audio(int16_t*s,int n){static uint32_t p;int hz=g.ping?880:55+g.sub.y/8,amp=g.ping?6000:1800;for(int i=0;i<n;i++){p+=(uint32_t)(hz*65536/44100);int v=(p&0x8000)?amp:-amp;if(!g.ping)v+=(int)((g.rng>>(i&15))&255)-128;s[i*2]=(int16_t)v;s[i*2+1]=(int16_t)v;}}
