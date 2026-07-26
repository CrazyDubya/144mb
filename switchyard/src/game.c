#include "game.h"
#include "scene_pixels.h"
#include <stdio.h>
#include <string.h>

#define MAX_TRAINS 10
enum { LOCAL,EXPRESS,FREIGHT,MAINTENANCE,HERITAGE,EMERGENCY,EMPTY_STOCK,TRAIN_TYPES };
typedef struct{int pos,dir,axis,wait,active,type,speed,length,due,braking;}Train;
static const char*district[8]={"RURAL BRANCH","COMMUTER JUNCTION","FREIGHT CORRIDOR","MOUNTAIN DISTRICT","CENTRAL TERMINAL","DERBY SPECIAL","LANDSLIP RELIEF","WINTER SHUTDOWN"};
static const char*train_name[TRAIN_TYPES]={"LOCAL","EXPRESS","FREIGHT","MAINTENANCE","HERITAGE","EMERGENCY","EMPTY STOCK"};
static const int target[8]={7,9,11,12,14,15,16,18};
static const int interval[8]={175,145,125,115,100,92,86,80};
static struct{
    Train train[MAX_TRAINS];uint32_t rng;
    int priority,hold,platform,paused,delivered,total,shift,delay,connections;
    int missed,unsafe,changes,incident,incident_timer,grade,won,crash,tick,shift_tick;
}g;

static uint32_t rnd(void){g.rng=g.rng*1103515245u+12345u;return g.rng;}
const char*game_name(void){return "SWITCHYARD 1.0 RC1";}
const char*game_help(void){return "Z CHANGE ROUTE  X HOLD ALL  C PLATFORM  V PAUSE/PLAN  H HELP  M MUTE  ENTER RESTART";}
const char*game_goal(void){return "DISPATCH EIGHT DISTRICTS SAFELY. PROTECT CONNECTIONS, MATCH PLATFORMS, AND KEEP PRIORITY TRAINS MOVING.";}

static void spawn(Train*t,int axis,int side,int type){
    static const int lengths[TRAIN_TYPES]={30,34,70,42,38,28,48};
    static const int speeds[TRAIN_TYPES]={2,4,1,2,2,4,2};
    t->axis=axis;t->dir=side?-1:1;t->pos=side?700:-60;t->wait=0;t->active=1;t->type=type;
    t->length=lengths[type];t->speed=speeds[type]+g.shift/4;t->due=g.shift_tick+380+(int)(rnd()%240);t->braking=0;
}
static void begin_shift(void){
    memset(g.train,0,sizeof g.train);g.delivered=0;g.hold=0;g.paused=0;g.shift_tick=0;g.incident=0;g.incident_timer=0;
    spawn(&g.train[0],0,0,LOCAL);spawn(&g.train[1],1,1,g.shift?EXPRESS:LOCAL);
}
void game_init(uint32_t seed){memset(&g,0,sizeof g);g.rng=seed;g.priority=0;g.platform=0;begin_shift();}

static int protected_platform(int type){return type==FREIGHT||type==MAINTENANCE||type==EMPTY_STOCK;}
static int distance_to_crossing(const Train*t){int center=t->axis?240:320,d=t->pos-center;return d<0?-d:d;}
static int route_locked(void){
    for(int i=0;i<MAX_TRAINS;i++)if(g.train[i].active&&g.train[i].axis==g.priority&&distance_to_crossing(&g.train[i])<95+g.train[i].length/2)return 1;
    return 0;
}
static void finish_train(Train*t){
    int lateness=g.shift_tick-t->due;if(lateness>0)g.delay+=lateness;
    if((t->type==LOCAL||t->type==EXPRESS)&&lateness<100)g.connections++;else if(t->type==LOCAL||t->type==EXPRESS)g.missed++;
    if(protected_platform(t->type)!=g.platform){g.delay+=80;g.missed++;}
    t->active=0;g.delivered++;g.total++;
}
void game_tick(const Input*in){
    if(in->pressed[START]){game_init(g.rng+1);return;}if(g.crash||g.won)return;
    if(in->pressed[D])g.paused=!g.paused;
    int locked=route_locked();if(in->pressed[A]&&!locked){g.priority=!g.priority;g.changes++;}else if(in->pressed[A]&&locked)g.unsafe++;
    if(in->pressed[B])g.hold=!g.hold;if(in->pressed[C])g.platform=!g.platform;
    if(g.paused)return;g.tick++;g.shift_tick++;
    if(g.shift>=3&&!g.incident&&g.shift_tick==360+(int)(g.rng%220)){g.incident=1;g.incident_timer=240;}
    if(g.incident_timer>0){g.incident_timer--;if(g.hold)g.incident_timer-=2;if(g.incident_timer<=0)g.incident=0;}
    if(g.shift_tick%interval[g.shift]==0)for(int i=0;i<MAX_TRAINS;i++)if(!g.train[i].active){int type=(int)(rnd()%TRAIN_TYPES);if(g.shift<2&&type>EXPRESS)type=LOCAL;spawn(&g.train[i],(int)(rnd()&1),(int)((rnd()>>3)&1),type);break;}
    for(int i=0;i<MAX_TRAINS;i++){Train*t=&g.train[i];if(!t->active)continue;int distance=distance_to_crossing(t);
        int green=!g.hold&&t->axis==g.priority&&!(g.incident&&t->axis==(g.shift&1));
        if(!green&&distance<100+t->speed*18)t->braking=1;
        if(green)t->braking=0;
        int movement=t->speed;if(t->braking&&distance<55)movement=0;else if(t->braking&&movement>1)movement--;
        if(movement==0){t->wait++;g.delay+=t->type==EMERGENCY?3:t->type==EXPRESS?2:1;if(t->wait>700)g.crash=1;}
        else t->pos+=t->dir*movement;
        if(t->pos>735||t->pos<-95)finish_train(t);
    }
    for(int i=0;i<MAX_TRAINS;i++)for(int j=i+1;j<MAX_TRAINS;j++)if(g.train[i].active&&g.train[j].active&&g.train[i].axis!=g.train[j].axis){
        int di=distance_to_crossing(&g.train[i]),dj=distance_to_crossing(&g.train[j]);
        if(di<18+g.train[i].length/2&&dj<18+g.train[j].length/2)g.crash=1;
    }
    if(!g.crash&&g.delivered>=target[g.shift]){int shift_score=1000-g.delay/8-g.missed*30-g.unsafe*20-g.changes;g.grade+=shift_score>0?shift_score:0;g.shift++;if(g.shift>=8)g.won=1;else begin_shift();}
    if(g.delay>15000||g.missed>60)g.crash=1;
}

