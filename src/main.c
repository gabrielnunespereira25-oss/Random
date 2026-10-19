/*
 * RANDOM ALPHA - MAPA EXPANDIDO (PSP / PSPSDK)
 * Apenas mapa/cenarios: sem personagem, inimigos ou combate.
 * D-pad esquerda/direita: explorar o mapa horizontalmente.
 * L/R: trocar de regiao. START: sair.
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>

PSP_MODULE_INFO("RANDOM_MAPA", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
#define SW 480
#define SH 272
#define WW 2400
#define GROUND 250
#define ZONES 6
#define CAVE 0
#define FOREST 1
#define CITY 2
#define CASTLE 3
#define TOWER 4
#define ARENA 5

typedef unsigned int Color;
typedef struct { Color color; float x,y,z; } Vertex;
static unsigned int __attribute__((aligned(16))) guList[262144];
static int zone=CAVE;
static unsigned int previousButtons=0;
static float cameraX=0;
#define BLACK  0xFF101018
#define DARK   0xFF252535
#define STONE  0xFF555568
#define GRAY   0xFF858596
#define LIGHT  0xFFBFC4D0
#define WHITE  0xFFFFFFFF
#define GREEN  0xFF35B85A
#define DGREEN 0xFF17633B
#define PURPLE 0xFFB050E8
#define YELLOW 0xFF30E8F8
#define BROWN  0xFF493020

static int pressed(unsigned int n,unsigned int o,unsigned int b){return (n&b)&&!(o&b);}
static void rect(float x,float y,float w,float h,Color c){
 if(w<=0||h<=0)return;
 Vertex *v=(Vertex*)sceGuGetMemory(2*sizeof(Vertex));
 v[0]=(Vertex){c,x,y,0}; v[1]=(Vertex){c,x+w,y+h,0};
 sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
}
static void tri(float x1,float y1,float x2,float y2,float x3,float y3,Color c){
 Vertex *v=(Vertex*)sceGuGetMemory(3*sizeof(Vertex));
 v[0]=(Vertex){c,x1,y1,0};v[1]=(Vertex){c,x2,y2,0};v[2]=(Vertex){c,x3,y3,0};
 sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,3,0,v);
}
static void wr(float x,float y,float w,float h,Color c){rect(x-cameraX,y,w,h,c);}
static void wt(float x1,float y1,float x2,float y2,float x3,float y3,Color c){tri(x1-cameraX,y1,x2-cameraX,y2,x3-cameraX,y3,c);}
static void arch(float x,float y,float w,float h){wr(x,y,10,h,STONE);wr(x+w-10,y,10,h,STONE);wr(x,y,w,8,GRAY);wt(x-4,y+8,x+5,y-8,x+14,y+8,LIGHT);wt(x+w-14,y+8,x+w-5,y-8,x+w+4,y+8,LIGHT);}
static void crystal(float x,float y,float s){wt(x,y+s,x+s*.5f,y,x+s,y+s,LIGHT);wt(x+s*.5f,y,x+s,y+s,x+s*.5f,y+s*1.35f,WHITE);wt(x,y+s,x+s*.5f,y+s*1.35f,x+s*.5f,y,GRAY);}
static void gate(float x,float y,float w,float h){wr(x,y,8,h,DARK);wr(x+w-8,y,8,h,DARK);wr(x,y,w,8,GRAY);for(int i=0;i<6;i++)wr(x+14+i*((w-28)/6.0f),y+12,3,h-14,STONE);}
static void rubble(float x,float y,float s){wt(x,y+s,x+s*.4f,y,x+s,y+s,DARK);wr(x+s*.25f,y+s*.65f,s*.6f,s*.2f,GRAY);}
static void drawCave(void){
 wr(0,0,WW,GROUND,BLACK);wr(0,GROUND,WW,22,STONE);wr(0,GROUND+22,WW,SH-GROUND-22,GRAY);wr(0,0,WW,24,STONE);
 for(int x=20;x<WW;x+=65){wt(x,0,x+8,18+(x%5)*3,x+17,0,GRAY);wr(x+20,45,5,45+(x%4)*8,DARK);}
 for(int x=45;x<WW-80;x+=155){wr(x,190,90,10,STONE);wr(x+28,145,78,10,GRAY);wr(x+80,GROUND-35,12,35,STONE);}
 for(int x=80;x<WW-120;x+=230){wr(x,210,12,40,STONE);wr(x+55,130,12,120,STONE);wr(x+67,130,65,8,GRAY);wr(x+130,130,12,55,STONE);}
 for(int x=120;x<WW-180;x+=330){crystal(x,GROUND-55,20+(x%3)*3);arch(x+70,GROUND-95,65,95);}
 /* sala secreta superior e saloes laterais */
 for(int x=180;x<WW-260;x+=440){wr(x,55,105,8,PURPLE);wr(x,55,8,45,PURPLE);wr(x+97,55,8,45,PURPLE);wr(x+18,90,70,6,LIGHT);}
 gate(WW-115,GROUND-110,75,110);wr(WW-360,GROUND-55,245,55,DARK);for(int i=0;i<7;i++)wr(WW-340+i*34,GROUND-40,4,25,GRAY);
 wr(710,100,45,40,PURPLE);crystal(720,85,25);
}
static void drawForest(void){
 wr(0,0,WW,SH,0xFF7DC6F0);wr(0,GROUND,WW,22,DGREEN);wr(0,GROUND+22,WW,SH-GROUND-22,BROWN);
 for(int x=25;x<WW;x+=92){float h=75+(x%4)*12;wr(x+18,GROUND-h,11,h,BROWN);wt(x-8,GROUND-h+10,x+23,GROUND-h-65,x+55,GROUND-h+10,DGREEN);wt(x,GROUND-h-20,x+23,GROUND-h-88,x+47,GROUND-h-20,GREEN);}
 for(int x=80;x<WW-80;x+=185){wr(x,190,80,10,BROWN);wr(x+85,155,70,10,BROWN);wr(x+145,120,75,10,BROWN);}
 for(int x=150;x<WW-100;x+=310){crystal(x,GROUND-125,18);wr(x+40,GROUND-45,8,45,STONE);}
 for(int x=330;x<WW-180;x+=480){arch(x,GROUND-90,85,90);gate(x+125,GROUND-100,65,100);}
 wr(WW-300,GROUND-105,220,105,DARK);crystal(WW-230,GROUND-150,24);
}
static void drawCity(void){
 wr(0,0,WW,SH,0xFF303044);wr(0,GROUND,WW,22,STONE);wr(0,GROUND+22,WW,SH-GROUND-22,DARK);
 for(int x=15;x<WW;x+=72){int h=75+(x%5)*19;wr(x,GROUND-h,52,h,GRAY);wr(x+7,GROUND-h+14,9,12,YELLOW);wr(x+30,GROUND-h+14,9,12,YELLOW);wr(x+7,GROUND-h+39,9,12,YELLOW);wr(x+30,GROUND-h+39,9,12,YELLOW);wr(x+5,GROUND-h+62,42,4,STONE);}
 for(int x=20;x<WW;x+=105)rubble(x,GROUND-35,28+(x%3)*5);
 for(int x=100;x<WW-100;x+=260){wr(x,190,85,10,STONE);wr(x+45,150,95,10,STONE);arch(x+150,GROUND-80,58,80);}
 gate(WW-130,GROUND-105,70,105);
}
static void drawCastle(void){
 wr(0,0,WW,SH,0xFF20202D);wr(0,GROUND,WW,22,STONE);wr(0,GROUND+22,WW,SH-GROUND-22,DARK);
 for(int x=20;x<WW;x+=115){arch(x,105,60,GROUND-105);wr(x-5,95,70,8,GRAY);wr(x+20,75,18,20,LIGHT);}
 for(int x=45;x<WW-80;x+=155){wr(x,190,75,10,GRAY);wr(x+75,155,70,10,GRAY);wr(x+145,180,70,10,GRAY);}
 for(int x=140;x<WW-150;x+=350){wr(x,55,14,195,DARK);wr(x-8,48,30,9,LIGHT);tri(x+7,28,x-5,50,x+20,50,GRAY);}
 gate(WW-135,GROUND-105,70,105);
}
static void drawTower(void){
 wr(0,0,WW,SH,0xFF101827);wr(0,GROUND,WW,22,STONE);wr(0,GROUND+22,WW,SH-GROUND-22,DARK);wr(0,0,22,SH,DARK);wr(WW-22,0,22,SH,DARK);
 for(int x=40;x<WW;x+=115){wr(x,35,7,215,GRAY);wr(x+35,35,7,215,GRAY);}
 for(int x=70;x<WW-80;x+=170){int y=45+(x%4)*24;wr(x,y,105,7,PURPLE);wt(x+35,y,x+50,y-20,x+65,y,LIGHT);wr(x+70,y+7,6,GROUND-y-7,DARK);}
 for(int x=100;x<WW-100;x+=270){wr(x,195,80,10,STONE);wr(x+90,160,80,10,STONE);wr(x+180,125,80,10,STONE);crystal(x+195,85,22);}
 gate(WW-135,GROUND-105,70,105);
}
static void drawArena(void){
 wr(0,0,WW,SH,0xFF140D22);wr(0,GROUND,WW,22,PURPLE);wr(0,GROUND+22,WW,SH-GROUND-22,DARK);
 for(int x=20;x<WW;x+=65){wt(x,GROUND,x+22,GROUND-70-(x%4)*8,x+50,GROUND,DARK);wr(x+20,GROUND-8,4,8,YELLOW);}
 for(int x=100;x<WW-300;x+=360){wr(x,140,280,110,DARK);wr(x+12,150,256,5,STONE);for(int p=x+20;p<x+260;p+=48){wr(p,85,12,60,GRAY);wr(p-5,78,22,7,LIGHT);}wt(x+100,95,x+140,35,x+180,95,WHITE);}
 gate(WW-360,GROUND-105,60,105);gate(WW-130,GROUND-105,60,105);
}
static void drawScene(void){switch(zone){case CAVE:drawCave();break;case FOREST:drawForest();break;case CITY:drawCity();break;case CASTLE:drawCastle();break;case TOWER:drawTower();break;default:drawArena();break;}}
static void drawHUD(void){for(int i=0;i<ZONES;i++)rect(174+i*23,8,17,6,i==zone?WHITE:STONE);rect(8,8,100,5,DARK);rect(8,18,464,2,GRAY);}
static void drawFrame(void){sceGuStart(GU_DIRECT,guList);sceGuClearColor(BLACK);sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);sceGuDisable(GU_DEPTH_TEST);sceGuDisable(GU_TEXTURE_2D);drawScene();drawHUD();sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuSwapBuffers();}
int main(void){
 sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);sceGuInit();sceGuStart(GU_DIRECT,guList);
 sceGuDrawBuffer(GU_PSM_8888,(void*)0,512);sceGuDispBuffer(SW,SH,(void*)0x88000,512);sceGuDepthBuffer((void*)0x110000,512);
 sceGuOffset(2048-SW/2,2048-SH/2);sceGuViewport(2048,2048,SW,SH);sceGuDepthRange(65535,0);sceGuScissor(0,0,SW,SH);sceGuEnable(GU_SCISSOR_TEST);sceGuDisable(GU_DEPTH_TEST);sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuDisplay(GU_TRUE);
 SceCtrlData pad;while(1){sceCtrlReadBufferPositive(&pad,1);
  if(pad.Buttons&PSP_CTRL_LEFT)cameraX-=4.0f;if(pad.Buttons&PSP_CTRL_RIGHT)cameraX+=4.0f;
  if(cameraX<0)cameraX=0;if(cameraX>WW-SW)cameraX=WW-SW;
  if(pressed(pad.Buttons,previousButtons,PSP_CTRL_LTRIGGER))zone=(zone+ZONES-1)%ZONES;
  if(pressed(pad.Buttons,previousButtons,PSP_CTRL_RTRIGGER))zone=(zone+1)%ZONES;
  if(pressed(pad.Buttons,previousButtons,PSP_CTRL_START))break;
  drawFrame();previousButtons=pad.Buttons;sceKernelDelayThread(16667);
 }
 sceGuDisplay(GU_FALSE);sceGuTerm();sceKernelExitGame();return 0;
}
