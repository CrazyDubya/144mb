#include "game.h"
#include "scene_pixels.h"
#include "story_pixels.h"
#include <stdio.h>
#include <string.h>

enum { WAIT,MOVE,AIM,FIRE,INTERACT,SPEAK,DIVE,MELEE,RELOAD,ACTIONS };
enum { CALM,NERVOUS,LOYAL,AGGRESSIVE };
typedef struct{int x,v,owner,live,aimed;}Bullet;
static const char*action_name[ACTIONS]={"WAIT","MOVE","AIM","FIRE","INTERACT","SPEAK","DIVE","MELEE","RELOAD"};
static const char*place[8]={"SALOON","TRAIN ROBBERY","JAILBREAK","CANYON AMBUSH","HOSTAGE EXCHANGE","RANCH DEFENSE","HIGH STREET","COURTHOUSE"};
static const char*temper_name[4]={"CALM","NERVOUS","LOYAL","AGGRESSIVE"};
static struct{
    uint32_t rng;int plan[3],enemy[3],slot,phase,clock,beat,px,ex,php,ehp;
    int paim,eaim,pdive,edive,pammo,eammo,pcover,ecover,door,lamp,smoke;
    int pmorale,emorale,temper,round,stage,reputation,spared,killed,ally;
    int civilians,intro,pspoke,won,lost,tick;Bullet bullet[8];
}g;

static uint32_t rnd(void){g.rng=g.rng*1664525u+1013904223u;return g.rng;}
const char*game_name(void){return "TEN PACES 1.0 RC1";}
const char*game_help(void){return "LEFT/RIGHT SLOT  UP/DOWN ACTION  Z EXECUTE  X CLEAR  C QUICK SPEAK  H HELP  M MUTE";}
const char*game_goal(void){return "PLAN THREE SECONDS AT ONCE. SURVIVE EIGHT SCENES; SURRENDERS AND SURVIVORS SHAPE THE FINALE.";}
const char*game_ending(void){static char text[320];if(g.won)snprintf(text,sizeof text,"THE TOWN REMEMBERS\nREPUTATION: %+d. OUTLAWS SPARED: %d. KILLED: %d. %s\nPRESS ENTER TO RIDE AGAIN.",g.reputation,g.spared,g.killed,g.ally?"FORMER ENEMIES STOOD WITH YOU AT THE COURTHOUSE.":"YOU FACED THE COURTHOUSE ALONE.");else snprintf(text,sizeof text,"YOU ARE DOWN\nREAD STANCE AND TEMPERAMENT, DIVE THROUGH THREATENED BEATS, RELOAD BEFORE THE CYLINDER IS EMPTY, AND USE SPEECH WHEN MORALE BREAKS.\nPRESS ENTER TO TRY AGAIN.");return text;}

static void roll_enemy(void){
    int d=g.ex-g.px;
    for(int k=0;k<3;k++){uint32_t r=rnd()%100;
        if(g.eammo==0)g.enemy[k]=RELOAD;
        else if(d<85)g.enemy[k]=g.temper==NERVOUS?DIVE:MELEE;
        else if(r<(uint32_t)(18+g.stage*3))g.enemy[k]=FIRE;
        else if(r<42)g.enemy[k]=AIM;
        else if(r<58)g.enemy[k]=MOVE;
        else if(r<70)g.enemy[k]=INTERACT;
        else if(r<82)g.enemy[k]=DIVE;
        else g.enemy[k]=g.temper==AGGRESSIVE?FIRE:SPEAK;
    }
    /* Every readable plan contains a threat; careless waiting is never safe. */
    if(g.enemy[0]!=FIRE&&g.enemy[1]!=FIRE&&g.enemy[2]!=FIRE){g.enemy[1]=AIM;g.enemy[2]=FIRE;}
}
static void begin_encounter(void){
    static const int hp[8]={2,2,3,3,3,4,4,5};
    g.px=105;g.ex=535;g.php=4+(g.ally?1:0);g.ehp=hp[g.stage];g.paim=g.eaim=0;
    g.pdive=g.edive=0;g.pammo=6;g.eammo=6;g.pcover=g.ecover=2;g.door=2;g.lamp=1;g.smoke=0;
    g.pmorale=3;g.emorale=2+g.stage/2;g.temper=(int)(rnd()%4);g.phase=0;g.clock=0;g.beat=0;g.slot=0;
    g.civilians=(g.stage==0||g.stage==4||g.stage==6)?2:0;g.intro=120;memset(g.plan,0,sizeof g.plan);memset(g.bullet,0,sizeof g.bullet);roll_enemy();
}
void game_init(uint32_t seed){memset(&g,0,sizeof g);g.rng=seed;g.reputation=1;begin_encounter();}

