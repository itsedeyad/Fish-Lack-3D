#include "ui/HUD.h"
#include "ui/Actions.h"
#include "core/Canvas.h"
#include "ui/UIFont.h"
#include "ui/Artwork.h"
#include "ui/Feedback.h"
#include "render/GLHeaders.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "game/Game.h"
#include "core/World.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
namespace lf { namespace {
constexpr float CW=1600, CH=900;
const int F_TINY=uifont::TINY,F_SMALL=uifont::SMALL,F_MED=uifont::MED,F_BIG=uifont::BIG;
const Color INK(.10f,.19f,.11f), FOREST(.15f,.27f,.14f), GREEN(.24f,.39f,.15f), LIME(.59f,.79f,.31f), CREAM(.96f,.90f,.73f), PAPER(.90f,.83f,.64f), DIM(.29f,.36f,.23f), WHITE(.99f,.96f,.84f), ACCENT(.65f,.85f,.37f), RED(.65f,.17f,.12f), ALERT(1,.65f,.50f);
using Rect=UiRect;
enum class ButtonStyle { Default, Primary, Secondary };
struct Widget { Rect r; int action; std::string label; int icon=-1; bool enabled=true; bool selected=false; ButtonStyle style=ButtonStyle::Default; };
void col(const Color& c,float a=1) {glColor4f(c.r,c.g,c.b,a*c.a);}
void fillRect(float x,float y,float w,float h,const Color& c,float a=1) {col(c,a);glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();}
// Twelve vertices make a stepped pixel frame; geometry stays sharp at any resolution.
void stepped(Rect r,const Color& c) {
    // Three rectangles form the stepped silhouette; no concave GL_POLYGON.
    fillRect(r.x+9,r.y,r.w-18,r.h,c);
    fillRect(r.x,r.y+4.5f,9,r.h-9,c);
    fillRect(r.x+r.w-9,r.y+4.5f,9,r.h-9,c);
}
void frame(Rect r,bool dark=false) {
    stepped({r.x+3,r.y+6,r.w,r.h},INK);
    stepped(r,INK);stepped({r.x+3,r.y+3,r.w-6,r.h-6},PAPER);
    stepped({r.x+6,r.y+6,r.w-12,r.h-12},dark?FOREST:CREAM);
    fillRect(r.x+12,r.y+8,r.w-24,2,dark?LIME:WHITE,.55f);
}
std::string money(int value){std::string s=std::to_string(value);int first=value<0?1:0;for(int i=int(s.size())-3;i>first;i-=3)s.insert(i,1,',');return s;}
void text(float x,float y,const std::string& s,int f,const Color& c=INK) {col(c);uifont::draw(f,x,y,s);}
void textC(float x,float y,const std::string& s,int f,const Color& c=INK,float alpha=1) {col(c,alpha);uifont::draw(f,x-uifont::width(f,s)/2,y,s);}
void textR(float x,float y,const std::string& s,int f,const Color& c=INK) {text(x-uifont::width(f,s),y,s,f,c);}
// Original 5x7 uppercase alphabet, drawn as solid pixel geometry (no runtime font).
void heading(float cx,float y,const std::string& s,float height,const Color& c=WHITE) {
    static const unsigned char glyphs[][7]={
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
        {31,4,4,4,4,4,31},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14},{14,17,17,15,1,1,14},{4,4,4,4,4,0,4},{0,0,0,31,0,0,0}
    };
    float p=height/7, left=cx-(s.size()*6-1)*p/2;
    for(int shadow=1;shadow>=0;--shadow) {
        float x=left+shadow*p*.45f,top=y-height+shadow*p*.45f;
        for(char ch:s) {
            int index=ch>='A'&&ch<='Z'?ch-'A':ch>='0'&&ch<='9'?26+ch-'0':ch=='!'?36:ch=='-'?37:-1;
            if(index>=0)for(int row=0;row<7;++row)for(int column=0;column<5;++column)
                if(glyphs[index][row]&(1<<(4-column)))fillRect(x+column*p,top+row*p,p+.05f,p+.05f,shadow?INK:c);
            x+=6*p;
        }
    }
}

void daylight(const Game& g) {
    float progress=clampf(1-g.timeLeft/g.dayLength,0,1);
    const Atmosphere sky=Atmosphere::at(g.dayProgress());
    col(sky.zenith);glBegin(GL_QUADS);glVertex2f(1134,35);glVertex2f(1464,35);
    col(sky.horizon);glVertex2f(1464,75);glVertex2f(1134,75);glEnd();
    float x=1170+progress*240,y=70-sky.sunDir.y*40;
    for(int row=-11;row<=11;++row) {
        float py=y+row;
        if(py<35 || py>=70)continue;
        float half=std::sqrt(std::max(0.f,121.f-row*row));
        fillRect(x-half,py,half*2,1.1f,Color(1,.83f,.34f));
    }
    fillRect(1134,70,330,7,FOREST);fillRect(1145,66,39,5,FOREST);fillRect(1417,64,47,8,FOREST);
    textC(1299,91,g.roundMode==RoundMode::FullDay && progress<.48f?(progress<.2f?"MORNING":"MIDDAY"):progress<.6f?"AFTERNOON":progress<.9f?"GOLDEN HOUR":"SUNSET",F_TINY);
}
// Tiny hand-authored pixel sprites. Their silhouette is shared across every screen.
void fishIcon(float x,float y,float size,int type=0) {
    const Color base=fishSpec(type).back;float pixel=size/24;
    bool shape[14][24]={};
    for(int row=0;row<14;++row)for(int column=0;column<24;++column) {
        float dx=(column-14.f)/9.f,dy=(row-7.f)/(type==FISH_PIKE?3.2f:type==FISH_CARP?5.4f:4.6f);
        bool body=dx*dx+dy*dy<1;
        bool tail=column>=1 && column<=6 && std::abs(row-7)<=5-column/2;
        bool dorsal=row>=1 && row<=4 && column>=10+row/2 && column<=16;
        bool fin=row>=10 && row<=12 && column>=12 && column<=17-(row-10);
        shape[row][column]=body || tail || dorsal || fin;
    }
    for(int row=0;row<14;++row)for(int column=0;column<24;++column)if(shape[row][column]) {
        bool edge=row==0 || row==13 || column==0 || column==23 || !shape[row-1][column] || !shape[row+1][column] || !shape[row][column-1] || !shape[row][column+1];
        Color c=edge?INK:(row>7?lerpc(base,CREAM,.68f):row<5?scalec(base,.78f):base);
        fillRect(x+column*pixel,y+row*pixel,pixel+.05f,pixel+.05f,c);
    }
    fillRect(x+19*pixel,y+5*pixel,2*pixel,2*pixel,WHITE);fillRect(x+20*pixel,y+5*pixel,pixel,pixel,INK);
    fillRect(x+17*pixel,y+7*pixel,pixel,3*pixel,scalec(base,.65f));
    for(int row=5;row<=9;row+=2)for(int column=9;column<17;column+=3)
        fillRect(x+column*pixel,y+row*pixel,pixel*.7f,pixel*.6f,lerpc(base,INK,.25f));
    if(type==FISH_TROUT)fillRect(x+8*pixel,y+8*pixel,9*pixel,pixel,Color(.85f,.46f,.43f));
    fillRect(x+5*pixel,y+5*pixel,pixel,5*pixel,lerpc(base,CREAM,.4f));
    if(type==FISH_PERCH){for(int fin=0;fin<4;++fin)fillRect(x+(10+fin*2)*pixel,y+(2-fin%2)*pixel,pixel,3*pixel,Color(1,.42f,.12f));}
    if(type==FISH_CATFISH){fillRect(x+21*pixel,y+8*pixel,3*pixel,pixel,CREAM);fillRect(x+21*pixel,y+9*pixel,pixel,3*pixel,CREAM);}
    if(type==FISH_PIKE)fillRect(x+20*pixel,y+7*pixel,4*pixel,2*pixel,lerpc(base,CREAM,.3f));

}

// Geometry-based strokes scale with the icon; no fixed screen-pixel line widths.
void stroke(float ax,float ay,float bx,float by,float width,const Color& c) {
    float length=std::hypot(bx-ax,by-ay);if(length<.001f)return;
    float nx=-(by-ay)/length*width/2,ny=(bx-ax)/length*width/2;
    col(c);glBegin(GL_QUADS);glVertex2f(ax+nx,ay+ny);glVertex2f(bx+nx,by+ny);glVertex2f(bx-nx,by-ny);glVertex2f(ax-nx,ay-ny);glEnd();
}
void disc(float x,float y,float radius,const Color& c) {
    col(c);glBegin(GL_TRIANGLE_FAN);glVertex2f(x,y);
    for(int n=0;n<=20;++n){float a=n*TWO_PI/20;glVertex2f(x+std::cos(a)*radius,y+std::sin(a)*radius);}glEnd();
}
void icon(float x,float y,int kind,float size=32,bool lightSurface=false) {
    glPushMatrix();glTranslatef(x,y,0);glScalef(size/32,size/32,1);
    const Color GOLD(.96f,.66f,.15f),METAL(.62f,.74f,.78f),WOOD(.64f,.36f,.14f);
    if(kind==0)fishIcon(0,6,32);
    else if(kind==1 || kind==11 || kind==12 || kind==15) {
        Color rod=kind==15?GREEN:kind==11?METAL:kind==12?GOLD:WOOD;
        stroke(4,29,25,3,4,INK);stroke(5,28,25,3,2.4f,rod);stroke(7,25,24,4,.65f,WHITE);
        stroke(3,30,8,24,5,INK);stroke(4,29,8,24,2.8f,WOOD);
        disc(9,23,3.7f,INK);disc(9,23,2.5f,METAL);disc(9,23,1.2f,FOREST);
        stroke(10,24,13,26,1,INK);disc(13,26,1.3f,WOOD);
        for(int i=0;i<3;++i){float px=13+i*5,py=18-i*6;disc(px,py,1.7f,INK);disc(px,py,1,CREAM);}
        stroke(26,3,29,22,.65f,CREAM);stroke(29,22,27,25,.9f,METAL);stroke(27,25,25,23,.9f,METAL);
    } else if(kind==2) {
        const int pts[][2]={{9,3},{15,5},{19,10},{18,15},{13,18},{8,19},{6,24},{10,27},{16,26}};
        for(auto& pt:pts) {disc(pt[0]+2,pt[1]+2,3.8f,INK);disc(pt[0]+2,pt[1]+1.5f,2.7f,Color(.88f,.41f,.29f));disc(pt[0]+1,pt[1],1,Color(1,.67f,.42f));}
        fillRect(9,4,1,1,INK);fillRect(11,4,1,1,INK);
    } else if(kind==13) {
        for(int k=0;k<3;++k){stroke(12,11+k*5,5,8+k*7,1.2f,INK);stroke(21,11+k*5,28,8+k*7,1.2f,INK);}
        disc(16,16,8,INK);disc(16,16,6,Color(.66f,.43f,.19f));
        fillRect(10,12,6,10,LIME);fillRect(17,12,5,10,GREEN);stroke(16,9,16,25,1,INK);
        disc(16,7,4,INK);disc(16,7,2.4f,WOOD);stroke(14,4,11,1,1,INK);stroke(18,4,21,1,1,INK);
    } else if(kind==14 || kind==16) {
        stroke(17,20,17,28,1.2f,METAL);stroke(17,28,12,30,1.2f,METAL);stroke(12,30,10,26,1.2f,METAL);
        disc(16,10,9,INK);disc(16,10,7,kind==16?GOLD:Color(.91f,.26f,.17f));
        fillRect(10,5,12,6,WHITE);fillRect(9,10,14,2,INK);fillRect(12,4,2,5,WHITE);
        stroke(16,0,16,3,1,METAL);disc(16,0,1.2f,INK);
    } else if(kind==3) {
        fillRect(2,10,28,20,INK);fillRect(4,12,24,16,WOOD);fillRect(5,14,22,5,Color(.77f,.51f,.22f));
        fillRect(1,7,30,7,INK);fillRect(3,9,26,4,Color(.88f,.66f,.32f));
        fillRect(9,2,14,7,INK);fillRect(12,4,8,4,METAL);fillRect(11,4,10,2,CREAM);
        fillRect(5,12,3,16,METAL);fillRect(24,12,3,16,METAL);fillRect(13,13,6,8,INK);fillRect(14,14,4,5,GOLD);
        fillRect(16,16,1,2,INK);fillRect(9,24,13,1,Color(.37f,.23f,.12f));
    } else if(kind==4) {
        stroke(1,3,5,3,2,CREAM);stroke(5,3,8,24,2,CREAM);
        fillRect(7,8,23,14,INK);fillRect(9,10,19,10,Color(.80f,.61f,.30f));
        for(int n=0;n<4;++n)fillRect(11+n*4,10,1,10,INK);
        fillRect(9,14,19,1,INK);stroke(8,24,27,24,2,CREAM);
        disc(11,28,3,INK);disc(11,28,1.5f,METAL);disc(25,28,3,INK);disc(25,28,1.5f,METAL);
        disc(17,8,3,GOLD);disc(24,6,3,GOLD);fillRect(16,6,1,3,WHITE);fillRect(23,4,1,3,WHITE);
    } else if(kind==5) {
        fillRect(2,9,29,20,INK);fillRect(4,11,25,15,METAL);fillRect(7,5,13,6,INK);fillRect(9,7,9,3,PAPER);
        fillRect(5,12,6,13,FOREST);fillRect(25,12,3,3,WHITE);disc(19,19,8,INK);disc(19,19,6,CREAM);disc(19,19,4,Color(.14f,.37f,.48f));disc(18,18,1.6f,WHITE);
        fillRect(23,7,5,2,GOLD);fillRect(5,24,3,1,DIM);
    } else if(kind==6) {
        for(int n=0;n<8;++n){float a=n*PI/4;stroke(16+std::cos(a)*9,16+std::sin(a)*9,16+std::cos(a)*14,16+std::sin(a)*14,5,INK);}
        disc(16,16,12,INK);
        for(int n=0;n<8;++n){float a=n*PI/4;stroke(16+std::cos(a)*9,16+std::sin(a)*9,16+std::cos(a)*13,16+std::sin(a)*13,3,CREAM);}
        disc(16,16,10,CREAM);disc(16,16,6,INK);disc(16,16,4,METAL);disc(15,15,2,FOREST);
    } else if(kind==7) {
        fillRect(4,3,24,17,INK);fillRect(6,4,20,13,GOLD);fillRect(9,5,3,9,CREAM);
        stroke(5,6,1,6,2,GOLD);stroke(1,6,1,14,2,GOLD);stroke(1,14,8,19,2,GOLD);
        stroke(27,6,31,6,2,GOLD);stroke(31,6,31,14,2,GOLD);stroke(31,14,24,19,2,GOLD);
        fillRect(13,17,7,10,INK);fillRect(15,17,3,9,GOLD);fillRect(8,26,17,5,INK);fillRect(10,27,13,2,GOLD);
        fillRect(19,8,2,6,Color(.77f,.42f,.07f));fillRect(9,30,16,1,WOOD);
    } else if(kind==8) {
        col(INK);glBegin(GL_TRIANGLES);glVertex2f(6,1);glVertex2f(31,16);glVertex2f(6,31);glEnd();
        col(WHITE);glBegin(GL_TRIANGLES);glVertex2f(9,5);glVertex2f(27,16);glVertex2f(9,27);glEnd();
    } else if(kind==9) {
        fillRect(1,5,30,25,INK);fillRect(3,4,12,23,PAPER);fillRect(17,4,12,23,CREAM);
        stroke(16,5,16,29,2,WOOD);
        for(int row=0;row<4;++row){fillRect(5,9+row*4,8,1,DIM);fillRect(20,9+row*4,7,1,DIM);}
        fillRect(22,4,3,11,RED);fillRect(5,5,7,2,WHITE);
    } else if(kind==19) {
        fillRect(3,4,26,26,INK);fillRect(5,6,22,22,CREAM);fillRect(5,6,22,7,GREEN);stroke(10,1,10,8,3,INK);stroke(23,1,23,8,3,INK);
        for(int row=0;row<2;++row)for(int c=0;c<3;++c)fillRect(9+c*6,17+row*6,3,3,GREEN);
    } else if(kind==18) {
        stroke(9,16,9,7,4,CREAM);stroke(9,7,23,7,4,CREAM);stroke(23,7,23,16,4,CREAM);fillRect(4,14,24,17,INK);fillRect(7,16,18,12,PAPER);fillRect(15,19,3,6,INK);
    } else if(kind==17) {
        fillRect(4,2,18,29,INK);fillRect(7,5,12,23,lightSurface?Color(.65f,.44f,.22f):PAPER);Color arrow=lightSurface?INK:CREAM;stroke(15,16,30,16,3,arrow);stroke(25,10,31,16,3,arrow);stroke(31,16,25,22,3,arrow);
    } else {
        disc(16,16,15,INK);disc(16,16,13,GOLD);disc(16,16,10,Color(.77f,.47f,.08f));disc(16,16,8,GOLD);
        stroke(13,10,19,10,1.8f,CREAM);stroke(13,10,13,16,1.8f,CREAM);stroke(13,16,19,16,1.8f,CREAM);stroke(19,16,19,22,1.8f,CREAM);stroke(19,22,13,22,1.8f,CREAM);
        stroke(16,8,16,24,1,CREAM);fillRect(8,5,5,2,WHITE);
    }
    glPopMatrix();
}

void screenRing(float,float,float,const Color&);
void gearIcon(float x,float y,GearCategory category,int id,float size) {
    if(id>=4 && category!=GearCategory::Line){Color colors[]={Color(.27f,.65f,.76f),Color(.71f,.44f,.79f),Color(.91f,.52f,.22f),Color(.47f,.71f,.24f)};disc(x+size*.78f,y+size*.16f,size*.13f,colors[id-4]);textC(x+size*.78f,y+size*.16f+4,std::to_string(id-3),F_TINY,INK);}
    if(category==GearCategory::Rod){icon(x,y,id==3?12:id==2?11:id==1?15:1,size);if(id>=4){Color colors[]={Color(.27f,.65f,.76f),Color(.71f,.44f,.79f),Color(.91f,.52f,.22f),Color(.47f,.71f,.24f)};stroke(x+size*.32f,y+size*.70f,x+size*.75f,y+size*.16f,size*.055f,colors[id-4]);}return;}
    if(category==GearCategory::Bait){
        if(id==4){fillRect(x+size*.15f,y+size*.2f,size*.7f,size*.62f,Color(.68f,.39f,.15f));fillRect(x+size*.23f,y+size*.28f,size*.54f,size*.46f,CREAM);for(int i=0;i<4;++i)disc(x+size*(.34f+(i%2)*.27f),y+size*(.4f+(i/2)*.2f),size*.04f,PAPER);return;}
        if(id==5){for(int i=0;i<6;++i){float a=i*.5f;disc(x+size*(.5f+.24f*std::cos(a)),y+size*(.5f+.24f*std::sin(a)),size*.14f,INK);disc(x+size*(.5f+.24f*std::cos(a)),y+size*(.5f+.24f*std::sin(a)),size*.10f,Color(1,.52f,.28f));}stroke(x+size*.65f,y+size*.3f,x+size*.2f,y+size*.12f,2,RED);return;}
        if(id==6){stroke(x+size*.5f,y+size*.08f,x+size*.5f,y+size*.88f,2,INK);disc(x+size*.4f,y+size*.42f,size*.22f,Color(.63f,.77f,.85f));disc(x+size*.4f,y+size*.42f,size*.1f,WHITE);stroke(x+size*.5f,y+size*.88f,x+size*.76f,y+size*.75f,2,DIM);return;}
        if(id==7){for(int i=0;i<6;++i){float xx=x+size*(.28f+i*.09f),yy=y+size*(.2f+i*.1f);disc(xx,yy,size*.13f,INK);disc(xx,yy,size*.1f,i%2?LIME:Color(.45f,.76f,.87f));}return;}
        if(id==3){fishIcon(x,y+size*.18f,size,FISH_BLUE);return;}
        if(id==2){
            for(int i=0;i<3;++i){float dx=x+size*(.35f+i*.16f),dy=y+size*(.4f+(i%2)*.2f);disc(dx,dy,size*.19f,INK);disc(dx,dy,size*.16f,Color(.89f,.69f,.36f));disc(dx-size*.04f,dy-size*.05f,size*.065f,CREAM);}
            stroke(x+size*.65f,y+size*.7f,x+size*.7f,y+size*.9f,1.5f,DIM);return;
        }
        icon(x,y,id==1?13:2,size);return;
    }
    glPushMatrix();glTranslatef(x,y,0);glScalef(size/32,size/32,1);
    if(category==GearCategory::Float){
        const Color paint[]={RED,Color(.90f,.64f,.15f),LIME,Color(.23f,.55f,.80f),Color(.76f,.84f,.86f),Color(.64f,.40f,.77f),Color(.96f,.46f,.16f),Color(.25f,.69f,.66f)};
        stroke(16,0,16,5,2,INK);float r=id==4?6.f:9.f;
        disc(16,12,r+2,INK);disc(16,12,r,paint[id]);fillRect(16-r+1,7,r*2-2,5,WHITE);stroke(16-r,12,16+r,12,2,INK);
        stroke(16,22,16,29,2,INK);stroke(16,29,10,30,2,INK);stroke(10,30,8,26,2,INK);
        if(id==2)screenRing(16,12,13,GREEN);
        if(id==3 || id==6){stroke(9,16,23,16,2,WHITE);stroke(10,19,22,19,2,INK);}
        if(id==5){disc(16,23,5,INK);disc(16,23,3,paint[id]);}
        if(id==7){stroke(4,3,7,6,2,GREEN);stroke(28,3,25,6,2,GREEN);}
    }else if(category==GearCategory::Reel){
        const Color metals[]={Color(.70f,.66f,.46f),Color(.53f,.70f,.78f),Color(.93f,.67f,.19f),Color(.70f,.59f,.91f),Color(.39f,.68f,.64f),Color(.64f,.48f,.33f),Color(.84f,.46f,.23f),Color(.36f,.51f,.73f)};
        fillRect(14,2,4,10,INK);disc(16,17,12,INK);disc(16,17,9,metals[id]);
        screenRing(16,17,7,INK);disc(16,17,3,INK);disc(16,17,1.5f,CREAM);
        int spokes=id>=4?4:3;for(int i=0;i<spokes;++i){float a=i*TWO_PI/spokes;stroke(16+std::cos(a)*3,17+std::sin(a)*3,16+std::cos(a)*7,17+std::sin(a)*7,1.5f,INK);}
        stroke(24,18,29,24,2,INK);disc(29,25,3,GREEN);
    }else {
        const Color wire[]={Color(.47f,.54f,.29f),Color(.18f,.56f,.57f),Color(.31f,.58f,.83f),Color(.62f,.43f,.78f),Color(.88f,.83f,.62f),Color(.62f,.83f,.88f),Color(.93f,.47f,.15f),Color(.22f,.35f,.60f)};
        // Filled rings and geometry strokes scale with the UI canvas. GL line
        // widths do not, which made the old pale GL_LINE_LOOP icons disappear.
        disc(15,14,13,INK);disc(15,14,11,wire[id]);
        screenRing(15,14,9,id==7?CREAM:INK);disc(15,14,5,INK);disc(15,14,3,CREAM);
        stroke(24,22,29,28,2,INK);stroke(29,28,24,30,2,INK);
        if(id==1 || id==7){stroke(3,27,18,27,2,INK);stroke(5,30,18,30,2,INK);}
        if(id==2)for(int k=0;k<3;++k)fillRect(7+k*6,3,3,3,INK);
        if(id==3){stroke(5,6,9,10,2,CREAM);stroke(21,19,25,23,2,CREAM);}
        if(id==5){stroke(5,8,9,4,2,WHITE);stroke(22,20,25,17,2,WHITE);}
        if(id==6){stroke(5,24,8,28,2,INK);stroke(8,28,11,24,2,INK);}
    }
    glPopMatrix();
}

void mapIcon(float x,float y,float size,MapId id) {
    glPushMatrix();glTranslatef(x,y,0);glScalef(size/100,size/100,1);
    fillRect(0,0,100,70,Color(.69f,.80f,.86f));fillRect(0,43,100,27,world::mapSpec(id).deep);
    if(id==MapId::Highland){col(Color(.38f,.44f,.49f));glBegin(GL_TRIANGLES);glVertex2f(0,44);glVertex2f(32,4);glVertex2f(63,44);glVertex2f(42,44);glVertex2f(73,0);glVertex2f(100,44);glEnd();col(WHITE);glBegin(GL_TRIANGLES);glVertex2f(23,16);glVertex2f(32,4);glVertex2f(42,17);glVertex2f(63,14);glVertex2f(73,0);glVertex2f(82,14);glEnd();}
    else for(int i=0;i<4;++i){float px=10+i*23;fillRect(px+5,17,4,29,Color(.38f,.25f,.12f));fillRect(px,14,16,18,id==MapId::Marsh?Color(.40f,.53f,.18f):GREEN);if(id==MapId::Forest)fillRect(px+3,7,10,11,FOREST);else for(int k=0;k<3;++k)fillRect(px+k*6,28,3,12+k*3,GREEN);}
    fillRect(32,58,38,3,CREAM);fillRect(12,51,14,2,CREAM);glPopMatrix();
}
Rect itemCard(int slot){return {float(286+(slot%2)*320),float(314+(slot/2)*150),300,134};}
float uiClock=0;
void button(const Widget& b,int hover,int pressed) {
    bool lit=b.selected || b.action==hover;frame(b.r,true);
    Color base=b.enabled?(lit?GREEN:FOREST):DIM;
    bool paperButton=b.style==ButtonStyle::Secondary && b.enabled;
    if(b.style==ButtonStyle::Primary && b.enabled)base=lit?Color(.29f,.46f,.18f):GREEN;
    if(paperButton)base=lit?WHITE:PAPER;
    stepped({b.r.x+6,b.r.y+6,b.r.w-12,b.r.h-12},base);
    if(lit && b.enabled) {fillRect(b.r.x+14,b.r.y+10,(b.r.w-28)*(.92f+.08f*std::sin(uiClock*6)),3,LIME);}
    float dy=b.action==pressed?2:0;
    if(b.icon>=0) {
        float iconSize=b.label.empty()?48.f:36.f;
        float ix=b.r.x+(b.label.empty()?(b.r.w-iconSize)/2:16),iy=b.r.y+(b.r.h-iconSize)/2+dy;
        icon(ix,iy,b.icon,iconSize,paperButton);
    }
    textC(b.r.x+b.r.w/2+(b.icon>=0?14:0),b.r.y+b.r.h/2+7+dy,b.label,F_MED,b.enabled?(paperButton?INK:WHITE):PAPER);
}
std::vector<Widget> widgets(const Game& g) {
    std::vector<Widget> v;
    if(g.overlay==Overlay::NoBait || g.overlay==Overlay::Secret){v.push_back({{620,650,360,56},ui::Close,g.overlay==Overlay::NoBait?"View Results":"Continue",8});return v;}
    if(g.overlay==Overlay::DailyOrder){v.push_back({{1310,150,52,48},ui::Close,"X"});v.push_back({{470,650,310,58},ui::AcceptTrip,"Accept",8,true,false,ButtonStyle::Primary});v.push_back({{820,650,310,58},ui::SkipTrip,"Skip",-1,true,false,ButtonStyle::Secondary});return v;}
    if(g.showHelp){v.push_back({{1310,150,52,48},ui::Close,"X"});return v;}
    if(g.overlay!=Overlay::None) {
        v.push_back({g.overlay==Overlay::Settings?Rect{1208,196,52,48}:Rect{1310,150,52,48},ui::Close,"X"});
        if(g.overlay==Overlay::Credits){v.push_back({{722,258,156,100},ui::SecretFish,""});v.push_back({{650,500,330,48},ui::CreatorProfile,""});v.push_back({{620,730,360,48},ui::Close,"Back"});return v;}
        if(g.overlay==Overlay::Settings){
            for(int i=0;i<5;++i){float y=265.f+i*74;v.push_back({{954,y,52,42},ui::SettingDecrease+i*2,"<"});v.push_back({{1170,y,52,42},501+i*2,">"});}
            v.push_back({{soundSlider.x-6,soundSlider.y-10,soundSlider.w+12,24},ui::SoundSlider,"",-1});
        } else if(g.overlay==Overlay::TripSetup){
            for(int i=0;i<3;++i)v.push_back({{float(620+i*230),264,214,54},ui::Mode+i,roundModeName(RoundMode(i)),-1,true,int(g.selectedMode)==i});
            bool custom=g.selectedMode!=RoundMode::Hardcore;
            v.push_back({{800,455,52,42},ui::DifficultyDown,"<",-1,custom});v.push_back({{1218,455,52,42},ui::DifficultyUp,">",-1,custom});
            for(int i=0;i<3;++i)v.push_back({{286,float(254+i*136),300,120},ui::Location+i,"",-1,true,int(g.selectedMap)==i});
            v.push_back({{286,730,220,56},ui::TripInventory,"Inventory",3});v.push_back({{522,730,220,56},ui::TripShop,"Shop",4});v.push_back({{946,730,360,56},ui::Start,"Start Fishing",8});
        } else {
            int tabs=g.overlay==Overlay::Shop?6:5;float step=g.overlay==Overlay::Shop?178.f:214.f,width=step-12;
            for(int i=0;i<tabs;++i)v.push_back({{270+i*step,246,width,44},ui::Category+i,i==5?"Locations":categoryName(GearCategory(i)),-1,true,(g.locationsTab?5:int(g.gearCategory))==i});
            if(g.locationsTab)for(int i=0;i<3;++i){const auto& m=world::mapSpec(MapId(i));v.push_back({{float(296+i*340),590,300,52},ui::BuyLocation+i,g.session.maps[i]?"Unlocked":std::to_string(m.price)+" coins",-1,!g.session.maps[i] && g.session.coins>=m.price});}
            if(!g.locationsTab){
                auto items=g.visibleItems();int page=g.overlayIndex/4;for(int slot=page*4;slot<std::min(int(items.size()),page*4+4);++slot){int id=items[slot];v.push_back({itemCard(slot%4),810+id,"",-1,true,g.overlayIndex==slot});}
                if(items.size()>4){v.push_back({{450,623,52,36},ui::PreviousPage,"<",-1,page>0});v.push_back({{674,623,52,36},ui::NextPage,">",-1,page==0});}
                if(!items.empty()) {
                    int id=items[std::min(g.overlayIndex,int(items.size())-1)];bool shop=g.overlay==Overlay::Shop,bait=g.gearCategory==GearCategory::Bait;
                    bool enabled=shop?((bait || !g.session.owns(g.gearCategory,id)) && g.session.coins>=gearSpec(g.gearCategory,id).price*(bait?g.purchaseQuantity:1)):g.canEquip();
                    v.push_back({{964,626,324,48},shop?ui::Buy:ui::Equip,shop?(g.session.owns(g.gearCategory,id) && !bait?"Owned":"Buy "+std::to_string(gearSpec(g.gearCategory,id).price*(bait?g.purchaseQuantity:1))+" coins"):g.session.equipped(g.gearCategory)==id?"Equipped":"Equip",-1,enabled});
                    if(shop && bait){v.push_back({{964,568,72,42},ui::QuantityDown,"-1"});for(int i=0;i<3;++i)v.push_back({{float(1048+i*84),568,72,42},ui::AddOne+i,"+"+std::to_string(i==0?1:i==1?5:10)});}
                }
            }
            if(g.overlay==Overlay::Inventory && g.ledger.active && !g.hasBait())v.push_back({{946,684,360,44},ui::EndDay,"End Day",8});
            if(g.overlay==Overlay::Shop)v.push_back({{286,750,220,42},ui::OpenInventory,"Inventory",3});
            v.push_back({{1006,750,300,42},ui::Close,"Back"});
        }
        return v;
    }
    if(g.catchPending && g.state==GameState::FishCaught){v.push_back({{510,604,270,64},ui::Keep,"KEEP",3,true,g.menuIndex==0});v.push_back({{820,604,270,64},ui::Release,"RELEASE",0,true,g.menuIndex==1,ButtonStyle::Secondary});return v;}
    if(g.menuVisible()){
        for(int i=0;i<g.menuItemCount();++i){Rect r;
            if(g.state==GameState::MainMenu){r=i==1?Rect{580,414,212,60}:i==2?Rect{808,414,212,60}:Rect{580,float(i==0?342:486+(i-3)*72),440,60};}
            else if(g.state==GameState::Paused)r={580,float(350+i*74),440,60};
            else r={float(340+i*500),738,420,58};
            int icons[]={8,3,4,6,9,17};int resultIcons[]={8,17};v.push_back({r,ui::Menu+i,g.menuItemLabel(i),g.state==GameState::MainMenu?icons[i]:g.state==GameState::Paused?(i==1?9:i==2?17:8):resultIcons[i],true,g.menuIndex==i,i==0?ButtonStyle::Primary:((g.state==GameState::MainMenu && i==5) || (g.state==GameState::Paused && i==2) || ((g.state==GameState::Victory || g.state==GameState::GameOver) && i==1))?ButtonStyle::Secondary:ButtonStyle::Default});
        }return v;
    }
    v.push_back({{1508,24,64,64},ui::Settings,"",6});v.push_back({{1508,104,64,64},ui::Pause,"",17});
    v.push_back({{24,810,100,60},ui::Inventory,"",3});return v;
}

void meter(float x,float y,float value,const std::string& label,bool danger=false) {
    frame({x,y,330,66},true);text(x+18,y+25,label,F_SMALL,WHITE);
    fillRect(x+18,y+39,294,13,INK);fillRect(x+20,y+41,290*clampf(value,0,1),9,danger?RED:LIME);
}
void tensionMeter(const Game& g) {
    const float x=24,y=430,width=380,track=344;
    float value=clampf(g.fishing.tension(),0,1);
    auto band=feedback::tensionBand(value);
    const Color amber(.96f,.70f,.23f);
    frame({x,y,width,100},true);
    text(x+18,y+25,"LINE TENSION",F_SMALL,WHITE);
    const char* state=band==feedback::TensionBand::Low?"LOW":band==feedback::TensionBand::Safe?"SAFE":band==feedback::TensionBand::Warning?"WARNING":"DANGER";
    textR(x+width-18,y+25,state,F_SMALL,band==feedback::TensionBand::Danger?ALERT:band==feedback::TensionBand::Safe?LIME:amber);
    float edges[]={0,settings::tensionLow,settings::tensionWarning,settings::tensionDanger,1};
    Color colors[]={amber,GREEN,amber,RED};
    fillRect(x+16,y+36,track+4,20,INK);
    for(int zone=0;zone<4;++zone)fillRect(x+18+track*edges[zone],y+38,track*(edges[zone+1]-edges[zone])-1,16,colors[zone]);
    float needle=x+18+track*value;
    fillRect(needle-3,y+33,6,25,INK);fillRect(needle-1,y+34,2,23,WHITE);
    text(x+18,y+70,"LOW",F_TINY,WHITE);text(x+82,y+70,"SAFE",F_TINY,WHITE);
    text(x+225,y+70,"WARN",F_TINY,WHITE);textR(x+width-18,y+70,"DANGER",F_TINY,WHITE);
    text(x+18,y+91,"QUALITY "+std::to_string(g.fishing.fishQuality())+"%",F_SMALL,WHITE);fillRect(x+180,y+79,180,13,INK);fillRect(x+182,y+81,176*g.fishing.fishQuality()/100.f,9,g.fishing.fishQuality()>=75?LIME:amber);
    if(band==feedback::TensionBand::Danger) {
        float pulse=.45f+.20f*std::sin(g.worldTime*TWO_PI*settings::alertPulseHz);
        fillRect(x+13,y+7,width-26,3,ALERT,pulse);
    }
}
// Project once into the same fitted canvas used by drawing and mouse hit tests.
bool canvasPoint(const Renderer& renderer,const Canvas& canvas,const Vec3& point,float& x,float& y) {
    float sx=0,sy=0;
    if(!renderer.project(point,sx,sy))return false;
    canvas.point(sx,sy,x,y);
    return x>=24 && x<=CW-24 && y>=112 && y<=CH-160;
}
void screenRing(float x,float y,float radius,const Color& color) {
    col(color);glBegin(GL_QUAD_STRIP);
    for(int n=0;n<=32;++n){float a=n*TWO_PI/32;
        glVertex2f(x+std::cos(a)*(radius-1),y+std::sin(a)*(radius-1));
        glVertex2f(x+std::cos(a)*(radius+1),y+std::sin(a)*(radius+1));
    }glEnd();
}
void worldFeedback(const Game& g,const Renderer& renderer,const Canvas& canvas) {
    if(g.uiBlocked())return;
    const FishingSystem& fishing=g.fishing;
    float x=0,y=0;
    if(g.state==GameState::Aiming || g.state==GameState::Charging) {
        float power=g.state==GameState::Charging?-1.f:settings::aimPreviewPower;
        Vec3 landing=fishing.predictedLanding(power);
        landing.y=world::waveHeight(landing.x,landing.z,g.worldTime)+.06f;
        if(!canvasPoint(renderer,canvas,landing,x,y)) {
            float dx=0,dy=0;renderer.screenDirection(landing,dx,dy);float factor=std::min(540.f/std::max(.001f,std::fabs(dx)),300.f/std::max(.001f,std::fabs(dy)));
            x=800+dx*factor;y=450+dy*factor;float length=std::hypot(dx,dy);dx/=length;dy/=length;
            stroke(x-dx*24,y-dy*24,x,y,8,INK);stroke(x-dx*24,y-dy*24,x,y,4,ACCENT);
            col(ACCENT);glBegin(GL_TRIANGLES);glVertex2f(x+dx*12,y+dy*12);glVertex2f(x-dx*8-dy*10,y-dy*8+dx*10);glVertex2f(x-dx*8+dy*10,y-dy*8-dx*10);glEnd();return;
        }
        screenRing(x,y,settings::aimReticleRadius+4,INK);screenRing(x,y,settings::aimReticleRadius+2,WHITE);
        screenRing(x,y,settings::aimReticleRadius-4,Color(1,.79f,.22f));
        stroke(x-6,y,x+6,y,3,INK);stroke(x,y-6,x,y+6,3,INK);
        stroke(x-5,y,x+5,y,1.5f,WHITE);stroke(x,y-5,x,y+5,1.5f,WHITE);
        return;
    }
    bool bite=g.state==GameState::BiteDetected,nibble=feedback::nibbling(g);
    if(!bite && !nibble)return;
    if(canvasPoint(renderer,canvas,fishing.bobberPos(),x,y)) {
        float pulse=.5f+.5f*std::sin(g.worldTime*TWO_PI*settings::alertPulseHz);
        screenRing(x,y,(bite?20.f:14.f)+pulse*4,bite?Color(1,.78f,.25f):CREAM);
        if(bite)icon(x-17,y-64-pulse*5,14,34);
    } else if(bite){icon(54,240,14,48);}

}
void wrapped(float x,float y,float width,const std::string& value,int font,const Color& color) {
    std::istringstream words(value);std::string word,line;int rows=0;
    while(words>>word) {
        auto next=line.empty()?word:line+" "+word;
        if(!line.empty() && uifont::width(font,next)>width) {
            text(x,y,line,font,color);y+=24;if(++rows==2)return;line=word;
        } else line=next;
    }
    if(!line.empty())text(x,y,line,font,color);
}

void logo(Rect r,bool big=false) {
    frame(r,true);fishIcon(r.x+20,r.y+(big?46:18),big?100:52);
    heading(r.x+r.w*(big?.57f:.64f),r.y+(big?100:43),"FISH LACK 3D",big?52:20);
}

void gameplay(const Game& g,const Camera&) {
    logo({24,24,300,64});frame({24,106,220,54});icon(40,118,0,30);
    text(84,142,"Catch "+std::to_string(g.caught)+(g.roundMode==RoundMode::Classic?" / "+std::to_string(g.target):""),F_MED);

    frame({1110,24,376,98});daylight(g);frame({1280,140,206,54});icon(1294,151,10,30);text(1336,176,money(g.session.coins),F_MED);
    frame({1110,140,154,54});icon(1123,151,19,30);text(1164,176,"Day "+std::to_string(g.session.daysStarted),F_MED);
    if(g.activeState()==GameState::Charging){
        meter(24,260,g.fishing.power(),"CAST POWER "+std::to_string(int(g.fishing.power()*100))+"%");
        frame({24,332,330,42},true);text(42,360,std::to_string(int(distXZ(world::anglerPosition(),g.fishing.predictedLanding())))+" metres",F_MED,WHITE);
    }
    if(g.activeState()==GameState::Reeling || g.activeState()==GameState::FishHooked)tensionMeter(g);
    if(g.session.baitCount[g.session.bait]<=3){gearIcon(26,748,GearCategory::Bait,g.session.bait,38);text(78,779,"x"+std::to_string(g.session.baitCount[g.session.bait]),F_MED,Color(1,.67f,.26f));}
}

void drawDailyOrder(const Game& g) {
    auto order=dailyOrder(g.session.daysStarted+1);
    heading(800,258,"DAILY ORDER",30,GREEN);fishIcon(710,310,180,order.type);
    textC(800,470,"Keep "+std::to_string(order.quantity)+" healthy "+fishSpec(order.type).name,F_BIG);
    textC(800,520,"Bonus: "+std::to_string(order.reward)+" coins",F_MED,GREEN);
    textC(800,565,"Optional. Earn the sale price and the bonus together.",F_SMALL);
    textC(800,598,"Suspicious fish may be poisonous. Choose carefully.",F_SMALL);
}

void drawSpecialNotice(const Game& g) {
    if(g.overlay==Overlay::NoBait){icon(764,290,2,72);heading(800,440,"OUT OF BAIT",38,GREEN);textC(800,510,"Your fishing day has ended.",F_BIG);textC(800,560,"Your catches have been settled. Restock before the next trip.",F_SMALL);}
    else {icon(760,290,10,80);heading(800,430,"1 000 000",54,GREEN);uiart::secretMessage(350,480,900,108);}
}

void drawCredits() {
    fishIcon(722,258,156,FISH_BLUE);heading(800,430,"FISH LACK 3D",40,GREEN);
    textC(800,485,"Created by ED Eyad",F_BIG);uiart::githubMark(661,504,30);textC(820,529,"GitHub: itsedeyad",F_MED,GREEN);
    textC(800,582,"Computer Graphics Course Project",F_MED);
    textC(800,633,"A 3D lake fishing adventure built with C++, OpenGL and FreeGLUT.",F_SMALL);
    textC(800,670,"Aim, cast and reel. Manage your supplies and grow your fishing kit.",F_SMALL);
}

void drawSettings(const Game& g) {
    const char* names[]={"Mouse sensitivity","Brightness","Sound Effects","Camera","Fullscreen"};
    const char* cameras[]={"Angler","Float","Dock","Map"};
    const std::string values[]={
        std::to_string(int(g.preferences.sensitivity*100))+"%",
        std::to_string(int(g.preferences.brightness*100))+"%",
        std::to_string(int(g.preferences.sfx*100))+"%",
        cameras[g.preferences.camera],
        g.preferences.fullscreen?"On":"Off"
    };
    for(int row=0;row<5;++row) {
        const float y=265.f+row*74;
        if(g.overlayIndex==row)fillRect(352,y-3,896,56,PAPER);
        text(374,y+29,names[row],F_MED);
        textC(1092,y+25,values[row],F_MED);
        if(row<3) {
            const float value=row==0?(g.preferences.sensitivity-.3f)/1.7f:
                row==1?(g.preferences.brightness-.65f)/.6f:g.preferences.sfx;
            fillRect(soundSlider.x,y+34,soundSlider.w,4,PAPER);
            fillRect(soundSlider.x,y+34,soundSlider.w*value,4,GREEN);
            if(row==2)disc(soundSlider.x+soundSlider.w*value,y+36,7,GREEN);
        }
    }
    if(!g.saveStatus.empty()) {
        text(374,676,"Progress: "+g.saveStatus+" / progress.save",F_SMALL,g.saveStatus=="Saved"?GREEN:RED);
    }
}

void drawTripSetup(const Game& g) {
    const char* durations[]={"Target / before sunset","10 minutes / full day","60 seconds / Hard"};
    for(int i=0;i<3;++i){float x=620.f+i*230;frame({x,254,214,138});textC(x+107,363,i==0?std::to_string(difficultySettings(g.difficulty).targetFish)+" fish / "+std::to_string(int(difficultySettings(g.difficulty).dayLength))+" seconds":durations[i],F_TINY);}
    textC(1020,482,g.selectedMode==RoundMode::Hardcore?"Hard (fixed)":difficultyName(g.difficulty),F_MED);
    text(620,530,"YOUR LOADOUT",F_MED,GREEN);
    for(int i=0;i<5;++i){float x=650.f+i*130;gearIcon(x,552,GearCategory(i),g.session.equipped(GearCategory(i)),44);textC(x+22,626,gearSpec(GearCategory(i),g.session.equipped(GearCategory(i))).name,F_SMALL);}
    text(286,226,"Choose location",F_SMALL);
}

void drawEquipmentCatalog(const Game& g) {
    bool shop=g.overlay==Overlay::Shop;
    text(286,226,shop?"EQUIPMENT & SUPPLIES / between days":"YOUR EQUIPMENT & SUPPLIES",F_SMALL);
    std::string wallet=money(g.session.coins)+" coins";float walletX=1306-uifont::width(F_MED,wallet);icon(walletX-42,201-(g.purchasePulse>0?std::sin(g.purchasePulse*20)*4:0),10,26);text( walletX,226,wallet,F_MED);
    if(g.locationsTab){
        for(int i=0;i<3;++i){float x=286.f+i*340;frame({x,320,320,350});mapIcon(x+80,345,160,MapId(i));textC(x+160,508,world::mapSpec(MapId(i)).name,F_MED,GREEN);textC(x+160,550,g.session.maps[i]?"Ready for your next trip":"Unlock for this session",F_SMALL);}
    } else {
        auto items=g.visibleItems();
        // Card contents are drawn after the selectable card background below.
        if(!items.empty()) {
            int id=items[std::min(g.overlayIndex,int(items.size())-1)];const auto& item=gearSpec(g.gearCategory,id);
            frame({946,314,360,376});textC(1126,350,item.name,F_MED,GREEN);gearIcon(1082,370,g.gearCategory,id,80);
            wrapped(964,486,324,item.detail,F_SMALL,INK);
            int owned=g.gearCategory==GearCategory::Bait?g.session.baitCount[id]:int(g.session.owns(g.gearCategory,id));
            text(964,540,"Owned: "+std::to_string(owned),F_SMALL);
            if(shop && g.gearCategory==GearCategory::Bait)textR(1288,540,"Quantity: "+std::to_string(g.purchaseQuantity),F_MED);
            const auto& current=gearSpec(g.gearCategory,g.session.equipped(g.gearCategory));
            if(shop && g.gearCategory!=GearCategory::Bait)wrapped(964,575,324,"Current: "+std::string(current.name)+". "+current.detail,F_SMALL,DIM);
            else if(!shop && g.session.equipped(g.gearCategory)!=id)text(964,575,"Equipped: "+std::string(current.name),F_SMALL,DIM);
            else if(shop)textC(1126,464,"Using: "+std::string(current.name),F_SMALL,DIM);
            if(items.size()>4)textC(588,649,"Page "+std::to_string(g.overlayIndex/4+1)+" / 2",F_SMALL,GREEN);
        } else {text(296,374,"No supplies in this category.",F_MED);text(296,415,"End the day to restock from the Shop.",F_SMALL);}
        if(!shop){int total=0;for(int n:g.session.baitCount)total+=n;
            text(296,684,"Bait remaining: "+std::to_string(total)+"   /   Spare lines: "+std::to_string(g.linesLeft),F_SMALL);
            if(g.ledger.active && g.ledger.orderAccepted)text(296,716,"Order: "+std::string(fishSpec(g.ledger.order.type).name)+" "+std::to_string(std::min(g.ledger.safeCount(g.ledger.order.type),g.ledger.order.quantity))+" / "+std::to_string(g.ledger.order.quantity)+(g.ledger.orderComplete()?" / Complete":""),F_SMALL,GREEN);
        } else text(296,716,"Purchase here. Equip owned gear in Inventory.",F_SMALL);

    }
}


const char* overlayTitle(Overlay panel) {
    switch(panel) {
    case Overlay::Shop:return "FISHING SHOP";
    case Overlay::Inventory:return "INVENTORY";
    case Overlay::TripSetup:return "PREPARE YOUR TRIP";
    case Overlay::Credits:return "CREDITS";
    case Overlay::DailyOrder:return "NEW DAY";
    case Overlay::NoBait:return "DAY COMPLETE";
    case Overlay::Secret:return "SECRET FOUND";
    default:return "SETTINGS";
    }
}

void overlay(const Game& g) {
    bool settings=g.overlay==Overlay::Settings;
    fillRect(0,0,CW,CH,INK,.50f);frame(settings?Rect{320,185,960,530}:Rect{238,134,1130,670});frame(settings?Rect{540,155,520,80}:Rect{510,110,580,90},true);
    heading(800,settings?214:169,overlayTitle(g.overlay),34);
    if(g.overlay==Overlay::DailyOrder){
        drawDailyOrder(g);
    } else if(g.overlay==Overlay::NoBait || g.overlay==Overlay::Secret){
        drawSpecialNotice(g);
    } else if(g.overlay==Overlay::Credits){
        drawCredits();
    } else if(g.overlay==Overlay::Settings){
        drawSettings(g);
    } else if(g.overlay==Overlay::TripSetup){
        drawTripSetup(g);
    } else {
        drawEquipmentCatalog(g);
    }
}

void catchScreen(const Game& g) {
    fillRect(0,0,CW,CH,INK,.42f);frame({450,234,700,470});frame({535,205,530,80},true);heading(800,259,"YOU CAUGHT A FISH!",32);
    fishIcon(680,326,230,std::max(0,g.lastCatchType));
    textC(800,510,fishSpec(std::max(0,g.lastCatchType)).name,F_BIG);
    textC(800,545,std::string(qualityName(g.lastCatchQuality))+" / "+std::to_string(g.lastCatchQuality)+"% / "+std::to_string(g.lastCatchSale)+(g.lastCatchSuspicious?" coins if healthy":" coins"),F_MED,GREEN);
    if(g.lastCatchSuspicious){
        for(int i=0;i<5;++i)disc(747+i*21,385+(i%2)*12,5,Color(.58f,.23f,.67f));
        textC(800,580,"Unusual markings. Keeping a poisonous fish costs 10 coins.",F_SMALL,RED);
    }else textC(800,580,"Keep for the day's sale, or release back to the lake.",F_SMALL);
}
void result(const Game& g) {
    const auto& day=g.summary;
    fillRect(0,0,CW,CH,INK,.38f);frame({200,160,1200,660});frame({490,119,620,91},true);
    heading(800,179,"DAY "+std::to_string(day.number)+" RESULTS",38);
    textC(800,236,std::string(roundModeName(day.mode))+" / "+world::mapSpec(day.map).name,F_MED,GREEN);textC(800,270,g.endReason,F_SMALL);
    int total=0,types=0;for(int n:day.caught){total+=n;types+=n>0;}
    text(238,309,std::to_string(total)+" fish caught / "+std::to_string(types)+" species",F_MED,GREEN);
    if(!total){fishIcon(535,412,110);textC(588,520,"No catches today",F_BIG);textC(588,560,"Your next catch is waiting.",F_SMALL,DIM);}
    else {
        fillRect(238,326,700,38,FOREST);text(302,353,"Fish",F_SMALL,WHITE);textC(520,353,"Caught",F_SMALL,WHITE);textC(610,353,"Kept",F_SMALL,WHITE);textC(710,353,"Released",F_SMALL,WHITE);textR(922,353,"Income",F_SMALL,WHITE);
        int row=0;
        for(int type=0;type<FISH_TYPE_COUNT;++type){if(!day.caught[type])continue;float y=370.f+row++*52;
            if(row%2)fillRect(238,y,700,50,PAPER);
            fishIcon(248,y+8,44,type);text(302,y+23,fishSpec(type).name,F_SMALL);
            std::string detail=day.sold[type]?"Quality "+std::to_string(day.qualityAverage[type])+"%":"";
            if(day.toxic[type])detail+=(detail.empty()?"":" / ")+std::to_string(day.toxic[type])+" toxic";
            if(!detail.empty())text(302,y+45,detail,F_SMALL,day.toxic[type]?RED:DIM);
            textC(520,y+31,std::to_string(day.caught[type]),F_MED);textC(610,y+31,std::to_string(day.sold[type]+day.toxic[type]),F_MED);textC(710,y+31,std::to_string(day.released[type]),F_MED);textR(922,y+31,std::to_string(day.saleTotals[type]),F_MED,GREEN);
        }
    }
    textC(1160,320,"DAY'S NET INCOME",F_MED,GREEN);
    float t=clampf(g.stateTimer/1.2f,0,1),ease=1-std::pow(1-t,4);
    icon(990,350,10,48);heading(1180,411,std::to_string(int(day.net*ease)),48,day.net<0?RED:GREEN);
    float y=456;auto line=[&](const std::string& label,const std::string& value,const Color& color){text(990,y,label,F_SMALL);textR(1356,y,value,F_MED,color);};
    line("Fish sales","+"+std::to_string(day.gross),GREEN);
    if(day.order.accepted){y+=38;text(990,y,"Order: "+std::string(fishSpec(day.order.type).name)+" "+std::to_string(std::min(day.order.keptHealthy,day.order.required))+" / "+std::to_string(day.order.required),F_SMALL);y+=30;line(day.order.complete()?"Order completed":"Order not completed","+"+std::to_string(day.orderBonus),day.order.complete()?GREEN:DIM);}
    if(day.cleaningFee){y+=38;line("Cleaning fees","-"+std::to_string(day.cleaningFee),RED);}
    y+=46;fillRect(990,y-25,366,1,DIM,.5f);line("Net income",(day.net>=0?"+":"")+std::to_string(day.net),day.net<0?RED:GREEN);
    y+=44;line("Wallet at closing",money(day.closingCoins),INK);
    text(238,712,"Healthy kept fish are sold automatically. Released fish return to the lake.",F_SMALL,DIM);
}

void help() {
    fillRect(0,0,CW,CH,INK,.50f);frame({238,134,1130,670});frame({510,110,580,90},true);heading(800,169,"HOW TO PLAY",38);
    const char* labels[]={"Aim","Charge / cast","Hook the fish","Reel / ease tension","Equipment / shop","Camera views","Pause / settings","Trip setup","Keep / Release"};
    const char* keys[]={"Mouse or W A S D / arrows","Hold left / SPACE: power fills 0-100%, then cycles","Click left mouse / E when the float dips","Hold left mouse / R. Release when tension rises.","I inventory, U shop between days, B change bait","1 Angler, 2 Float, 3 Dock, 4 Map; V cycle, M map, C reset","P / ESC pause, O settings, F11 fullscreen","Q / E location, arrows mode/difficulty, Enter starts","Enter keeps, L releases. Poisonous fish cost a cleaning fee."};
    for(int i=0;i<9;++i){float y=248.f+i*58;text(292,y,labels[i],F_MED,GREEN);text(615,y,keys[i],F_SMALL);}
    textC(800,750,"Quality drops when reeling above 80% tension or during a surge. Ease off to protect value.",F_SMALL);
    textC(800,786,"Same physical keys work in Arabic / English. Each cast uses one bait.",F_SMALL);
}
} // namespace
int HUD::actionAt(const Game& g,int mx,int my,int w,int h) const {
    Canvas c(w,h);float x,y;c.point(mx,my,x,y);
    for(const auto& b:widgets(g)) if(b.enabled && b.r.contains(x,y))return b.action;
    return -1;
}
int HUD::inspectAt(const Game& g,int mx,int my,int w,int h) const {
    Canvas canvas(w,h);float x=0,y=0;canvas.point(mx,my,x,y);
    for(const auto& widget:widgets(g))if(widget.r.contains(x,y))return widget.action;
    if(!g.showHelp && (g.overlay==Overlay::Shop || g.overlay==Overlay::Inventory)) {
        auto items=g.visibleItems();for(int slot=0;slot<int(items.size());++slot)if(slot/4==g.overlayIndex/4 && itemCard(slot%4).contains(x,y))return 800+items[slot];
    }
    return -1;
}
bool HUD::actionCenter(const Game& g,int action,int w,int h,int& x,int& y) const {
    Canvas c(w,h);for(const auto& b:widgets(g))if(b.action==action){x=int(c.ox+(b.r.x+b.r.w/2)*c.scale);y=int(c.oy+(b.r.y+b.r.h/2)*c.scale);return true;}return false;
}
bool HUD::menuItemCenter(const Game& g,int i,int w,int h,int& x,int& y) const {return actionCenter(g,ui::Menu+i,w,h,x,y);}
int HUD::menuItemAt(const Game& g,int x,int y,int w,int h) const {int a=actionAt(g,x,y,w,h);return a>=ui::Menu&&a<ui::MenuEnd?a-ui::Menu:-1;}
void HUD::draw(const Game& g,const Renderer& r,const Camera& cam,int w,int h,float fps) {
    uiClock=g.uiTime;
    (void)fps;
    glPushAttrib(GL_ALL_ATTRIB_BITS);glViewport(0,0,w,h);glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_TEXTURE_2D);glDisable(GL_LINE_SMOOTH);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,w,h,0,-1,1);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    Canvas c(w,h);
    // Fit the 16:9 design without stretching; input uses the identical transform.
    if(c.ox>0){fillRect(0,0,c.ox,float(h),INK);fillRect(w-c.ox,0,c.ox,float(h),INK);}
    if(c.oy>0){fillRect(0,0,float(w),c.oy,INK);fillRect(0,h-c.oy,float(w),c.oy,INK);}
    glTranslatef(c.ox,c.oy,0);glScalef(c.scale,c.scale,1);uifont::setCanvasScale(c.scale);
    if(g.preferences.brightness<1)fillRect(0,0,CW,CH,INK,1-g.preferences.brightness);
    else if(g.preferences.brightness>1)fillRect(0,0,CW,CH,CREAM,(g.preferences.brightness-1)*.45f);
    if(g.state==GameState::MainMenu) {
        fillRect(0,0,CW,CH,INK,.16f);logo({430,124,740,150},true);
    } else if(g.state==GameState::Victory || g.state==GameState::GameOver) result(g);
    else {
        gameplay(g,cam);
        worldFeedback(g,r,c);
        if(g.catchPending && g.state==GameState::FishCaught)catchScreen(g);
        if(g.state==GameState::Paused){fillRect(0,0,CW,CH,INK,.48f);frame({510,270,580,352});frame({590,242,420,80},true);heading(800,296,"PAUSED",40);}
    }
    if(g.overlay!=Overlay::None)overlay(g);
    if(g.showHelp)help();
    for(const auto& b:widgets(g)){
        if(b.action==ui::SoundSlider || b.action==ui::SecretFish || b.action==ui::CreatorProfile){if(b.action==ui::CreatorProfile && hovered==ui::CreatorProfile)fillRect(695,537,260,2,GREEN);continue;}
        Widget drawn=b;if((b.action==ui::Shop || b.action==ui::OpenShop) && !g.shopAvailable())drawn.enabled=false;
        button(drawn,hovered,pressed);
        if(b.action>=ui::Location && b.action<ui::LocationEnd){int i=b.action-ui::Location;mapIcon(b.r.x+12,b.r.y+17,92,MapId(i));text(b.r.x+118,b.r.y+42,world::mapSpec(MapId(i)).name,F_SMALL,WHITE);text(b.r.x+118,b.r.y+80,g.session.maps[i]?"Ready":std::to_string(world::mapSpec(MapId(i)).price)+" coins",F_SMALL,CREAM);}
        if(b.action>=ui::Item && b.action<ui::ItemEnd){int id=b.action-ui::Item;Color accents[]={Color(.85f,.64f,.30f),Color(.34f,.68f,.91f),Color(.65f,.77f,.86f),Color(.72f,.60f,.90f),Color(.98f,.68f,.30f)};fillRect(b.r.x+8,b.r.y+15,4,b.r.h-30,accents[int(g.gearCategory)]);fillRect(b.r.x+10,b.r.y+14,64,68,CREAM);gearIcon(b.r.x+14,b.r.y+20,g.gearCategory,id,54);text(b.r.x+82,b.r.y+42,gearSpec(g.gearCategory,id).name,F_MED,WHITE);int quantity=g.gearCategory==GearCategory::Bait?g.session.baitCount[id]:int(g.session.owns(g.gearCategory,id));text(b.r.x+82,b.r.y+75-(g.purchasePulse>0 && b.selected?3*std::sin(g.purchasePulse*12):0),"Owned x"+std::to_string(quantity),F_SMALL,CREAM);if(g.overlay==Overlay::Shop)text(b.r.x+82,b.r.y+107,std::to_string(gearSpec(g.gearCategory,id).price)+(g.gearCategory==GearCategory::Bait?" coins / each":" coins"),F_SMALL,Color(1,.8f,.36f));}
        if(b.action==ui::Shop && !g.shopAvailable())icon(b.r.x+72,b.r.y+36,18,24);
    }
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
}
} // namespace lf
