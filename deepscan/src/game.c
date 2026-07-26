#include "game.h"
#include "scene_pixels.h"
#include "story_pixels.h"
#include <stdio.h>
#include <string.h>

#define CONTACTS 18
#define WORLD_H 1800
enum{SURVEY,SALVAGE,ABYSS,CONTRACTS};
enum{SURVEYOR,SALVAGER,DEEP_DIVER,EXPERIMENTAL,VESSELS};
typedef struct{int x,y,vx,vy,type,tagged,awake;}Contact;
static const char*contract_name[CONTRACTS]={"BIOLOGICAL SURVEY","WRECK SALVAGE","ABYSSAL SIGNAL"};
static const char*vessel_name[VESSELS]={"SURVEY VESSEL","SALVAGE VESSEL","DEEP-DIVING VESSEL","EXPERIMENTAL VESSEL"};
static const char*zone_name[6]={"CONTINENTAL SHELF","MIDNIGHT ZONE","WRECK FIELD","THERMAL VENTS","ABYSSAL PLAIN","ASCENT"};
static const char*contact_name[12]={"LANTERN JELLY","GLASS SQUID","RIBBON EEL","BONE CRAB","VENT WORM","MIRROR RAY","CHOIR WHALE","DRIFT COLONY","HUNTER","COIL SERPENT","SHADOW MOUTH","THE LISTENER"};
static struct{
    uint32_t rng;Contact contact[CONTACTS];int x,y,vx,vy,heading;
    int briefing,contract,vessel,battery,oxygen,hull,max_hull,cargo,cargo_max;
    int samples,salvage,blackbox,structure,discoveries,ping,ping_strength,lights,silent;
    int noise,pressure,repair_cooldown,objective,ascending,ending,won,lost,tick;
    int black_x,black_y,structure_x,structure_y;
}g;

static uint32_t rnd(void){g.rng=g.rng*1664525u+1013904223u;return g.rng;}
static int absi(int x){return x<0?-x:x;}
const char*game_name(void){return "DEEPSCAN 1.0 RC1";}
const char*game_help(void){return "ARROWS THRUST/SELECT  Z SONAR STRENGTH  X SILENT  C LIGHTS  V REPAIR  ENTER DEPLOY/RESTART  M MUTE";}
const char*game_goal(void){return "CHOOSE A CONTRACT AND SUBMERSIBLE, DESCEND THROUGH FIVE DEPTH BANDS, COMPLETE THE OBJECTIVE, THEN ASCEND.";}

