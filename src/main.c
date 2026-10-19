/*
RANDOM ALPHA - mapa detalhado integrado ao prototipo PSP (src/main.c)
CONTROLES:
Direcional/analógico = mover
X = pular / pulo duplo
QUADRADO = espada
CÍRCULO = magia (após pegar o cristal roxo)
TRIÂNGULO = gastar ponto na habilidade selecionada
L/R = selecionar habilidade: vida, magia, espada, velocidade, defesa
START = pausar
SELECT = renascer no checkpoint
Requer PSPSDK. Código-base de protótipo: integre/compile no projeto existente.
*/

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <math.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define WW 1600
#define GROUND 250
#define PW 18
#define PH 30
#define CAVE 0
#define FOREST 1
#define CITY 2
#define CASTLE 3
#define TOWER 4
#define ARENA 5
#define ZONE_COUNT 6
#define N_MOBS 4
typedef unsigned int Color;
typedef struct { Color c; float x,y,z; } Vtx;
typedef struct { float x,y,w,h; } Box;
static unsigned int __attribute__((aligned(16))) list[262144];

#define BLACK  0xFF101018
#define DARK   0xFF252535
#define STONE  0xFF555568
#define GRAY   0xFF858596
#define LIGHT  0xFFBFC4D0
#define WHITE  0xFFFFFFFF
#define GREEN  0xFF35B85A
#define DGREEN 0xFF17633B
#define BLUE   0xFFFFA040
#define PURPLE 0xFFB050E8
#define RED    0xFF3030E8
#define YELLOW 0xFF30E8F8
#define BROWN  0xFF493020
#define SKIN   0xFF70B8E8

static int zone=CAVE, paused=0, face=1, grounded=1, jumps=2, swordCD=0;
static float cam=0, px=60, py=220, vy=0;
static int hp=5,maxHP=5, magic=0, magicCD=0;
static int xp=0, level=1, nextXP=20, points=0;
static int magicPower=1, swordDamage=1, speedLevel=1, defense=0, selected=0;
static int cpZone=CAVE, cpReached[6]={0,0,0,0,0,0};
static float cpX=60,cpY=220;
static int zoneBossHP[ZONE_COUNT]={12,16,20,24,28,40};
static int zoneBossMax[ZONE_COUNT]={12,16,20,24,28,40};

typedef struct { float x,y,min,max,speed; int alive,hp,cd; } Mob;
static Mob mobs[N_MOBS]={
 {300,220,250,380,.8f,1,1,0},{610,220,550,680,1,1,1,0},
 {960,220,900,1040,.9f,1,1,0},{1190,220,1150,1250,1.1f,1,1,0}
};
static struct {float x,y,vy;int grounded,timer,active,hp,cd;} boss=
 {1350,218,0,1,50,1,12,0};
typedef struct {int on;float x,y,vx;} Shot;
static Shot shots[8];

