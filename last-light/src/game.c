#include "game.h"
#include <stdio.h>
#include <string.h>
static struct{uint32_t rng;int night,correct,mistakes,saved,power,timer,genuine,green,tide,horn,call,delay,decided,won,lost,tick;}g;
static uint32_t rnd(void){g.rng=g.rng*1664525u+1013904223u;return g.rng;}
static void contact(void){g.genuine=(int)(rnd()&1);g.green=1;g.tide=1;g.horn=1;g.call=1;g.delay=(int)(rnd()%4);if(!g.genuine){int flaw=(int)(rnd()%5);if(flaw==0)g.green=0;if(flaw==1)g.tide=0;if(flaw==2)g.delay=9;if(flaw==3)g.horn=0;if(flaw==4)g.call=0;}g.timer=600;g.decided=0;}
const char*game_name(void){return "LAST LIGHT BETA";}
const char*game_help(void){return "Z OPEN THE SAFE CHANNEL   X KEEP THE LIGHT DARK   ENTER RESTART";}
const char*game_goal(void){return "GUIDE ONLY CONTACTS WHOSE LIGHTS, TIDE, HORN, CALL SIGN AND RADIO DELAY ARE VALID.";}
void game_init(uint32_t seed){memset(&g,0,sizeof g);g.rng=seed;g.night=1;g.power=70;contact();}
void game_tick(const Input*in){
 if(in->pressed[START]){game_init(g.rng+1);return;}if(g.won||g.lost)return;g.tick++;
 if(!g.decided&&(in->pressed[A]||in->pressed[B])){int guided=in->pressed[A]&&g.power>=12;if(in->pressed[A]&&g.power<12)return;if(guided)g.power-=12;if(guided==g.genuine){g.correct++;if(guided)g.saved++;}else g.mistakes++;g.decided=1;g.timer=90;}
 if(g.timer>0)g.timer--;
 if(g.timer==0){if(!g.decided){g.mistakes++;g.decided=1;g.timer=90;}else{g.night++;g.power+=8;if(g.power>70)g.power=70;if(g.night>7){g.won=g.mistakes<3;g.lost=!g.won;}else contact();}}
}
void game_draw(Framebuffer*f){
 clear(f,0x00060b16);for(int y=280;y<440;y+=16)line(f,0,y,640,y+((y+g.tick/8)%11)-5,0x00142b44);
 for(int i=0;i<70;i++){int x=(i*83+g.tick*3)%680-20;int y=(i*47+g.tick*5)%430;line(f,x,y,x-8,y+16,0x002a3d52);}
 rect(f,70,170,75,270,0x007a735f);rect(f,58,150,100,28,0x003f423d);rect(f,82,110,50,45,0x00c7a85b);circle(f,107,132,34,0x00e2ca72);
 int sx=460,sy=330;rect(f,sx-24,sy,48,8,0x00363c42);line(f,sx-18,sy,sx,sy-15,0x00545c61);line(f,sx,sy-15,sx+18,sy,0x00545c61);
 if(g.green)circle(f,sx-12,sy-10,4,0x004de27b);
 circle(f,sx+12,sy-10,4,0x00e54a55);
 if(g.horn)line(f,sx-20,sy-22,sx+20,sy-22,0x00d7c466);
 if(g.call)rect(f,sx-5,sy-32,10,5,0x0079b8df);
 if(!g.decided){for(int y=126;y<200;y+=8)line(f,134,y,560,y+45,0x002d3b3d);}else rect(f,200,210,240,60,g.genuine?0x00256849:0x00652739);
 if(g.won)rect(f,170,180,300,120,0x00256849);
 if(g.lost)rect(f,170,180,300,120,0x00652739);
}
void game_status(char*d,size_t n){snprintf(d,n,"NIGHT %d/7  POWER %d  GREEN %s  TIDE %s  HORN %s  CALL SIGN %s  DELAY %dS  SAVED %d  ERRORS %d%s",g.night>7?7:g.night,g.power,g.green?"YES":"NO",g.tide?"VALID":"FALSE",g.horn?"CORRECT":"WRONG",g.call?"LISTED":"UNKNOWN",g.delay,g.saved,g.mistakes,g.won?"  HARBOR SAFE":g.lost?"  SOMETHING CAME ASHORE":"");}
Input game_autoplay(int t){(void)t;Input i={0};if(!g.decided){if(g.green&&g.tide&&g.horn&&g.call&&g.delay<5)i.pressed[A]=1;else i.pressed[B]=1;}return i;}
Input game_careless(int t){(void)t;Input i={0};return i;}
uint32_t game_hash(void){return (uint32_t)(g.night*31+g.correct*17+g.mistakes*13+g.saved*19+g.power*23+g.timer+g.tick);}
int game_result(void){return g.won?1:g.lost?-1:0;}
void game_audio(int16_t*s,int n){static uint32_t p,noise=1;int hz=g.horn?73:61;for(int i=0;i<n;i++){p+=(uint32_t)(hz*65536/44100);noise=noise*1103515245u+12345u;int rain=(int)((noise>>20)&511)-255;int horn=(!g.decided&&(g.tick%180)<50)?((p&0x8000)?2600:-2600):0;int v=rain*3+horn;s[i*2]=(int16_t)v;s[i*2+1]=(int16_t)(v-rain);}}