static void shot(int who){
    int*ammo=who?&g.eammo:&g.pammo,*aim=who?&g.eaim:&g.paim;if(*ammo<=0)return;(*ammo)--;
    for(int i=0;i<8;i++)if(!g.bullet[i].live){g.bullet[i]=(Bullet){who?g.ex:g.px,who?-12:12,who,1,*aim>0};break;}
    if(*aim>0)(*aim)--;
}
static void act(int who,int action){
    int*x=who?&g.ex:&g.px,*aim=who?&g.eaim:&g.paim,*dive=who?&g.edive:&g.pdive,*ammo=who?&g.eammo:&g.pammo;
    int*cover=who?&g.ecover:&g.pcover;
    if(action==MOVE)*x+=who?-62:62;
    else if(action==AIM){(*aim)++;if(*aim>2)*aim=2;if(!who&&g.pmorale<3)g.pmorale++;}
    else if(action==FIRE)shot(who);
    else if(action==INTERACT){if(g.door>0){g.door=0;g.smoke+=8;}else if(*cover<3)(*cover)++;else{g.lamp=!g.lamp;g.smoke+=20;}}
    else if(action==SPEAK){if(who){if(g.reputation<0&&!g.pspoke)g.pmorale--;}else{g.pspoke=1;g.emorale-=g.temper==NERVOUS?2:1;}}
    else if(action==DIVE)*dive=48;
    else if(action==MELEE&&g.ex-g.px<88&&(who?g.pdive:g.edive)==0){if(who){g.php--;g.pmorale--;}else{g.ehp--;g.emorale--;}}
    else if(action==RELOAD)*ammo=6;
    if(*x<70)*x=70;
    if(*x>570)*x=570;
}
static void resolve_bullets(void){
    for(int i=0;i<8;i++){Bullet*b=&g.bullet[i];if(!b->live)continue;int old=b->x;b->x+=b->v;
        if(g.door>0&&((old<320&&b->x>=320)||(old>320&&b->x<=320))){g.door--;g.smoke+=20;if(!b->aimed){b->live=0;continue;}}
        int target=b->owner?g.px:g.ex,dive=b->owner?g.pdive:g.edive;int*cover=b->owner?&g.pcover:&g.ecover;
        if((b->v>0&&b->x>=target)||(b->v<0&&b->x<=target)){
            int hit=b->aimed||((rnd()%100)<(unsigned)(g.lamp?45:28));if(dive>0)hit=0;
            if(hit&&*cover>0){(*cover)--;if(!b->aimed||*cover>0)hit=0;g.smoke+=15;if(b->owner)g.pmorale--;else g.emorale--;}
            if(hit){if(b->owner){g.php--;g.pmorale--;}else{g.ehp--;g.emorale--;}}
            else if(!dive&&(rnd()%5)==0){b->owner=!b->owner;b->v=-b->v;b->aimed=0;continue;}
            else if(g.civilians>0&&(rnd()%7)==0){g.civilians--;g.reputation-=3;}
            b->live=0;
        }else if(b->x<0||b->x>=640)b->live=0;
    }
}
static void finish_encounter(int spared){
    if(spared){g.spared++;g.reputation+=2;}else{g.killed++;g.reputation--;}
    if(g.spared>=2)g.ally=1;
    g.stage++;
    if(g.stage>=8)g.won=1;else begin_encounter();
}
void game_tick(const Input*in){
    if(in->pressed[START]){game_init(g.rng+1);return;}if(g.won||g.lost)return;g.tick++;if(g.intro>0)g.intro--;
    if(g.pdive>0)g.pdive--;
    if(g.edive>0)g.edive--;
    if(g.smoke>0)g.smoke--;
    if(g.phase==0){
        if(in->pressed[LEFT]&&g.slot>0)g.slot--;
        if(in->pressed[RIGHT]&&g.slot<2)g.slot++;
        if(in->pressed[UP])g.plan[g.slot]=(g.plan[g.slot]+1)%ACTIONS;
        if(in->pressed[DOWN])g.plan[g.slot]=(g.plan[g.slot]+ACTIONS-1)%ACTIONS;
        if(in->pressed[B])memset(g.plan,0,sizeof g.plan);
        if(in->pressed[C])g.plan[g.slot]=SPEAK;
        if(in->pressed[A]){g.phase=1;g.clock=0;g.beat=0;g.round++;}
    }else{
        g.clock++;resolve_bullets();
        if(g.clock%60==1&&g.beat<3){g.pspoke=0;act(0,g.plan[g.beat]);act(1,g.enemy[g.beat]);g.beat++;}
        if(g.php<=0||g.pmorale<=0){g.lost=1;return;}
        if(g.ehp<=0){finish_encounter(0);return;}
        if(g.emorale<=0){finish_encounter(1);return;}
        if(g.clock>=180){g.phase=0;g.clock=0;g.slot=0;memset(g.plan,0,sizeof g.plan);roll_enemy();}
    }
}

