#include "game.h"
#include "scene_pixels.h"
#include <stdio.h>
#include <string.h>

enum { LENS, RADIO, CHART, GENERATOR, JOURNAL, GALLERY, STATIONS };
enum { RULE_GREEN, RULE_DELAY, RULE_TIDE, RULE_REEF, RULE_WRECK, RULES };
typedef struct {
    int genuine, green, tide, horn, call, delay, reef, wreck_name;
    unsigned known;
} Contact;
static struct {
    uint32_t rng;
    Contact contact;
    int rule, night, contact_no, station, power, generator, timer;
    int saved, denied, intruders, errors, evidence, day, day_choice;
    int previous_keeper, ending, won, lost, tick;
} g;

static uint32_t rnd(void){g.rng=g.rng*1664525u+1013904223u;return g.rng;}
static const char *rule_text[]={
    "THE IMITATION CANNOT SHOW A GREEN STARBOARD LIGHT.",
    "THE IMITATION ANSWERS EXACTLY NINE SECONDS LATE.",
    "THE IMITATION ALWAYS REPORTS THE WRONG TIDE.",
    "THE IMITATION APPROACHES THROUGH WIDOW'S REEF.",
    "THE IMITATION USES A NAME FROM THE WRECK REGISTER."
};
static const char *station_name[]={"LENS","RADIO","CHART","GENERATOR","JOURNAL","GALLERY"};

const char*game_name(void){return "LAST LIGHT 1.0 RC1";}
const char*game_help(void){return "LEFT/RIGHT STATION  Z INSPECT/REPAIR  X GUIDE  C DENY  V WAIT  H HELP  M MUTE";}
const char*game_goal(void){return "USE THE KEEPER'S RULE AND WRITTEN EVIDENCE. GUIDE REAL SHIPS; NEVER LIGHT THE IMITATION ASHORE.";}

static void new_contact(void){
    Contact *c=&g.contact;memset(c,0,sizeof *c);
    c->genuine=(int)(rnd()%100)<58;c->green=1;c->tide=1;c->horn=1;c->call=1;
    c->delay=1+(int)(rnd()%4);c->reef=0;c->wreck_name=0;
    if(!c->genuine){
        if(g.rule==RULE_GREEN)c->green=0;
        if(g.rule==RULE_DELAY)c->delay=9;
        if(g.rule==RULE_TIDE)c->tide=0;
        if(g.rule==RULE_REEF)c->reef=1;
        if(g.rule==RULE_WRECK)c->wreck_name=1;
        /* A second imperfection rewards broad investigation without making it required. */
        int flaw=(int)(rnd()%5);
        if(flaw==0)c->horn=0;if(flaw==1)c->call=0;if(flaw==2)c->tide=0;
        if(flaw==3)c->green=0;if(flaw==4)c->delay=9;
    }
    g.timer=900;g.station=LENS;
}

void game_init(uint32_t seed){
    memset(&g,0,sizeof g);g.rng=seed;g.rule=(int)(rnd()%RULES);g.night=1;
    g.power=90;g.generator=100;g.previous_keeper=(int)(rnd()&1);new_contact();
}

static void inspect(void){
    Contact*c=&g.contact;
    if(g.station==GENERATOR){if(g.power>=2){g.power-=2;g.generator+=12;if(g.generator>100)g.generator=100;}return;}
    if(g.power<=0)return;
    int cost=g.station==GALLERY?8:3; if(g.power<cost)return;g.power-=cost;
    if(g.station==LENS)c->known|=1u<<0;
    if(g.station==RADIO)c->known|=(1u<<1)|(1u<<2)|(1u<<3);
    if(g.station==CHART)c->known|=(1u<<4)|(1u<<5);
    if(g.station==JOURNAL)c->known|=(1u<<6)|(1u<<7);
    if(g.station==GALLERY){c->known=255;g.generator-=3;}
    g.evidence++;
}

