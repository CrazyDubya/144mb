#include "game.h"
#include "scene_pixels.h"
#include <stdio.h>
#include <string.h>
typedef struct{int pos,dir,axis,wait,active;}Train;
static struct{Train t[6];int priority,hold,delivered,total,shift,delay,crash,won,tick;uint32_t rng;}g;
static uint32_t rnd(void){g.rng=g.rng*1103515245u+12345u;return g.rng;}
const char*game_name(void){return "SWITCHYARD BETA";}
const char*game_help(void){return "Z CHANGE PRIORITY EAST-WEST/NORTH-SOUTH   X HOLD ALL   ENTER RESTART";}
const char*game_goal(void){return "CLEAR THREE DISTRICTS WITHOUT A COLLISION OR CATASTROPHIC DELAY.";}
static void spawn(Train*t,int axis,int side){t->axis=axis;t->dir=side? -1:1;t->pos=side?650:-10;t->wait=0;t->active=1;}
static void begin_shift(void){memset(g.t,0,sizeof g.t);g.delivered=0;g.hold=0;spawn(&g.t[0],0,0);spawn(&g.t[1],1,1);}
void game_init(uint32_t seed){memset(&g,0,sizeof g);g.rng=seed;g.priority=0;begin_shift();}
void game_tick(const Input*in){
 if(in->pressed[START]){game_init(g.rng+1);return;}if(g.crash||g.won)return;g.tick++;
 int locked=0;for(int i=0;i<6;i++)if(g.t[i].active&&g.t[i].axis==g.priority){int center=g.t[i].axis?240:320;if(g.t[i].pos>center-80&&g.t[i].pos<center+80)locked=1;}
 if(in->pressed[A]&&!locked)g.priority=!g.priority;
 if(in->pressed[B])g.hold=!g.hold;
 static const int interval[3]={160,130,100},speed[3]={2,2,3},target[3]={6,8,10};
 if(g.tick%interval[g.shift]==0)for(int i=0;i<6;i++)if(!g.t[i].active){spawn(&g.t[i],(int)(rnd()&1),(int)((rnd()>>3)&1));break;}
 for(int i=0;i<6;i++){Train*t=&g.t[i];if(!t->active)continue;int center=t->axis?240:320;int near=t->pos>center-60&&t->pos<center+60;int green=!g.hold&&t->axis==g.priority;if(near&&!green){t->wait++;g.delay++;continue;}t->pos+=t->dir*speed[g.shift];if(t->pos>670||t->pos<-30){t->active=0;g.delivered++;g.total++;}}
 for(int i=0;i<6;i++)for(int j=i+1;j<6;j++)if(g.t[i].active&&g.t[j].active&&g.t[i].axis!=g.t[j].axis){
  int ci=g.t[i].axis?240:320,cj=g.t[j].axis?240:320;
  int di=g.t[i].pos-ci,dj=g.t[j].pos-cj;if(di<0)di=-di;if(dj<0)dj=-dj;
  if(di<26&&dj<26)g.crash=1;
 }
 if(!g.crash&&g.delivered>=target[g.shift]){g.shift++;if(g.shift>=3)g.won=1;else begin_shift();}
 if(g.delay>1500)g.crash=1;
}
void game_draw(Framebuffer*f){
 int sh=g.shift<3?g.shift:2;scene_frame(f,g.crash?5:sh,70);for(int x=0;x<640;x+=20){rect(f,x,228,10,4,0x006b7068);rect(f,x,248,10,4,0x006b7068);}for(int y=50;y<440;y+=20){rect(f,310,y,4,10,0x006b7068);rect(f,330,y,4,10,0x006b7068);}rect(f,0,234,640,12,0x00969b91);rect(f,316,40,12,400,0x00969b91);
 if(sh>=1){line(f,80,234,210,150,0x00969b91);line(f,80,246,210,162,0x00969b91);}if(sh>=2){line(f,430,234,580,330,0x00969b91);line(f,430,246,580,342,0x00969b91);}
 uint32_t ew=g.priority==0&&!g.hold?0x004ed97a:0x00e04f5f,ns=g.priority==1&&!g.hold?0x004ed97a:0x00e04f5f;circle(f,270,216,7,ew);circle(f,344,270,7,ns);
 for(int i=0;i<6;i++){Train*t=&g.t[i];if(!t->active)continue;uint32_t c=(i&1)?0x00f2c14e:0x0058a6e7;if(t->axis==0){rect(f,t->pos-16,232,32,16,c);rect(f,t->pos-10,228,18,5,c);}else{rect(f,314,t->pos-16,16,32,c);rect(f,310,t->pos-10,5,18,c);}}
 if(g.crash)rect(f,190,170,260,120,0x00952038);
 if(g.won)rect(f,190,170,260,120,0x002a8055);
}
void game_status(char*d,size_t n){static const char*name[3]={"RURAL BRANCH","COMMUTER JUNCTION","FREIGHT CORRIDOR"};static const int target[3]={6,8,10};int sh=g.shift<3?g.shift:2;snprintf(d,n,"DISTRICT %d/3 %s  SERVICES %d/%d  TOTAL %d  DELAY %d  PRIORITY %s%s%s",sh+1,name[sh],g.delivered,target[sh],g.total,g.delay,g.priority?"NORTH-SOUTH":"EAST-WEST",g.hold?"  ALL HELD":"",g.crash?"  NETWORK FAILED":g.won?"  CAMPAIGN COMPLETE":"");}
Input game_autoplay(int t){
 (void)t;Input i={0};int want=g.priority,best=10000;
 for(int k=0;k<6;k++)if(g.t[k].active){int center=g.t[k].axis?240:320;int d=g.t[k].pos-center;if(d<0)d=-d;if(d<best){best=d;want=g.t[k].axis;}if(g.t[k].pos>center-35&&g.t[k].pos<center+35){want=g.t[k].axis;break;}}
 if(want!=g.priority)i.pressed[A]=1;
 return i;
}
Input game_careless(int t){(void)t;Input i={0};return i;}
uint32_t game_hash(void){return (uint32_t)(g.delivered*17+g.total*19+g.shift*23+g.delay*3+g.priority+g.tick*7+g.crash*101);}
int game_result(void){return g.won?1:g.crash?-1:0;}
void game_audio(int16_t*s,int n){static uint32_t p;int hz=82+g.shift*28,amp=g.hold?700:1600;for(int i=0;i<n;i++){p+=(uint32_t)(hz*65536/44100);int beat=((g.tick/15)&3)==0;int v=(p&0x8000)?amp:-amp;if(beat)v+=900;s[i*2]=(int16_t)v;s[i*2+1]=(int16_t)v;}}
