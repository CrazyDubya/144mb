#include "game.h"
#include "scene_pixels.h"
#include <stdio.h>
#include <string.h>

#define GW 40
#define GH 28
#define CELLS (GW*GH)
enum { EMPTY, PRODUCER, GRAZER, PREDATOR, DECOMPOSER, PARASITE, SPORE, SPECIES };
enum { TOOL_LIGHT, TOOL_PH, TOOL_SPECIES, TOOL_CLEAN, TOOL_CURRENT, TOOL_QUARANTINE, TOOLS };
enum { USE_NUTRIENT=1,USE_ANTIBIOTIC=2,USE_LIGHT=4,USE_PH=8,USE_SPECIES=16,USE_CLEAN=32,USE_CURRENT=64,USE_QUARANTINE=128 };
static const char *sample_name[6]={"POND BALANCE","RUNOFF RECOVERY","HOSPITAL CULTURE","VENT DIVERSITY","ALIEN ICE","SYNTHETIC CONTAINMENT"};
static const char *tool_name[TOOLS]={"LIGHT","ACIDITY","INTRODUCE","CLEAN","CURRENT","QUARANTINE"};
static const unsigned required[6]={
    USE_NUTRIENT,USE_CLEAN|USE_CURRENT,USE_ANTIBIOTIC|USE_QUARANTINE,
    USE_LIGHT|USE_PH,USE_SPECIES|USE_CURRENT,USE_ANTIBIOTIC|USE_CLEAN|USE_QUARANTINE
};
static struct {
    uint8_t cell[CELLS],next[CELLS],quarantine[CELLS];
    uint32_t rng;
    int cx,cy,tick,stable,mission,budget,selected,light,ph,current,toxin;
    int count[SPECIES],history[6][64],history_at,won,lost;
    unsigned used;
} g;

static uint32_t rnd(void){g.rng=g.rng*1664525u+1013904223u;return g.rng;}
const char*game_name(void){return "MICROCOLONY 1.0 RC1";}
const char*game_help(void){return "ARROWS PIPETTE  Z NUTRIENT  X ANTIBIOTIC  C SELECT TOOL  V APPLY  H HELP  M MUTE";}
const char*game_goal(void){return "STEER SIX ECOSYSTEMS. USE EACH SAMPLE'S REQUIRED TOOLS, THEN HOLD ALL SIX POPULATIONS STABLE.";}

static void recount(void){
    memset(g.count,0,sizeof g.count);
    for(int i=0;i<CELLS;i++)if(g.cell[i]<SPECIES)g.count[g.cell[i]]++;
}
static void populate(void){
    memset(g.cell,0,sizeof g.cell);memset(g.next,0,sizeof g.next);memset(g.quarantine,0,sizeof g.quarantine);
    g.stable=0;g.budget=32;g.used=0;g.selected=0;g.light=50;g.ph=50;g.current=0;g.toxin=g.mission==1?70:g.mission==2?35:g.mission==5?55:10;
    for(int i=0;i<CELLS;i++){int r=(int)(rnd()%100);int k=EMPTY;
        if(r<28)k=PRODUCER;else if(r<38)k=GRAZER;else if(r<43)k=PREDATOR;
        else if(r<51)k=DECOMPOSER;else if(r<55)k=PARASITE;else if(r<59)k=SPORE;
        g.cell[i]=(uint8_t)k;
    }
    if(g.mission==2)for(int i=0;i<CELLS;i+=13)g.cell[i]=PARASITE;
    if(g.mission==3)for(int i=0;i<CELLS;i+=11)g.cell[i]=SPORE;
    recount();
}
void game_init(uint32_t seed){memset(&g,0,sizeof g);g.rng=seed;g.cx=GW/2;g.cy=GH/2;populate();}