static void finish_contact(int guide){
    Contact*c=&g.contact;
    if(guide){if(c->genuine)g.saved++;else g.intruders++;}
    else {if(c->genuine)g.denied++;else g.evidence+=2;}
    if((guide!=c->genuine))g.errors++;
    g.contact_no++;
    if(g.intruders>0||g.errors>=4){g.lost=1;g.ending=5;return;}
    if(g.contact_no>=2){g.day=1;g.day_choice=0;g.timer=0;return;}
    new_contact();
}

static int evidence_says_fake(const Contact*c){
    if(g.rule==RULE_GREEN)return (c->known&(1u<<0))&&!c->green;
    if(g.rule==RULE_DELAY)return (c->known&(1u<<1))&&c->delay==9;
    if(g.rule==RULE_TIDE)return (c->known&(1u<<4))&&!c->tide;
    if(g.rule==RULE_REEF)return (c->known&(1u<<5))&&c->reef;
    return (c->known&(1u<<6))&&c->wreck_name;
}

void game_tick(const Input*in){
    if(in->pressed[START]&&(g.won||g.lost)){game_init(g.rng+1);return;}if(g.won||g.lost)return;g.tick++;
    if(g.day){
        if(in->pressed[A]){g.generator+=25;if(g.generator>100)g.generator=100;g.day_choice=1;}
        if(in->pressed[B]){g.previous_keeper=1;g.evidence+=3;g.day_choice=2;}
        if(in->pressed[C]){g.power+=18;if(g.power>100)g.power=100;g.day_choice=3;}
        if(in->pressed[START]&&g.day_choice){g.day=0;g.night++;g.contact_no=0;if(g.night>7){g.won=1;g.ending=g.previous_keeper&&g.saved>=6?4:g.saved>=7?2:1;}else new_contact();}
        return;
    }
    if(in->pressed[LEFT])g.station=(g.station+STATIONS-1)%STATIONS;
    if(in->pressed[RIGHT])g.station=(g.station+1)%STATIONS;
    if(in->pressed[A])inspect();
    if(in->pressed[B])finish_contact(1);
    if(in->pressed[C])finish_contact(0);
    if(in->pressed[D]&&g.timer>90)g.timer-=90;
    if(g.timer>0)g.timer--;
    if(g.tick%240==0){g.generator--;if(g.generator<40&&g.power>0)g.power--;}
    if(g.generator<=0||g.power<=0){g.lost=1;g.ending=0;}
    if(g.timer==0&&!g.day&&!g.won&&!g.lost)finish_contact(0);
}

void game_draw(Framebuffer*f){
    int scene=g.won?4:g.lost?5:g.day?0:(!g.contact.genuine?3:g.night<4?1:2);
    scene_frame(f,scene,g.day?125:82);
    for(int y=275;y<450;y+=17)line(f,0,y,640,y+((y+g.tick/8)%13)-6,0x001d3b50);
    for(int i=0;i<75;i++){int x=(i*83+g.tick*3)%680-20,y=(i*47+g.tick*5)%430;line(f,x,y,x-8,y+16,0x0033485a);}
    if(!g.day&&!g.won&&!g.lost){
        rect(f,40,55,150,28,0x00101820);rect(f,42,57,(g.timer*146)/900,24,0x00c49a4a);
        for(int i=0;i<STATIONS;i++)rect(f,35+i*99,410,88,25,i==g.station?0x00d7b85a:0x00202b34);
        Contact*c=&g.contact;
        if((c->known&1)&&c->green)circle(f,470,315,5,0x004de27b);
        if((c->known&1)&&!c->green)circle(f,470,315,5,0x00d13d51);
        if(evidence_says_fake(c))rect(f,210,92,220,8,0x00c74252);
    }
    if(g.day)rect(f,90,120,460,230,0x00251f18);
    if(g.won)rect(f,150,175,340,125,0x00256849);
    if(g.lost)rect(f,150,175,340,125,0x00652739);
}