static int pressed(unsigned int n,unsigned int o,unsigned int b){return (n&b)&&!(o&b);}
static void rect(float x,float y,float w,float h,Color c){
 if(w<=0||h<=0)return;
 Vtx *v=(Vtx*)sceGuGetMemory(2*sizeof(Vtx));
 v[0]=(Vtx){c,x,y,0}; v[1]=(Vtx){c,x+w,y+h,0};
 sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
}
static void wr(float x,float y,float w,float h,Color c){rect(x-cam,y,w,h,c);}
static void tri(float x1,float y1,float x2,float y2,float x3,float y3,Color c){
 Vtx *v=(Vtx*)sceGuGetMemory(3*sizeof(Vtx));
 v[0]=(Vtx){c,x1-cam,y1,0}; v[1]=(Vtx){c,x2-cam,y2,0}; v[2]=(Vtx){c,x3-cam,y3,0};
 sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,3,0,v);
}
static void arch(float x,float y,float w,float h){
 wr(x,y,10,h,STONE); wr(x+w-10,y,10,h,STONE); wr(x,y,w,8,GRAY);
 tri(x-4,y+8,x+5,y-8,x+14,y+8,LIGHT); tri(x+w-14,y+8,x+w-5,y-8,x+w+4,y+8,LIGHT);
}
static void rubble(float x,float y,float z){
 tri(x,y+z,x+z*.4f,y,x+z,y+z,DARK); wr(x+z*.25f,y+z*.65f,z*.6f,z*.2f,GRAY);
}
static void crystal(float x,float y,float z){
 tri(x,y+z,x+z*.5f,y,x+z,y+z,LIGHT);
 tri(x+z*.5f,y,x+z,y+z,x+z*.5f,y+z*1.35f,WHITE);
 tri(x,y+z,x+z*.5f,y+z*1.35f,x+z*.5f,y,GRAY);
}
static void gate(float x,float y,float w,float h){
 wr(x,y,8,h,DARK); wr(x+w-8,y,8,h,DARK); wr(x,y,w,8,GRAY);
 for(int i=0;i<6;i++)wr(x+14+i*((w-28)/6),y+12,3,h-14,STONE);
}
static void drawExtraArchitecture(void){
 int i;
 if(zone==CAVE){
  for(i=0;i<30;i++){float x=i*53.0f;tri(x,0,x+9,18+(i%5)*8,x+18,0,GRAY);}
  for(i=0;i<17;i++){float x=i*98.0f;wr(x,95+(i%4)*20,7,100,DARK);wr(x+12,175+(i%3)*16,5,65,STONE);}
  crystal(310,GROUND-53,20);crystal(650,GROUND-55,24);crystal(1030,GROUND-52,18);
  arch(25,GROUND-105,75,105);gate(1490,GROUND-110,75,110);
  wr(1190,GROUND-55,270,55,DARK);wr(1190,GROUND-55,270,4,LIGHT);
  for(i=0;i<7;i++)wr(1210+i*35,GROUND-40,4,25,GRAY);
 } else if(zone==FOREST){
  for(i=0;i<18;i++){float x=i*92.0f+10;wr(x,GROUND-80-(i%4)*10,10,80+(i%4)*10,DARK);
   tri(x-28,GROUND-70-(i%4)*10,x+5,GROUND-145-(i%3)*10,x+40,GROUND-70-(i%4)*10,DGREEN);
   tri(x-20,GROUND-100,x+5,GROUND-165,x+32,GROUND-100,DGREEN);}
  wr(1120,GROUND-110,360,110,DARK);wr(1120,GROUND-110,360,5,LIGHT);
  crystal(1288,GROUND-195,20);gate(1125,GROUND-215,75,105);
 } else if(zone==CITY){
  for(i=0;i<22;i++){float x=i*75.0f,h=85+(i%5)*23;wr(x,GROUND-h,55,h,DARK);wr(x+5,GROUND-h+12,45,4,STONE);
   wr(x+12,GROUND-h+28,8,15,GRAY);wr(x+33,GROUND-h+28,8,15,GRAY);}
  for(i=0;i<12;i++)rubble(i*125.0f+20,GROUND-35,32+(i%3)*7);
  for(i=0;i<5;i++){float x=230+i*280.0f;wr(x,GROUND-80,5,80,GRAY);wr(x-14,GROUND-80,34,12,STONE);}
 } else if(zone==CASTLE){
  for(i=0;i<9;i++){float x=i*180.0f+20;arch(x,145+(i%2)*20,110,GROUND-145-(i%2)*20);if(i%2==0){wr(x-20,GROUND-150,24,150,DARK);wr(x-25,GROUND-150,34,7,GRAY);}}
  wr(0,250,WW,12,DARK);for(i=0;i<6;i++){float x=120+i*260.0f;wr(x,95,4,100,GRAY);tri(x+4,100,x+52,112,x+4,145,STONE);}
  wr(1120,GROUND-110,360,110,DARK);wr(1120,GROUND-110,360,6,LIGHT);gate(1125,GROUND-215,75,105);
 } else if(zone==TOWER){
  wr(0,0,48,SH,DARK);wr(WW-48,0,48,SH,DARK);
  for(i=0;i<12;i++){float x=100+i*125.0f,y=25+(i%3)*18;wr(x,y,24,GROUND-y,DARK);wr(x-5,y,34,7,GRAY);tri(x-3,y,x+12,y-18,x+27,y,LIGHT);}
  wr(1120,GROUND-110,360,110,DARK);wr(1120,GROUND-110,360,5,WHITE);gate(1125,GROUND-215,75,105);crystal(1280,GROUND-220,24);
 } else {
  for(i=0;i<18;i++){float x=i*95.0f;tri(x,GROUND,x+45,GROUND-95-(i%4)*15,x+95,GROUND,DARK);}
  wr(590,GROUND-110,420,110,DARK);wr(590,GROUND-110,420,6,WHITE);wr(610,GROUND-95,380,4,STONE);
  for(i=0;i<6;i++){float x=615+i*72.0f;wr(x,GROUND-180,16,70,GRAY);wr(x-5,GROUND-188,26,8,LIGHT);}
  wr(730,GROUND-122,140,12,STONE);wr(745,GROUND-135,110,13,LIGHT);tri(750,GROUND-155,800,GROUND-210,850,GROUND-155,WHITE);
  gate(605,GROUND-215,75,105);gate(915,GROUND-215,75,105);
 }
}
static int hit(Box a,Box b){return a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;}
static Box pbox(void){return (Box){px,py,PW,PH};}
static void addXP(int n){
 xp+=n; while(xp>=nextXP){xp-=nextXP;level++;points++;nextXP+=10;if(hp<maxHP)hp++;}
}
static void spend(void){
 if(!points)return;
 switch(selected){case 0:maxHP++;hp++;break;case 1:magicPower++;break;
 case 2:swordDamage++;break;case 3:speedLevel++;break;case 4:defense++;break;}
 points--;
}
static int damage(void){int d=2-defense;return d<1?1:d;}
static void respawn(void){
 for(int i=0;i<8;i++)shots[i].on=0;
 zone=cpZone;px=cpX;py=cpY;vy=0;grounded=1;jumps=2;hp=maxHP;
 cam=px-SW/2;if(cam<0)cam=0;if(cam>WW-SW)cam=WW-SW;
}
static void drawCave(void){
 rect(0,0,SW,SH,BLACK);wr(0,0,WW,GROUND,DARK);wr(0,GROUND,WW,22,STONE);
 wr(0,GROUND+22,WW,20,GRAY);wr(0,0,WW,24,STONE);
 for(int x=40;x<WW;x+=130){wr(x,24,14,18,GRAY);wr(x+4,42,6,10,STONE);}
 wr(190,190,100,10,STONE);wr(430,160,110,10,STONE);wr(650,190,100,10,STONE);
 wr(680,135,70,10,GRAY);wr(850,180,100,10,STONE);wr(1080,155,100,10,STONE);
 wr(1300,235,150,15,GRAY);
 /* Labirinto original: paredes que formam bifurcacoes e salas laterais. */
 wr(150,210,12,40,STONE); wr(310,145,12,105,STONE);
 wr(330,145,95,8,GRAY); wr(425,145,12,48,STONE);
 wr(470,205,12,45,STONE); wr(555,120,12,130,STONE);
 wr(575,120,75,8,GRAY); wr(640,120,12,40,STONE);
 wr(740,160,12,90,STONE); wr(770,160,75,8,GRAY);
 wr(835,110,12,70,STONE); wr(930,150,12,100,STONE);
 wr(980,150,80,8,GRAY); wr(1055,95,12,60,STONE);
 wr(1170,175,12,75,STONE); wr(1210,175,80,8,GRAY);
 /* Sala secreta superior e passagem baixa para a arena. */
 wr(390,55,100,8,PURPLE); wr(390,55,8,45,PURPLE); wr(482,55,8,45,PURPLE);
 wr(1370,145,12,105,STONE); wr(1390,145,65,8,GRAY);
 /* Cristal que libera magia */
 wr(713,107,24,10,PURPLE);wr(720,100,10,24,PURPLE);wr(718,102,14,20,WHITE);
 wr(690,65,45,40,PURPLE);wr(1470,185,30,65,YELLOW);
}
static void drawForest(void){
 rect(0,0,SW,SH,0xFF7DC6F0);wr(0,GROUND,WW,22,DGREEN);wr(0,GROUND+22,WW,20,BROWN);
 for(int x=80;x<WW;x+=170){wr(x,145,12,105,BROWN);wr(x-24,120,62,55,DGREEN);wr(x-15,95,45,45,GREEN);}
 wr(220,190,100,10,BROWN);wr(520,160,100,10,BROWN);wr(800,190,120,10,BROWN);wr(1080,150,100,10,BROWN);
 wr(1240,85,45,50,PURPLE);wr(120,185,25,65,YELLOW);
}
static void drawOtherZone(void){
 if(zone==CITY){rect(0,0,SW,SH,0xFF303044);wr(0,GROUND,WW,22,STONE);wr(0,GROUND+22,WW,20,DARK);for(int x=70;x<WW;x+=190){wr(x,90,60,160,GRAY);wr(x+8,105,10,12,YELLOW);wr(x+32,105,10,12,YELLOW);wr(x+8,135,10,12,YELLOW);wr(x+32,135,10,12,YELLOW);}wr(250,190,100,10,STONE);wr(570,155,120,10,STONE);wr(920,185,100,10,STONE);wr(1200,140,110,10,STONE);}
 else if(zone==CASTLE){rect(0,0,SW,SH,0xFF20202D);wr(0,GROUND,WW,22,STONE);wr(0,GROUND+22,WW,20,DARK);for(int x=100;x<WW;x+=260){wr(x,65,25,185,GRAY);wr(x-12,55,50,12,LIGHT);}wr(300,190,110,10,GRAY);wr(560,150,110,10,GRAY);wr(850,180,120,10,GRAY);wr(1120,135,110,10,GRAY);}
 else if(zone==TOWER){rect(0,0,SW,SH,0xFF101827);wr(0,GROUND,WW,22,STONE);wr(0,GROUND+22,WW,20,DARK);for(int y=55;y<220;y+=55)wr(150+(y%3)*170,y,130,8,PURPLE);for(int x=40;x<WW;x+=300){wr(x,35,8,215,GRAY);wr(x+70,35,8,215,GRAY);}}
 else {rect(0,0,SW,SH,0xFF140D22);wr(0,GROUND,WW,22,PURPLE);wr(0,GROUND+22,WW,20,DARK);for(int x=80;x<WW;x+=160){wr(x,GROUND-8,4,8,YELLOW);}wr(650,185,180,10,PURPLE);wr(1020,155,150,10,PURPLE);wr(250,70,12,180,PURPLE);wr(1330,70,12,180,PURPLE);wr(265,70,1065,8,STONE);}
}
static void drawPlayer(void){
 float x=px-cam;rect(x+4,py+2,10,12,SKIN);rect(x+2,py+13,14,13,BLUE);
 rect(x,py+25,7,5,BROWN);rect(x+11,py+25,7,5,BROWN);
 rect(x+(face?13:-7),py+15,12,3,LIGHT);
 rect(8,8,80,7,DARK);rect(9,9,78.0f*hp/(maxHP?maxHP:1),5,RED);
}
static void drawMobs(void){
 for(int i=0;i<N_MOBS;i++)if(mobs[i].alive){
  wr(mobs[i].x,mobs[i].y,20,25,RED);wr(mobs[i].x+4,mobs[i].y+5,4,4,WHITE);wr(mobs[i].x+13,mobs[i].y+5,4,4,WHITE);
 }
 if(boss.active){
  wr(boss.x,boss.y,36,32,PURPLE);wr(boss.x+5,boss.y+7,7,6,YELLOW);wr(boss.x+23,boss.y+7,7,6,YELLOW);
  rect(345,8,125,7,DARK);rect(346,9,123.0f*boss.hp/(float)(zoneBossMax[zone]?zoneBossMax[zone]:1),5,RED);
 }
}
static void drawCheckpoints(void){
 float xs[3]={520,1050,800};int zs[3]={CAVE,CAVE,FOREST};
 for(int i=0;i<3;i++)if(zs[i]==zone){wr(xs[i],192,4,28,GRAY);wr(xs[i]+4,192,15,10,cpReached[i]?GREEN:YELLOW);}
}
static void drawHUD(void){
 /* Marcador visual de zona: seis segmentos, o ativo fica claro. */
 for(int zi=0;zi<ZONE_COUNT;zi++)rect(300+zi*22,8,16,5,zi==zone?WHITE:STONE);
 rect(8,20,80,5,DARK);rect(9,21,78.0f*xp/nextXP,3,YELLOW);
 rect(8,29,points?40:8,4,points?GREEN:GRAY);
 Color colors[5]={RED,PURPLE,YELLOW,GREEN,BLUE};
 for(int i=0;i<5;i++)rect(100+i*16,8,12,12,i==selected?colors[i]:GRAY);
 if(magic)rect(184,8,12,12,PURPLE);
 if(paused){rect(130,75,220,110,DARK);rect(145,90,190,5,WHITE);rect(145,110,150,5,GRAY);rect(145,130,170,5,YELLOW);rect(145,150,120,5,GREEN);}
}
static void draw(void){
 sceGuStart(GU_DIRECT,list);sceGuClearColor(BLACK);sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
 sceGuDisable(GU_DEPTH_TEST);sceGuDisable(GU_TEXTURE_2D);
 if(zone==CAVE)drawCave();else if(zone==FOREST)drawForest();else drawOtherZone();
 drawExtraArchitecture();
 drawCheckpoints();drawMobs();
 for(int i=0;i<8;i++)if(shots[i].on)wr(shots[i].x,shots[i].y,9,7,PURPLE);
 drawPlayer();drawHUD();sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuSwapBuffers();
}
static void cast(void){
 if(!magic||magicCD)return;
 for(int i=0;i<8;i++)if(!shots[i].on){shots[i]=(Shot){1,px+(face?PW:-8),py+12,face?5:-5};magicCD=18;break;}
}
static void updateShots(void){
 for(int i=0;i<8;i++)if(shots[i].on){
  shots[i].x+=shots[i].vx;if(shots[i].x<0||shots[i].x>WW)shots[i].on=0;
  if(zone>=ZONE_COUNT)continue;
  Box s={shots[i].x,shots[i].y,9,7};
  for(int m=0;m<N_MOBS;m++)if(mobs[m].alive&&hit(s,(Box){mobs[m].x,mobs[m].y,20,25})){
   mobs[m].hp-=magicPower;shots[i].on=0;if(mobs[m].hp<=0){mobs[m].alive=0;addXP(5);}break;
  }
  if(shots[i].on&&boss.active&&hit(s,(Box){boss.x,boss.y,36,32})){
   boss.hp-=magicPower;shots[i].on=0;if(boss.hp<=0){boss.active=0;addXP(50);}
  }
 }
}
static void updateMobs(void){
 for(int i=0;i<N_MOBS;i++)if(mobs[i].alive){
  mobs[i].x+=mobs[i].speed;if(mobs[i].x<mobs[i].min||mobs[i].x>mobs[i].max)mobs[i].speed=-mobs[i].speed;
  if(mobs[i].cd>0)mobs[i].cd--;
  if(hit(pbox(),(Box){mobs[i].x,mobs[i].y,20,25})&&!mobs[i].cd){hp-=damage();mobs[i].cd=35;}
 }
 if(boss.active){
  if(boss.cd>0)boss.cd--;
  if(boss.grounded&&fabsf(px-boss.x)<300&&boss.timer<=0){boss.vy=-6;boss.grounded=0;boss.timer=70;}
  if(boss.timer>0)boss.timer--;
  boss.vy+=.25f;boss.y+=boss.vy;
  if(boss.y>=218){boss.y=218;boss.vy=0;boss.grounded=1;}
  if(px<boss.x)boss.x-=.5f;else boss.x+=.5f;
  if(boss.x<1300)boss.x=1300;if(boss.x>1410)boss.x=1410;
  if(hit(pbox(),(Box){boss.x,boss.y,36,32})&&!boss.cd){hp-=damage();boss.cd=45;}
 }
}
static void sword(int down){
 static int was=0;if(!down){was=0;return;}if(was||swordCD>0)return;was=1;swordCD=14;
 Box a={px+(face?PW:-24),py+5,24,20};
 for(int i=0;i<N_MOBS;i++)if(mobs[i].alive&&hit(a,(Box){mobs[i].x,mobs[i].y,20,25})){
  mobs[i].hp-=swordDamage;if(mobs[i].hp<=0){mobs[i].alive=0;addXP(5);}
 }
 if(boss.active&&!boss.cd&&hit(a,(Box){boss.x,boss.y,36,32})){
  boss.hp-=swordDamage;boss.cd=12;if(boss.hp<=0){boss.active=0;addXP(50);}
 }
}
static void checkpoints(void){
 float xs[3]={520,1050,800};int zs[3]={CAVE,CAVE,FOREST};
 for(int i=0;i<3;i++)if(zs[i]==zone&&hit(pbox(),(Box){xs[i],190,22,60})){
  cpZone=zone;cpX=xs[i]-10;cpY=220;cpReached[i]=1;
 }
}
static void changeZone(int nz,float nx,float ny){
 if(nz<0||nz>=ZONE_COUNT)return;
 zone=nz;px=nx;py=ny;vy=0;grounded=1;jumps=2;
 for(int i=0;i<8;i++)shots[i].on=0;
 for(int i=0;i<N_MOBS;i++){mobs[i].alive=1;mobs[i].hp=1+(zone/2);mobs[i].x=280.0f+i*300;mobs[i].y=220;mobs[i].min=mobs[i].x-45;mobs[i].max=mobs[i].x+45;mobs[i].speed=.7f+i*.12f;mobs[i].cd=0;}
 boss.active=1;boss.hp=zoneBossHP[zone];boss.x=1320;boss.y=218;boss.vy=0;boss.grounded=1;boss.timer=45;boss.cd=0;
}
static void transition(void){
 if(px>=WW-PW-2 && zone<ARENA && !boss.active)changeZone(zone+1,35,GROUND-PH);
 else if(px<=1 && zone> CAVE)changeZone(zone-1,WW-PW-35,GROUND-PH);
}
static void update(SceCtrlData p,unsigned int old){
 float sp=2.0f+.25f*(speedLevel-1);
 int l=(p.Buttons&PSP_CTRL_LEFT)||p.Lx<90,r=(p.Buttons&PSP_CTRL_RIGHT)||p.Lx>165;
 if(l){px-=sp;face=0;}if(r){px+=sp;face=1;}
 if(px<0)px=0;if(px>WW-PW)px=WW-PW;
 if(pressed(p.Buttons,old,PSP_CTRL_CROSS)&&jumps>0){vy=-6.3f;grounded=0;jumps--;}
 vy+=.34f;py+=vy;
 if(py+PH>=GROUND){py=GROUND-PH;vy=0;grounded=1;jumps=2;}
 float xs[12],ys[12],ws[12]; int np=0;
 if(zone==CAVE){float ax[]={190,430,650,680,850,1080,260,360,500,570,780,920};float ay[]={190,160,190,135,180,155,125,90,115,75,100,125};float aw[]={100,110,100,70,100,100,75,75,70,70,65,75};for(int k=0;k<12;k++){xs[np]=ax[k];ys[np]=ay[k];ws[np++]=aw[k];}}
 else if(zone==FOREST){float ax[]={220,390,570,760,960,1130,1300};float ay[]={190,155,120,175,140,105,165};for(int k=0;k<7;k++){xs[np]=ax[k];ys[np]=ay[k];ws[np++]=100;}}
 else if(zone==CITY){float ax[]={150,320,500,680,890,1080,1280};float ay[]={190,155,120,180,145,105,160};for(int k=0;k<7;k++){xs[np]=ax[k];ys[np]=ay[k];ws[np++]=100;}}
 else if(zone==CASTLE){float ax[]={180,330,470,620,790,960,1120,1280};float ay[]={195,160,125,175,135,100,155,120};for(int k=0;k<8;k++){xs[np]=ax[k];ys[np]=ay[k];ws[np++]=90;}}
 else if(zone==TOWER){float ax[]={160,300,420,540,660,790,920,1050,1180,1320};float ay[]={195,165,135,105,75,110,145,115,85,125};for(int k=0;k<10;k++){xs[np]=ax[k];ys[np]=ay[k];ws[np++]=80;}}
 else {float ax[]={180,400,620,840,1060,1280};float ay[]={195,175,195,175,195,175};for(int k=0;k<6;k++){xs[np]=ax[k];ys[np]=ay[k];ws[np++]=100;}}
 for(int i=0;i<np;i++){Box a=pbox(),b={xs[i],ys[i],ws[i],10};if(vy>=0&&a.x+a.w>b.x&&a.x<b.x+b.w&&a.y+a.h>=b.y&&a.y+a.h<=b.y+12){py=b.y-PH;vy=0;grounded=1;jumps=2;}}
 if(magicCD>0)magicCD--;if(swordCD>0)swordCD--;
 transition();checkpoints();updateMobs();updateShots();
 if(zone==CAVE&&!magic&&hit(pbox(),(Box){713,100,24,24}))magic=1;
 if(hp<=0)respawn();
 cam=px-SW/2;if(cam<0)cam=0;if(cam>WW-SW)cam=WW-SW;
}
int main(void){
 sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
 sceGuInit();sceGuStart(GU_DIRECT,list);
 sceGuDrawBuffer(GU_PSM_8888,(void*)0,512);
 sceGuDispBuffer(SW,SH,(void*)0x88000,512);
 sceGuDepthBuffer((void*)0x110000,512);
 sceGuOffset(2048-SW/2,2048-SH/2);sceGuViewport(2048,2048,SW,SH);
 sceGuDepthRange(65535,0);sceGuScissor(0,0,SW,SH);sceGuEnable(GU_SCISSOR_TEST);
 sceGuDisable(GU_DEPTH_TEST);sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuDisplay(GU_TRUE);
 SceCtrlData p;unsigned int old=0;
 while(1){
  sceCtrlReadBufferPositive(&p,1);
  if(pressed(p.Buttons,old,PSP_CTRL_START))paused=!paused;
  if(!paused){
   if(pressed(p.Buttons,old,PSP_CTRL_CIRCLE))cast();
   if(pressed(p.Buttons,old,PSP_CTRL_TRIANGLE))spend();
   if(pressed(p.Buttons,old,PSP_CTRL_LTRIGGER))selected=(selected+4)%5;
   if(pressed(p.Buttons,old,PSP_CTRL_RTRIGGER))selected=(selected+1)%5;
   if(pressed(p.Buttons,old,PSP_CTRL_SELECT))respawn();
   sword((p.Buttons&PSP_CTRL_SQUARE)!=0);update(p,old);
  }
  draw();old=p.Buttons;sceKernelDelayThread(16667);
 }
 sceGuTerm();sceKernelExitGame();return 0;
}
