#ifndef ONBOARDING_WIN_H
#define ONBOARDING_WIN_H
#include "title_pixels.h"
static void beta_title_frame(uint32_t*d){for(int y=0;y<240;y++)for(int x=0;x<320;x++){unsigned v=title_pixels[y*320+x],r=v&224,g=(v&28)<<3,b=(v&3)<<6;uint32_t c=(r<<16)|(g<<8)|b;int at=(y*2)*640+x*2;d[at]=d[at+1]=d[at+640]=d[at+641]=c;}}
static void beta_onboarding(HDC dc,int title){RECT r=title?(RECT){35,18,605,150}:(RECT){55,145,585,325};HBRUSH b=CreateSolidBrush(RGB(8,12,18));FillRect(dc,&r,b);DeleteObject(b);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(245,240,210));int y=title?32:162;TextOutA(dc,60,y,game_name(),(int)strlen(game_name()));RECT goal={60,y+28,580,y+70};DrawTextA(dc,game_goal(),-1,&goal,DT_WORDBREAK);RECT help={60,y+72,580,y+108};if(!title)DrawTextA(dc,game_help(),-1,&help,DT_WORDBREAK);const char*m=title?"PRESS ENTER TO BEGIN":"RELEASE H TO RESUME";TextOutA(dc,60,title?126:300,m,(int)strlen(m));}
#endif