static int near(int x,int y,int kind){
    int n=0;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int xx=x+dx,yy=y+dy;if((dx||dy)&&xx>=0&&yy>=0&&xx<GW&&yy<GH&&g.cell[yy*GW+xx]==kind)n++;}return n;
}
static void simulate(void){
    for(int y=0;y<GH;y++)for(int x=0;x<GW;x++){int at=y*GW+x,k=g.cell[at],p=near(x,y,PRODUCER),h=near(x,y,GRAZER),r=near(x,y,PREDATOR),d=near(x,y,DECOMPOSER),q=near(x,y,PARASITE),v=k;
        if(g.quarantine[at]&&g.current)v=k;
        else if(k==EMPTY&&p>=2&&(rnd()%10)<(unsigned)(2+g.light/35))v=PRODUCER;
        else if(k==EMPTY&&d>=2&&(rnd()%16)==0)v=SPORE;
        else if(k==PRODUCER&&h>0&&(rnd()%5)==0)v=EMPTY;
        else if(k==PRODUCER&&p>=3&&(rnd()%30)==0)v=GRAZER;
        else if(k==GRAZER&&h>=3&&(rnd()%36)==0)v=PREDATOR;
        else if(k==GRAZER&&r>0&&(rnd()%5)==0)v=EMPTY;
        else if(k==GRAZER&&p==0&&(rnd()%5)==0)v=DECOMPOSER;
        else if(k==GRAZER&&q>0&&(rnd()%7)==0)v=PARASITE;
        else if(k==PREDATOR&&h==0&&(rnd()%4)==0)v=DECOMPOSER;
        else if(k==DECOMPOSER&&p>0&&(rnd()%24)==0)v=PRODUCER;
        else if(k==PARASITE&&h==0&&(rnd()%4)==0)v=SPORE;
        else if(k==SPORE&&g.ph>35&&g.ph<70&&(rnd()%18)==0)v=PRODUCER;
        if(g.toxin>45&&(rnd()%160)==0)v=EMPTY;
        g.next[at]=(uint8_t)v;
    }
    memcpy(g.cell,g.next,sizeof g.cell);recount();
    if(g.toxin>0&&g.current)g.toxin--;if(g.current>0)g.current--;
    int diverse=1;for(int k=PRODUCER;k<SPECIES;k++)if(g.count[k]<3)diverse=0;
    int balanced=g.count[PRODUCER]>=80&&g.count[PRODUCER]<=750&&g.count[GRAZER]>=20&&g.count[PREDATOR]>=3&&g.count[DECOMPOSER]>=8&&g.count[PARASITE]>=3&&g.count[SPORE]>=3&&g.toxin<65;
    int ready=(g.used&required[g.mission])==required[g.mission];
    if(diverse&&balanced&&ready)g.stable++;else if(g.stable>0)g.stable-=2;
    if(g.stable<0)g.stable=0;
    if((g.tick%50)==0){for(int k=0;k<6;k++)g.history[k][g.history_at]=g.count[k+1];g.history_at=(g.history_at+1)&63;}
    if(g.stable>=120){g.mission++;if(g.mission>=6)g.won=1;else populate();}
    int extinct=0;for(int k=PRODUCER;k<SPECIES;k++)if(g.count[k]==0)extinct=1;
    if(extinct||g.budget<0||g.tick>=36000)g.lost=1;
}

static void patch_cells(int radius,int action){
    for(int y=g.cy-radius;y<=g.cy+radius;y++)for(int x=g.cx-radius;x<=g.cx+radius;x++)if(x>=0&&y>=0&&x<GW&&y<GH){int at=y*GW+x,k=g.cell[at];
        if(action==USE_NUTRIENT&&k==EMPTY)g.cell[at]=PRODUCER;
        if(action==USE_ANTIBIOTIC&&(k==PARASITE||k==PREDATOR))g.cell[at]=EMPTY;
        if(action==USE_SPECIES&&k==EMPTY)g.cell[at]=(uint8_t)(GRAZER+rnd()%4);
        if(action==USE_CLEAN&&k==PARASITE)g.cell[at]=DECOMPOSER;
        if(action==USE_QUARANTINE)g.quarantine[at]=1;
    }
}
static void apply_tool(int use){
    if(g.budget<=0)return;g.budget--;g.used|=(unsigned)use;
    if(use==USE_NUTRIENT)patch_cells(3,use);
    if(use==USE_ANTIBIOTIC){patch_cells(2,use);g.toxin+=4;}
    if(use==USE_LIGHT){g.light=g.light<70?g.light+20:g.light-30;}
    if(use==USE_PH){g.ph=g.ph<60?g.ph+12:g.ph-18;}
    if(use==USE_SPECIES)patch_cells(3,use);
    if(use==USE_CLEAN){patch_cells(4,use);g.toxin-=18;if(g.toxin<0)g.toxin=0;}
    if(use==USE_CURRENT)g.current=300;
    if(use==USE_QUARANTINE)patch_cells(3,use);
    recount();
}
void game_tick(const Input*in){
    if(in->pressed[START]){game_init(g.rng+1);return;}if(g.won||g.lost)return;g.tick++;
    if(in->pressed[LEFT]&&g.cx>0)g.cx--;if(in->pressed[RIGHT]&&g.cx<GW-1)g.cx++;
    if(in->pressed[UP]&&g.cy>0)g.cy--;if(in->pressed[DOWN]&&g.cy<GH-1)g.cy++;
    if(in->pressed[A])apply_tool(USE_NUTRIENT);if(in->pressed[B])apply_tool(USE_ANTIBIOTIC);
    if(in->pressed[C])g.selected=(g.selected+1)%TOOLS;
    if(in->pressed[D]){static const int use[TOOLS]={USE_LIGHT,USE_PH,USE_SPECIES,USE_CLEAN,USE_CURRENT,USE_QUARANTINE};apply_tool(use[g.selected]);}
    if(g.tick%5==0)simulate();
}