static void configure_vessel(void){
    static const int hull[VESSELS]={85,115,155,100},cargo[VESSELS]={5,10,6,7},battery[VESSELS]={1150,1000,820,1080};
    g.max_hull=hull[g.vessel];g.hull=g.max_hull;g.cargo_max=cargo[g.vessel];g.battery=battery[g.vessel];g.oxygen=1000;
}
static void deploy(void){
    g.briefing=0;g.x=320;g.y=45;g.vx=g.vy=0;configure_vessel();
}
void game_init(uint32_t seed){
    memset(&g,0,sizeof g);g.rng=seed;g.briefing=1;g.contract=(int)(seed%CONTRACTS);g.vessel=(int)((seed/3)%VESSELS);
    g.black_x=80+(int)(rnd()%480);g.black_y=720+(int)(rnd()%250);g.structure_x=100+(int)(rnd()%440);g.structure_y=1540+(int)(rnd()%180);
    for(int i=0;i<CONTACTS;i++){Contact*c=&g.contact[i];c->x=45+(int)(rnd()%550);c->y=180+i*82+(int)(rnd()%45);if(c->y>1720)c->y=1720;c->type=i%12;c->vx=(int)(rnd()%3)-1;c->vy=(int)(rnd()%3)-1;}
}
static int objective_done(void){
    if(g.contract==SURVEY)return g.samples>=6;
    if(g.contract==SALVAGE)return g.blackbox&&g.salvage>=4;
    return g.structure>=3;
}
static void pulse_contact(Contact*c,int distance){
    if(distance>45+g.ping_strength*52)return;
    if(!c->tagged&&c->type<8&&g.contract==SURVEY){c->tagged=1;g.samples++;g.discoveries++;}
    if(!c->tagged&&g.contract==SALVAGE&&c->type>=3&&c->type<=7&&g.salvage<4&&g.cargo<(g.blackbox?g.cargo_max:g.cargo_max-1)){c->tagged=1;g.salvage++;g.cargo++;g.discoveries++;}
    if(c->type>=8)c->awake=1;
}
void game_tick(const Input*in){
    if(in->pressed[START]&&(g.won||g.lost)){game_init(g.rng+1);return;}
    if(g.won||g.lost)return;g.tick++;
    if(g.briefing){
        if(in->pressed[LEFT])g.contract=(g.contract+CONTRACTS-1)%CONTRACTS;if(in->pressed[RIGHT])g.contract=(g.contract+1)%CONTRACTS;
        if(in->pressed[UP])g.vessel=(g.vessel+VESSELS-1)%VESSELS;if(in->pressed[DOWN])g.vessel=(g.vessel+1)%VESSELS;
        if(in->pressed[START])deploy();return;
    }
    if(in->down[LEFT]){g.vx--;g.heading=-1;}if(in->down[RIGHT]){g.vx++;g.heading=1;}if(in->down[UP])g.vy--;if(in->down[DOWN])g.vy++;
    if(in->pressed[B])g.silent=!g.silent;if(in->pressed[C])g.lights=!g.lights;
    if(in->pressed[A]&&g.battery>=4){g.ping_strength=g.ping_strength%3+1;g.ping=1;g.battery-=g.ping_strength*4;g.noise+=18*g.ping_strength;}
    if(in->pressed[D]&&g.battery>=15&&g.hull<g.max_hull&&g.repair_cooldown==0){g.battery-=15;g.hull+=10;if(g.hull>g.max_hull)g.hull=g.max_hull;g.repair_cooldown=180;g.noise+=20;}
    if(g.repair_cooldown>0)g.repair_cooldown--;
    if(g.silent){g.vx=g.vx*3/4;g.vy=g.vy*3/4;}else if(g.tick%70==0&&g.battery>0)g.battery--;
    if(g.lights&&g.tick%45==0)g.battery--;if(g.lights)g.noise++;
    if(g.vx>4)g.vx=4;if(g.vx<-4)g.vx=-4;if(g.vy>4)g.vy=4;if(g.vy<-4)g.vy=-4;
    g.x+=g.vx;g.y+=g.vy;g.vx=g.vx*7/8;g.vy=g.vy*7/8;
    if(g.x<20){g.x=20;g.hull--;}if(g.x>620){g.x=620;g.hull--;}if(g.y<25)g.y=25;if(g.y>WORLD_H-25){g.y=WORLD_H-25;g.hull--;}
    if(g.ping){g.ping+=5*g.ping_strength;if(g.ping>80+g.ping_strength*75)g.ping=0;}
    g.pressure=g.y/18;int tolerance=g.vessel==DEEP_DIVER?110:g.vessel==SALVAGER?85:75;
    if(g.pressure>tolerance&&g.tick%90==0)g.hull-=1+(g.pressure-tolerance)/30;
    int box_distance=absi(g.black_x-g.x)+absi(g.black_y-g.y);
    if(g.contract==SALVAGE&&!g.blackbox&&g.ping&&box_distance<55&&g.cargo<g.cargo_max){g.blackbox=1;g.cargo++;g.discoveries++;}
    int structure_distance=absi(g.structure_x-g.x)+absi(g.structure_y-g.y);
    if(g.contract==ABYSS&&g.ping&&structure_distance<100&&g.structure<3){g.structure++;g.discoveries++;}
    for(int i=0;i<CONTACTS;i++){Contact*c=&g.contact[i];int d=absi(c->x-g.x)+absi(c->y-g.y);
        if(g.ping)pulse_contact(c,d);
        if(c->type>=8&&(c->awake||g.noise>35)){c->awake=1;c->x+=g.x>c->x?1:-1;c->y+=g.y>c->y?1:-1;}
        else if(g.tick%30==0){c->x+=c->vx;c->y+=c->vy;if(c->x<35||c->x>605)c->vx=-c->vx;if(c->y<90||c->y>1710)c->vy=-c->vy;}
        if(d<18&&c->type>=8){g.hull-=3;c->x=40+(int)(rnd()%560);c->y=300+(int)(rnd()%1400);}
        if(g.lights&&d<34&&c->type<8&&!c->tagged){c->tagged=1;g.discoveries++;if(g.contract==SURVEY)g.samples++;else if(g.contract==SALVAGE&&g.salvage<4&&g.cargo<(g.blackbox?g.cargo_max:g.cargo_max-1)){g.salvage++;g.cargo++;}}
    }
    if(objective_done()){g.objective=1;g.ascending=1;}
    if(g.tick%60==0)g.oxygen--;if(g.noise>0)g.noise-=g.silent?3:1;
    if(g.battery<=0||g.oxygen<=0||g.hull<=0){g.lost=1;g.ending=0;}
    if(g.objective&&g.y<55){g.won=1;g.ending=g.discoveries>=8&&g.hull>g.max_hull/2?3:g.hull>20?2:1;}
}