void game_draw(Framebuffer*f){
    int sh=g.shift<8?g.shift:7,scene=g.crash?5:(sh<5?sh:sh==7?3:5);scene_frame(f,scene,72);
    for(int x=0;x<640;x+=20){rect(f,x,228,10,4,0x006b7068);rect(f,x,248,10,4,0x006b7068);}
    for(int y=50;y<440;y+=20){rect(f,310,y,4,10,0x006b7068);rect(f,330,y,4,10,0x006b7068);}
    rect(f,0,234,640,12,0x00969b91);rect(f,316,40,12,400,0x00969b91);
    if(sh>=1){line(f,80,234,210,150,0x00969b91);line(f,80,246,210,162,0x00969b91);}if(sh>=2){line(f,430,234,580,330,0x00969b91);line(f,430,246,580,342,0x00969b91);}
    uint32_t ew=g.priority==0&&!g.hold?0x004ed97a:0x00e04f5f,ns=g.priority==1&&!g.hold?0x004ed97a:0x00e04f5f;circle(f,270,216,7,ew);circle(f,344,270,7,ns);
    static const uint32_t color[TRAIN_TYPES]={0x0058a6e7,0x00f2c14e,0x00b86a43,0x0085c786,0x00d991c9,0x00f4f4f4,0x008b78cf};
    for(int i=0;i<MAX_TRAINS;i++){Train*t=&g.train[i];if(!t->active)continue;int len=t->length;uint32_t c=color[t->type];if(t->axis==0){rect(f,t->pos-len/2,231,len,18,c);rect(f,t->pos-10,227,18,5,c);}else{rect(f,313,t->pos-len/2,18,len,c);rect(f,309,t->pos-10,5,18,c);}}
    if(g.incident)for(int i=0;i<8;i++)line(f,250+i*18,180,270+i*18,205,0x00f0ad45);
    if(g.paused)rect(f,210,185,220,70,0x0015222c);if(g.crash)rect(f,180,165,280,130,0x00952038);if(g.won)rect(f,180,165,280,130,0x002a8055);
}
void game_status(char*d,size_t n){
    int sh=g.shift<8?g.shift:7,nearest=-1,best=9999;for(int i=0;i<MAX_TRAINS;i++)if(g.train[i].active&&distance_to_crossing(&g.train[i])<best){best=distance_to_crossing(&g.train[i]);nearest=i;}
    snprintf(d,n,"DISTRICT %d/8 %s  SERVICES %d/%d  DELAY %d  CONNECTIONS %d MISSED %d  ROUTE %s  PLATFORM %s  NEXT %s%s%s%s",
      sh+1,district[sh],g.delivered,target[sh],g.delay,g.connections,g.missed,g.priority?"NORTH-SOUTH":"EAST-WEST",g.platform?"FREIGHT":"PASSENGER",
      nearest>=0?train_name[g.train[nearest].type]:"NONE",g.incident?"  EQUIPMENT FAILURE":g.paused?"  PAUSED":"",g.crash?"  NETWORK FAILED":"",g.won?"  CAMPAIGN COMPLETE":"");
}
Input game_autoplay(int t){
    (void)t;Input in={0};if(g.paused){in.pressed[D]=1;return in;}int want=g.priority,best=10000,platform=g.platform;
    for(int k=0;k<MAX_TRAINS;k++)if(g.train[k].active){int d=distance_to_crossing(&g.train[k]);if(d<best){best=d;want=g.train[k].axis;platform=protected_platform(g.train[k].type);}if(d<30+g.train[k].length/2){want=g.train[k].axis;break;}}
    if(want!=g.priority&&!route_locked())in.pressed[A]=1;else if(platform!=g.platform&&best>115)in.pressed[C]=1;
    return in;
}
Input game_careless(int t){(void)t;Input in={0};return in;}
uint32_t game_hash(void){uint32_t h=(uint32_t)(g.delivered*17+g.total*19+g.shift*23+g.delay*3+g.priority+g.platform*5+g.connections*29+g.missed*31+g.grade*37+g.tick*7+g.crash*101);for(int i=0;i<MAX_TRAINS;i++)h=h*33u+(uint32_t)(g.train[i].pos+g.train[i].active*1009+g.train[i].type*17);return h;}
int game_result(void){return g.won?1:g.crash?-1:0;}
void game_audio(int16_t*s,int n){static uint32_t p,noise=7;int hz=74+g.shift*15,amp=g.hold?700:1700;for(int i=0;i<n;i++){p+=(uint32_t)(hz*65536/44100);noise=noise*1664525u+1013904223u;int beat=((g.tick/15)&3)==0;int v=(p&0x8000)?amp:-amp;if(beat)v+=1100;if(g.incident)v+=(int)((noise>>21)&511)-255;s[i*2]=(int16_t)v;s[i*2+1]=(int16_t)(v+(g.priority?220:-220));}}