void game_draw(Framebuffer*f){
    int scene=g.mission<6?g.mission:5;scene_frame(f,scene,68);circle(f,320,245,213,0x00517b6a);
    for(int y=0;y<GH;y++)for(int x=0;x<GW;x++){int at=y*GW+x,k=g.cell[at];if(!k)continue;static const uint32_t color[SPECIES]={0,0x005edb72,0x00e6c45b,0x00e65d67,0x00a977d8,0x00d35eb5,0x007bdde6};int px=60+x*13,py=72+y*13;circle(f,px,py,2+k/2,color[k]);if(g.quarantine[at])pixel(f,px+5,py-5,0x00ffffff);}
    rect(f,57+g.cx*13,69+g.cy*13,9,2,0x00ffffff);rect(f,60+g.cx*13,66+g.cy*13,2,9,0x00ffffff);
    rect(f,10,40,g.stable*2,7,0x0065d890);rect(f,10,50,g.toxin*2,5,0x00cf5060);
    for(int k=0;k<6;k++)for(int x=1;x<64;x++){int a=g.history[k][(g.history_at+x-1)&63]/5,b=g.history[k][(g.history_at+x)&63]/5;line(f,500+x*2,450-a,502+x*2,450-b,0x0040a050+k*0x00130d11);}
    if(g.won)rect(f,160,185,320,110,0x00257250);if(g.lost)rect(f,160,185,320,110,0x0071243d);
}
void game_status(char*d,size_t n){
    int m=g.mission<6?g.mission:5;snprintf(d,n,"SAMPLE %d/6 %s  P %d G %d H %d D %d PAR %d S %d  STABLE %d/120  TOXIN %d  TOOLS %d  SELECTED %s%s",
      m+1,sample_name[m],g.count[PRODUCER],g.count[GRAZER],g.count[PREDATOR],g.count[DECOMPOSER],g.count[PARASITE],g.count[SPORE],g.stable,g.toxin,g.budget,tool_name[g.selected],g.won?"  RESEARCH COMPLETE":g.lost?"  CULTURE COLLAPSED":"");
}
Input game_autoplay(int t){
    (void)t;Input in={0};unsigned missing=required[g.mission]&~g.used;
    if(missing&USE_NUTRIENT){in.pressed[A]=1;return in;}if(missing&USE_ANTIBIOTIC){in.pressed[B]=1;return in;}
    static const int use[TOOLS]={USE_LIGHT,USE_PH,USE_SPECIES,USE_CLEAN,USE_CURRENT,USE_QUARANTINE};
    if(missing){int want=0;while(want<TOOLS&&!(missing&(unsigned)use[want]))want++;if(g.selected!=want)in.pressed[C]=1;else in.pressed[D]=1;return in;}
    if(g.toxin>55){int want=TOOL_CLEAN;if(g.selected!=want)in.pressed[C]=1;else in.pressed[D]=1;return in;}
    if(g.count[PRODUCER]<100&&g.budget>0)in.pressed[A]=1;
    return in;
}
Input game_careless(int t){
    (void)t;Input in={0};int bx=-1,by=-1;for(int y=0;y<GH&&by<0;y++)for(int x=0;x<GW;x++)if(g.cell[y*GW+x]==PREDATOR){bx=x;by=y;break;}if(by<0)return in;
    if(g.cx<bx)in.pressed[RIGHT]=1;else if(g.cx>bx)in.pressed[LEFT]=1;else if(g.cy<by)in.pressed[DOWN]=1;else if(g.cy>by)in.pressed[UP]=1;else in.pressed[B]=1;return in;
}
uint32_t game_hash(void){uint32_t h=(uint32_t)(g.mission*3+g.stable*5+g.budget*7+g.toxin*11+g.light*13+g.ph*17+g.used*19+g.tick);for(int k=1;k<SPECIES;k++)h=h*33u+(uint32_t)g.count[k];return h;}
int game_result(void){return g.won?1:g.lost?-1:0;}
void game_audio(int16_t*s,int n){static uint32_t p;int hz=90+g.mission*24+(g.count[PREDATOR]%17),amp=850+g.stable*7;for(int i=0;i<n;i++){p+=(uint32_t)(hz*65536/44100);int v=(p&0x8000)?amp:-amp;if((p&0x1fff)<500)v/=3;s[i*2]=(int16_t)v;s[i*2+1]=(int16_t)(v*3/4);}}