void game_status(char*d,size_t n){
    Contact*c=&g.contact;
    if(g.day){snprintf(d,n,"DAWN %d/7  CHOOSE: Z REPAIR GENERATOR  X SEARCH LOGS  C STORE POWER  THEN ENTER%s",g.night,g.day_choice?"  READY":"");return;}
    char clues[150]="";
    if(c->known&1)snprintf(clues+strlen(clues),sizeof clues-strlen(clues)," GREEN %s",c->green?"YES":"NO");
    if(c->known&2)snprintf(clues+strlen(clues),sizeof clues-strlen(clues)," DELAY %dS",c->delay);
    if(c->known&4)snprintf(clues+strlen(clues),sizeof clues-strlen(clues)," HORN %s",c->horn?"RIGHT":"WRONG");
    if(c->known&8)snprintf(clues+strlen(clues),sizeof clues-strlen(clues)," CALL %s",c->call?"LISTED":"UNKNOWN");
    if(c->known&16)snprintf(clues+strlen(clues),sizeof clues-strlen(clues)," TIDE %s",c->tide?"VALID":"FALSE");
    if(c->known&32)snprintf(clues+strlen(clues),sizeof clues-strlen(clues)," REEF %s",c->reef?"WIDOW'S":"CLEAR");
    if(c->known&64)snprintf(clues+strlen(clues),sizeof clues-strlen(clues)," NAME %s",c->wreck_name?"WRECK LOG":"CURRENT");
    snprintf(d,n,"NIGHT %d/7 CONTACT %d/2  %s  POWER %d GEN %d  SAVED %d ERRORS %d  RULE: %s%s%s",
             g.night,g.contact_no+1,station_name[g.station],g.power,g.generator,g.saved,g.errors,
             rule_text[g.rule],clues,g.won?"  HARBOR SAFE":g.lost?"  SOMETHING CAME ASHORE":"");
}

Input game_autoplay(int t){
    (void)t;Input in={0};
    if(g.day){if(!g.day_choice)in.pressed[g.generator<65?A:B]=1;else in.pressed[START]=1;return in;}
    int need=g.rule==RULE_GREEN?LENS:g.rule==RULE_DELAY?RADIO:g.rule==RULE_TIDE||g.rule==RULE_REEF?CHART:JOURNAL;
    if(g.station!=need){in.pressed[RIGHT]=1;return in;}
    Contact*c=&g.contact;unsigned mask=g.rule==RULE_GREEN?1u:g.rule==RULE_DELAY?2u:g.rule==RULE_TIDE?16u:g.rule==RULE_REEF?32u:64u;
    if(!(c->known&mask)){in.pressed[A]=1;return in;}
    in.pressed[evidence_says_fake(c)?C:B]=1;return in;
}
Input game_careless(int t){(void)t;Input in={0};if(!g.day)in.pressed[B]=1;else if(!g.day_choice)in.pressed[B]=1;else in.pressed[START]=1;return in;}
uint32_t game_hash(void){return (uint32_t)(g.rule*3+g.night*5+g.contact_no*7+g.saved*11+g.denied*13+g.intruders*17+g.errors*19+g.power*23+g.generator*29+g.evidence*31+g.day*37+g.ending*41+g.tick);}
int game_result(void){return g.won?1:g.lost?-1:0;}
void game_audio(int16_t*s,int n){
    static uint32_t p,noise=1;int hz=g.contact.horn?73:61;
    for(int i=0;i<n;i++){p+=(uint32_t)(hz*65536/44100);noise=noise*1103515245u+12345u;int rain=(int)((noise>>20)&511)-255;int horn=(!g.day&&(g.tick%180)<50)?((p&0x8000)?3200:-3200):0;int gen=(p&0x1800)?450:-450;int v=rain*3+horn+gen;s[i*2]=(int16_t)v;s[i*2+1]=(int16_t)(v-rain);}
}
