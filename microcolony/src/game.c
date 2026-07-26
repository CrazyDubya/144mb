#include "game.h"
#include <stdio.h>
#include <string.h>
#define GW 32
#define GH 21
static struct{uint8_t c[GW*GH],next[GW*GH];uint32_t rng;int cx,cy,tick,stable,mission,budget,used_nutrient,used_antibiotic,won,lost,algae,grazers,predators;}g;
static uint32_t rnd(void){g.rng=g.rng*1664525u+1013904223u;return g.rng;}
const char*game_name(void){return "MICROCOLONY BETA";}
const char*game_help(void){return "ARROWS MOVE PIPETTE   Z ADD NUTRIENT   X ANTIBIOTIC   ENTER RESTART";}
const char*game_goal(void){return "STABILIZE POND, RUNOFF AND VENT CULTURES FOR 100 STEPS EACH.";}
static void populate(void){memset(g.c,0,sizeof g.c);memset(g.next,0,sizeof g.next);g.stable=0;g.budget=24;g.used_nutrient=g.used_antibiotic=0;for(int i=0;i<GW*GH;i++){uint32_t r=rnd()%100;if(g.mission==0)g.c[i]=r<24?1:r<30?2:r<32?3:0;else if(g.mission==1)g.c[i]=r<35?1:r<43?2:r<48?3:0;else g.c[i]=r<28?1:r<38?2:r<41?3:0;}}
void game_init(uint32_t seed){memset(&g,0,sizeof g);g.rng=seed;g.cx=GW/2;g.cy=GH/2;populate();}
static int count_near(int x,int y,int kind){int n=0;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int xx=x+dx,yy=y+dy;if((dx||dy)&&xx>=0&&yy>=0&&xx<GW&&yy<GH&&g.c[yy*GW+xx]==kind)n++;}return n;}
static void step(void){
 g.algae=g.grazers=g.predators=0;
 for(int y=0;y<GH;y++)for(int x=0;x<GW;x++){int k=g.c[y*GW+x],a=count_near(x,y,1),b=count_near(x,y,2),p=count_near(x,y,3),v=k;
  if(k==0&&a>=2&&(rnd()%5)<2)v=1;
  else if(k==1&&b>0&&(rnd()%4)==0)v=0;
  else if(k==1&&a>=3&&(rnd()%24)==0)v=2;
  else if(k==2&&p>0&&(rnd()%4)==0)v=0;
  else if(k==2&&a==0&&(rnd()%3)==0)v=0;
  else if(k==2&&b>=3&&(rnd()%30)==0)v=3;
  else if(k==3&&b==0&&(rnd()%4)==0)v=0;
  g.next[y*GW+x]=(uint8_t)v;}
 memcpy(g.c,g.next,sizeof g.c);for(int i=0;i<GW*GH;i++){g.algae+=g.c[i]==1;g.grazers+=g.c[i]==2;g.predators+=g.c[i]==3;}
 int ok=g.mission==0?(g.used_nutrient&&g.algae>=80&&g.grazers>=18&&g.predators>=4):
        g.mission==1?(g.used_antibiotic&&g.algae>=140&&g.algae<=400&&g.grazers>=30&&g.predators<=55):
                      (g.used_nutrient&&g.used_antibiotic&&g.algae>=120&&g.grazers>=60&&g.predators>=15);
 if(ok)g.stable++;else if(g.stable>0)g.stable--;
 if(g.stable>=100){g.mission++;if(g.mission>=3)g.won=1;else populate();}
 if(g.algae==0||g.grazers==0||g.predators==0||g.tick>=12000)g.lost=1;
}
void game_tick(const Input*in){if(in->pressed[START]){game_init(g.rng+1);return;}if(g.won||g.lost)return;g.tick++;if(in->pressed[LEFT]&&g.cx>0)g.cx--;if(in->pressed[RIGHT]&&g.cx<GW-1)g.cx++;if(in->pressed[UP]&&g.cy>0)g.cy--;if(in->pressed[DOWN]&&g.cy<GH-1)g.cy++;if(in->pressed[A]&&g.budget>0){g.budget--;g.used_nutrient=1;for(int y=g.cy-2;y<=g.cy+2;y++)for(int x=g.cx-2;x<=g.cx+2;x++)if(x>=0&&y>=0&&x<GW&&y<GH&&g.c[y*GW+x]==0)g.c[y*GW+x]=1;}if(in->pressed[B]&&g.budget>0){g.budget--;g.used_antibiotic=1;for(int y=g.cy-1;y<=g.cy+1;y++)for(int x=g.cx-1;x<=g.cx+1;x++)if(x>=0&&y>=0&&x<GW&&y<GH&&g.c[y*GW+x]>=3)g.c[y*GW+x]=0;}if(g.tick%5==0)step();}
void game_draw(Framebuffer*f){clear(f,0x000b1712);circle(f,320,240,212,0x00517b6a);for(int y=0;y<GH;y++)for(int x=0;x<GW;x++){int k=g.c[y*GW+x];if(!k)continue;uint32_t c=k==1?0x005edb72:k==2?0x00e6c45b:0x00e65d67;int px=80+x*15,py=75+y*15;circle(f,px,py,k+2,c);if(k==2)line(f,px-6,py,px+6,py,c);if(k==3){line(f,px-7,py-7,px+7,py+7,c);line(f,px+7,py-7,px-7,py+7,c);}}rect(f,77+g.cx*15,72+g.cy*15,7,2,0x00ffffff);rect(f,79+g.cx*15,70+g.cy*15,2,7,0x00ffffff);rect(f,10,40,g.stable*2,6,0x0065d890);if(g.won)rect(f,170,190,300,100,0x00257250);if(g.lost)rect(f,170,190,300,100,0x0071243d);}
void game_status(char*d,size_t n){static const char*name[3]={"POND BALANCE","RUNOFF RECOVERY","VENT DIVERSITY"};int m=g.mission<3?g.mission:2;snprintf(d,n,"SAMPLE %d/3 %s  ALGAE %d  GRAZERS %d  PREDATORS %d  STABILITY %d/100  TOOLS %d%s",m+1,name[m],g.algae,g.grazers,g.predators,g.stable,g.budget,g.won?"  RESEARCH COMPLETE":g.lost?"  CULTURE COLLAPSED":"");}
Input game_autoplay(int t){Input i={0};if(g.mission==0&&!g.used_nutrient)i.pressed[A]=1;if(g.mission==1&&!g.used_antibiotic)i.pressed[B]=1;if(g.mission==2&&!g.used_nutrient)i.pressed[A]=1;else if(g.mission==2&&!g.used_antibiotic)i.pressed[B]=1;else if(g.mission==0&&g.algae<120&&g.budget>0&&t%40==0)i.pressed[A]=1;else if(g.mission==1&&g.predators>45&&g.budget>0&&t%45==0)i.pressed[B]=1;else if(g.mission==2&&g.algae<150&&g.budget>0&&t%45==0)i.pressed[A]=1;if(t%11==0)i.pressed[(t/11)%4]=1;return i;}
Input game_careless(int t){(void)t;Input i={0};int bx=-1,by=-1;for(int y=0;y<GH&&by<0;y++)for(int x=0;x<GW;x++)if(g.c[y*GW+x]==3){bx=x;by=y;break;}if(by<0)return i;if(g.cx<bx)i.pressed[RIGHT]=1;else if(g.cx>bx)i.pressed[LEFT]=1;else if(g.cy<by)i.pressed[DOWN]=1;else if(g.cy>by)i.pressed[UP]=1;else i.pressed[B]=1;return i;}
uint32_t game_hash(void){return (uint32_t)(g.algae*3+g.grazers*5+g.predators*7+g.stable*11+g.mission*13+g.budget*17+g.used_nutrient*19+g.used_antibiotic*23+g.tick);}
int game_result(void){return g.won?1:g.lost?-1:0;}
void game_audio(int16_t*s,int n){static uint32_t p;int hz=110+g.mission*55+(g.predators%12),amp=900+g.stable*8;for(int i=0;i<n;i++){p+=(uint32_t)(hz*65536/44100);int v=(p&0x8000)?amp:-amp;if((p&0x1fff)<500)v/=3;s[i*2]=(int16_t)v;s[i*2+1]=(int16_t)(v*3/4);}}