static void cowboy(Framebuffer*f,int x,int hp,uint32_t c,int dive,int cover){
    int y=dive?375:330;circle(f,x,y-30,9,c);rect(f,x-10,y-21,20,34,c);line(f,x-8,y+13,x-13,y+38,c);line(f,x+8,y+13,x+13,y+38,c);line(f,x-17,y-39,x+17,y-39,c);rect(f,x-10,y-44,20,6,c);
    for(int i=0;i<hp;i++)rect(f,x-14+i*10,y+45,7,5,0x00db3e4c);
    if(cover)rect(f,x-28,y+15,56,20,0x00634b32);
}
void game_draw(Framebuffer*f){
    int scene=g.won?5:g.lost?4:(g.stage<4?g.stage:g.stage==7?5:4);if(g.won){story_frame(f,2,120);return;}if(g.lost){scene_frame(f,4,100);return;}if(g.intro>0)story_frame(f,g.stage?1:0,82);else scene_frame(f,scene,92);
    if(!g.lamp)rect(f,0,0,640,480,0x00101520);
    rect(f,0,390,640,90,0x00614a35);
    for(int i=0;i<g.civilians;i++){int x=275+i*45;circle(f,x,365,6,0x00c3a878);rect(f,x-5,371,10,19,0x006c765c);}
    cowboy(f,g.px,g.php,0x003e7188,g.pdive>0,g.pcover);cowboy(f,g.ex,g.ehp,0x008c3441,g.edive>0,g.ecover);
    for(int i=0;i<8;i++)if(g.bullet[i].live)line(f,g.bullet[i].x-8,g.bullet[i].owner?328:338,g.bullet[i].x+8,g.bullet[i].owner?328:338,0x00fff3b0);
    if(g.door>0)rect(f,308,250,24,140,0x006b4429);
    for(int i=0;i<3;i++){uint32_t c=i==g.slot&&g.phase==0?0x00fff0a5:0x0042352c;rect(f,155+i*112,54,102,34,c);for(int k=0;k<g.plan[i];k++)rect(f,162+k*10,62,6,18,0x00211b18);}
    if(g.phase==0)for(int i=0;i<3;i++)if(g.plan[i]==FIRE)line(f,g.px,320+i*4,g.ex,320+i*4,0x009a8456);
    if(g.phase)rect(f,155,102,g.clock*330/180,6,0x00f3d36a);
    if(g.smoke)for(int i=0;i<20;i++)circle(f,(i*47+g.tick)%640,250+(i*29)%130,4+(i%8),0x005b5b5b);
}
void game_status(char*d,size_t n){
    int st=g.stage<8?g.stage:7;const char*intent=g.enemy[0]==FIRE||g.enemy[1]==FIRE?"HAND NEAR GUN":g.enemy[0]==DIVE?"READY TO DIVE":g.enemy[0]==MOVE?"LEANING FORWARD":"WATCHING YOU";
    snprintf(d,n,"SCENE %d/8 %s  ROUND %d  YOU %dHP %d AMMO  OUTLAW %dHP %s %s  COVER %d/%d DOOR %s LAMP %s CIVILIANS %d  REP %+d SPARED %d  PLAN %s/%s/%s%s",
      st+1,place[st],g.round,g.php,g.pammo,g.ehp,temper_name[g.temper],intent,g.pcover,g.ecover,g.door?"CLOSED":"OPEN",g.lamp?"LIT":"DARK",g.civilians,g.reputation,g.spared,
      action_name[g.plan[0]],action_name[g.plan[1]],action_name[g.plan[2]],g.won?"  THE TOWN REMEMBERS":g.lost?"  YOU ARE DOWN":g.phase?"  EXECUTING":"");
}
Input game_autoplay(int t){
    (void)t;Input in={0};if(g.phase)return in;
    int aim=g.paim,ammo=g.pammo,want=WAIT;
    for(int k=0;k<g.slot;k++){if(g.plan[k]==AIM)aim++;if(g.plan[k]==FIRE){aim=aim?aim-1:0;ammo--;}if(g.plan[k]==RELOAD)ammo=6;}
    if(g.enemy[g.slot]==FIRE||g.enemy[g.slot]==MELEE)want=DIVE;else if(g.enemy[g.slot]==SPEAK&&g.reputation<0)want=SPEAK;else if(g.emorale<=1)want=SPEAK;else if(ammo<=0)want=RELOAD;else if(!aim)want=AIM;else want=FIRE;
    if(g.plan[g.slot]!=want)in.pressed[UP]=1;else if(g.slot<2)in.pressed[RIGHT]=1;else in.pressed[A]=1;return in;
}
Input game_careless(int t){(void)t;Input in={0};if(!g.phase)in.pressed[A]=1;return in;}
uint32_t game_hash(void){uint32_t h=(uint32_t)(g.px*3+g.ex*5+g.php*7+g.ehp*11+g.pammo*13+g.eammo*17+g.round*19+g.stage*23+g.reputation*29+g.spared*31+g.killed*37+g.tick);for(int i=0;i<8;i++)h=h*33u+(uint32_t)(g.bullet[i].x+g.bullet[i].live*701);return h;}
int game_result(void){return g.won?1:g.lost?-1:0;}
void game_audio(int16_t*s,int n){static uint32_t p,noise=9;int hz=82+g.stage*13;for(int i=0;i<n;i++){p+=(uint32_t)(hz*65536/44100);noise=noise*1664525u+1013904223u;int v=(p&0x8000)?900:-900;if(g.phase&&g.clock%60<7)v+=((int)(noise>>18)-8192)/2;if(g.smoke)v+=(int)((noise>>22)&255)-128;s[i*2]=(int16_t)v;s[i*2+1]=(int16_t)(v+(g.reputation>0?180:-180));}}
