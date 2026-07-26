#include "game.h"
#include "scene_pixels.h"
#include <stdio.h>
#include <string.h>
enum{WAIT,MOVE,AIM,FIRE,DIVE};
static const char*aname[]={"WAIT","MOVE","AIM","FIRE","DIVE"};
static struct{uint32_t rng;int plan[3],enemy[3],slot,phase,clock,px,ex,php,ehp,paim,eaim,pdive,edive,round,stage,won,lost,tick;}g;
static uint32_t rnd(void){g.rng=g.rng*1664525u+1013904223u;return g.rng;}
const char*game_name(void){return "TEN PACES BETA";}
const char*game_help(void){return "LEFT/RIGHT SLOT  UP/DOWN ACTION  Z EXECUTE  X CLEAR  ENTER RESTART";}
const char*game_goal(void){return "READ THE OUTLAW'S INTENT, PLAN THREE BEATS, AND SURVIVE FOUR DUELS.";}
static void roll_enemy(void){for(int k=0;k<3;k++){int d=g.ex-g.px;uint32_t r=rnd()%100;int aggression=35+g.stage*7;g.enemy[k]=d<170?(r<(uint32_t)aggression?FIRE:DIVE):(r<40?MOVE:r<75?AIM:FIRE);}}
static void begin_duel(void){static const int hp[4]={2,3,3,4};g.px=110;g.ex=530;g.php=3;g.ehp=hp[g.stage];g.paim=g.eaim=g.pdive=g.edive=0;g.phase=0;g.clock=0;g.slot=0;memset(g.plan,0,sizeof g.plan);roll_enemy();}
void game_init(uint32_t seed){memset(&g,0,sizeof g);g.rng=seed;begin_duel();}
static void act(int who,int a){
 int *x=who?&g.ex:&g.px,*aim=who?&g.eaim:&g.paim,*dive=who?&g.edive:&g.pdive;*dive=0;
 if(a==MOVE)*x+=who?-55:55;else if(a==AIM)*aim=1;else if(a==DIVE)*dive=1;else if(a==FIRE){int hit=*aim||((rnd()%100)<35);int otherdive=who?g.pdive:g.edive;if(hit&&!otherdive){if(who)g.php--;else g.ehp--;}*aim=0;}
}
void game_tick(const Input*in){
 if(in->pressed[START]){game_init(g.rng+1);return;}if(g.won||g.lost)return;g.tick++;
 if(g.phase==0){if(in->pressed[LEFT]&&g.slot>0)g.slot--;if(in->pressed[RIGHT]&&g.slot<2)g.slot++;if(in->pressed[UP])g.plan[g.slot]=(g.plan[g.slot]+1)%5;if(in->pressed[DOWN])g.plan[g.slot]=(g.plan[g.slot]+4)%5;if(in->pressed[B])memset(g.plan,0,sizeof g.plan);if(in->pressed[A]){g.phase=1;g.clock=0;g.round++;}}
 else{g.clock++;if(g.clock%60==1){int s=g.clock/60;if(s<3){act(0,g.plan[s]);act(1,g.enemy[s]);}if(g.php<=0){g.lost=1;return;}if(g.ehp<=0){if(g.stage>=3)g.won=1;else{g.stage++;begin_duel();}return;}}if(g.clock>=180&&!g.won&&!g.lost){g.phase=0;g.clock=0;g.slot=0;memset(g.plan,0,sizeof g.plan);roll_enemy();}}
}
static void cowboy(Framebuffer*f,int x,int hp,uint32_t c,int dive){int y=dive?375:330;circle(f,x,y-30,9,c);rect(f,x-10,y-21,20,34,c);line(f,x-8,y+13,x-13,y+38,c);line(f,x+8,y+13,x+13,y+38,c);line(f,x-17,y-39,x+17,y-39,c);rect(f,x-10,y-44,20,6,c);for(int i=0;i<hp;i++)rect(f,x-14+i*10,y+45,7,5,0x00db3e4c);}
void game_draw(Framebuffer*f){int scene=g.won?5:g.lost?4:g.stage;scene_frame(f,scene,86);rect(f,0,390,640,90,0x00865b35);rect(f,270,250,100,140,0x00694029);rect(f,288,300,30,90,0x00291f19);rect(f,50,300,75,90,0x008b4b2c);if(g.stage>=1)rect(f,470,280,85,110,0x005a3928);if(g.stage>=2)line(f,0,210,640,260,0x00342d2a);if(g.stage>=3)rect(f,390,330,55,60,0x002e2521);line(f,0,390,640,390,0x004c3525);cowboy(f,g.px,g.php,0x003e5d73,g.pdive);cowboy(f,g.ex,g.ehp,0x007b2938,g.edive);for(int i=0;i<3;i++){uint32_t c=i==g.slot&&g.phase==0?0x00fff0a5:0x00594535;rect(f,170+i*105,55,90,36,c);for(int k=0;k<g.plan[i];k++)rect(f,177+k*13,63,8,20,0x00211b18);for(int k=0;k<g.enemy[i];k++)rect(f,177+k*13,91,8,6,0x00852f3c);}if(g.phase==1)rect(f,170,105,g.clock*300/180,5,0x00f3d36a);if(g.won)rect(f,170,180,300,110,0x00296b47);if(g.lost)rect(f,170,180,300,110,0x00792b38);}
void game_status(char*d,size_t n){static const char*place[4]={"SALOON","TRAIN YARD","CANYON","HIGH STREET"};snprintf(d,n,"DUEL %d/4 %s  ROUND %d  YOU %d HP  OUTLAW %d HP  PLAN: %s / %s / %s%s",g.stage+1,place[g.stage],g.round,g.php,g.ehp,aname[g.plan[0]],aname[g.plan[1]],aname[g.plan[2]],g.won?"  CAMPAIGN WON":g.lost?"  YOU ARE DOWN":g.phase?"  EXECUTING":"");}
Input game_autoplay(int t){(void)t;Input i={0};if(g.phase==0){int aim=g.paim,want=WAIT;for(int k=0;k<=g.slot;k++){if(g.enemy[k]==FIRE)want=DIVE;else if(!aim){want=AIM;aim=1;}else{want=FIRE;aim=0;}if(k<g.slot){if(g.plan[k]==AIM)aim=1;if(g.plan[k]==FIRE)aim=0;}}if(g.plan[g.slot]!=want)i.pressed[UP]=1;else if(g.slot<2)i.pressed[RIGHT]=1;else i.pressed[A]=1;}return i;}
Input game_careless(int t){(void)t;Input i={0};if(g.phase==0)i.pressed[A]=1;return i;}
uint32_t game_hash(void){return (uint32_t)(g.px*3+g.ex*5+g.php*7+g.ehp*11+g.round*13+g.stage*17+g.tick);}
int game_result(void){return g.won?1:g.lost?-1:0;}
void game_audio(int16_t*s,int n){static uint32_t p,noise=9;int hz=98+g.stage*18;for(int i=0;i<n;i++){p+=(uint32_t)(hz*65536/44100);noise=noise*1664525u+1013904223u;int v=(p&0x8000)?900:-900;if(g.phase&&g.clock%60<5)v+=((int)(noise>>18)-8192)/2;s[i*2]=(int16_t)v;s[i*2+1]=(int16_t)v;}}