static void organism(Framebuffer*f,int x,int y,int type,uint32_t color){
    int r=4+(type%4);circle(f,x,y,r,color);if(type%3==0){line(f,x-r-5,y,x+r+5,y,color);line(f,x,y-r-5,x,y+r+5,color);}
    else if(type%3==1)for(int k=-2;k<=2;k++)line(f,x-r,y+k*2,x-r-7,y+k*4,color);
    else{line(f,x-r,y-r,x+r,y+r,color);line(f,x+r,y-r,x-r,y+r,color);}
}
void game_draw(Framebuffer*f){
    if(g.briefing){story_frame(f,0,120);rect(f,70,90,500,300,0x00101822);return;}
    int band=g.y/300;if(band>4)band=4;if(g.won)story_frame(f,2,130);else if(g.ascending)story_frame(f,1,70);else scene_frame(f,band,74);
    int camera=g.y-240;if(camera<0)camera=0;if(camera>WORLD_H-480)camera=WORLD_H-480;
    for(int yy=(camera/100)*100;yy<camera+480;yy+=100)rect(f,0,yy-camera,640,1,0x00234858);
    for(int i=0;i<CONTACTS;i++){Contact*c=&g.contact[i];int sy=c->y-camera;if(sy<20||sy>460)continue;int d=absi(c->x-g.x)+absi(c->y-g.y);int visible=(g.lights&&d<125)||(g.ping&&absi(d-g.ping)<25*g.ping_strength);if(visible)organism(f,c->x,sy,c->type,c->type>=8?0x00ef5368:0x007ee8d4);}
    if(g.contract==SALVAGE&&!g.blackbox){int sy=g.black_y-camera;if(sy>20&&sy<460)rect(f,g.black_x-9,sy-6,18,12,0x00dfae48);}
    if(g.contract==ABYSS){int sy=g.structure_y-camera;if(sy>0&&sy<480){line(f,g.structure_x-35,sy+25,g.structure_x,sy-40,0x006ddcf0);line(f,g.structure_x,sy-40,g.structure_x+35,sy+25,0x006ddcf0);}}
    if(g.ping)circle(f,g.x,g.y-camera,g.ping,0x0049cfe8);
    uint32_t sub=g.silent?0x006bd6bd:g.lights?0x00f5de72:0x00d6b64f;rect(f,g.x-13,g.y-camera-5,26,11,sub);line(f,g.x+13,g.y-camera,g.x+20,g.y-camera-6,sub);line(f,g.x+13,g.y-camera,g.x+20,g.y-camera+6,sub);
    rect(f,10,36,g.battery/6,6,0x00f2ca52);rect(f,10,46,g.oxygen/6,6,0x0056b4e9);rect(f,10,56,g.hull*160/g.max_hull,6,0x00ee5366);
    if(g.won)rect(f,145,175,350,130,0x001b6b4b);if(g.lost)rect(f,145,175,350,130,0x00752038);
}
void game_status(char*d,size_t n){
    if(g.briefing){snprintf(d,n,"CONTRACT: %s  SUBMERSIBLE: %s  ARROWS CHOOSE  ENTER DEPLOY",contract_name[g.contract],vessel_name[g.vessel]);return;}
    int band=g.y/300;if(band>4)band=4;int nearest=0,best=99999;for(int i=0;i<CONTACTS;i++){int q=absi(g.contact[i].x-g.x)+absi(g.contact[i].y-g.y);if(q<best){best=q;nearest=i;}}
    snprintf(d,n,"%s  %s  DEPTH %d  BAT %d O2 %d HULL %d/%d PRESSURE %d NOISE %d CARGO %d/%d  SAMPLES %d SALVAGE %d BOX %s SIGNAL %d/3  HYDROPHONE %s%s",
      contract_name[g.contract],g.ascending?zone_name[5]:zone_name[band],g.y,g.battery,g.oxygen,g.hull,g.max_hull,g.pressure,g.noise,g.cargo,g.cargo_max,g.samples,g.salvage,g.blackbox?"FOUND":"MISSING",g.structure,contact_name[g.contact[nearest].type],g.won?"  EXPEDITION COMPLETE":g.lost?"  SUBMERSIBLE LOST":g.silent?"  SILENT":"");
}
Input game_autoplay(int t){
    (void)t;Input in={0};if(g.briefing){in.pressed[START]=1;return in;}
    if(g.hull<g.max_hull/2&&g.battery>80&&g.repair_cooldown==0){in.pressed[D]=1;return in;}
    int tx=g.x,ty=g.y;
    if(objective_done()){ty=25;tx=320;}
    else if(g.contract==SALVAGE&&!g.blackbox){tx=g.black_x;ty=g.black_y;}
    else if(g.contract==ABYSS){tx=g.structure_x;ty=g.structure_y;}
    else{int best=-1,bd=99999;for(int k=0;k<CONTACTS;k++){Contact*c=&g.contact[k];int useful=c->type<8&&!c->tagged;if(g.contract==SALVAGE)useful=useful&&c->type>=3&&c->type<=7;if(useful){int d=absi(c->x-g.x)+absi(c->y-g.y);if(d<bd){bd=d;best=k;}}}if(best>=0){tx=g.contact[best].x;ty=g.contact[best].y;}}
    int d=absi(tx-g.x)+absi(ty-g.y);if(tx<g.x-5)in.down[LEFT]=1;if(tx>g.x+5)in.down[RIGHT]=1;if(ty<g.y-5)in.down[UP]=1;if(ty>g.y+5)in.down[DOWN]=1;
    if(d<100&&g.ping==0)in.pressed[A]=1;return in;
}
Input game_careless(int t){Input in={0};if(g.briefing)in.pressed[START]=1;else if(t%20==0)in.pressed[A]=1;return in;}
uint32_t game_hash(void){uint32_t h=(uint32_t)(g.x*3+g.y*5+g.battery*7+g.oxygen*11+g.hull*13+g.samples*17+g.salvage*19+g.blackbox*23+g.structure*29+g.discoveries*31+g.contract*37+g.vessel*41+g.tick);for(int i=0;i<CONTACTS;i++)h=h*33u+(uint32_t)(g.contact[i].x+g.contact[i].y*3+g.contact[i].tagged*701);return h;}
int game_result(void){return g.won?1:g.lost?-1:0;}
void game_audio(int16_t*s,int n){static uint32_t p,noise=3;int hz=g.ping?660+g.ping_strength*180:45+g.y/35,amp=g.ping?6200:1700;for(int i=0;i<n;i++){p+=(uint32_t)(hz*65536/44100);noise=noise*1664525u+1013904223u;int v=(p&0x8000)?amp:-amp;if(!g.ping)v+=(int)((noise>>21)&511)-255;int pan=(g.heading*350);s[i*2]=(int16_t)(v-pan);s[i*2+1]=(int16_t)(v+pan);}}
