/*
 * RANDOM ALPHA - LABIRINTO VERTICAL PRETO E BRANCO
 * PSP / PSPSDK - SOMENTE MAPA, SEM PERSONAGEM OU COMBATE
 * Direcional UP/DOWN: percorre verticalmente a regiao
 * Direcional LEFT/RIGHT: muda de regiao
 * START: sair
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>

PSP_MODULE_INFO("RANDOM_MAPA_PB", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define MAP_W 900
#define MAP_H 1200
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
static int zone = CAVE;
static int cameraY = 0;
static unsigned int previousButtons = 0;

#define BLACK 0xFF08080A
#define VOIDC 0xFF17171B
#define DARK  0xFF29292F
#define MID   0xFF55555D
#define LIGHT 0xFFBDBDC4
#define WHITE 0xFFF5F5F5

static int pressed(unsigned int now, unsigned int old, unsigned int b) {
    return (now & b) && !(old & b);
}
static void rect(float x,float y,float w,float h,Color c) {
    if(w<=0 || h<=0) return;
    Vertex *v=(Vertex*)sceGuGetMemory(2*sizeof(Vertex));
    v[0]=(Vertex){c,x,y,0}; v[1]=(Vertex){c,x+w,y+h,0};
    sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
}
static void tri(float x1,float y1,float x2,float y2,float x3,float y3,Color c) {
    Vertex *v=(Vertex*)sceGuGetMemory(3*sizeof(Vertex));
    v[0]=(Vertex){c,x1,y1,0}; v[1]=(Vertex){c,x2,y2,0}; v[2]=(Vertex){c,x3,y3,0};
    sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,3,0,v);
}
static void R(float x,float y,float w,float h,Color c) { rect(x,y-cameraY,w,h,c); }
static void spikeCeiling(int x,int y,int count,int step,int height) {
    int i; for(i=0;i<count;i++) tri(x+i*step,y,x+i*step+step/2,y+height,x+(i+1)*step,y,MID);
}
static void doorway(int x,int y,int w,int h) {
    R(x,y,7,h,MID); R(x+w-7,y,7,h,MID); R(x,y,w,7,LIGHT);
    R(x+12,y+12,w-24,h-12,BLACK);
}
static void platform(int x,int y,int w) { R(x,y,w,7,LIGHT); R(x,y+7,w,4,DARK); }
static void caveRoom(int x,int y,int w,int h) {
    R(x,y,w,8,MID); R(x,y,8,h,MID); R(x+w-8,y,8,h,MID); R(x,y+h-8,w,8,MID);
    /* Pequenas quebras na pedra, sem excesso de detalhe. */
    R(x+18,y+24,18,3,DARK); R(x+w-35,y+h-28,17,3,DARK);
}
static void drawCave(void) {
    rect(0,0,SW,SH,BLACK);
    /* Coluna principal do labirinto e salas laterais interligadas. */
    caveRoom(285,20,330,220);
    caveRoom(65,255,320,210);
    caveRoom(390,270,420,190);
    caveRoom(180,490,350,210);
    caveRoom(535,485,275,230);
    caveRoom(65,730,360,190);
    caveRoom(440,750,370,200);
    caveRoom(245,970,370,200);
    /* Passagens entre as câmaras. */
    R(420,232,55,35,BLACK); R(340,250,50,35,BLACK);
    R(350,440,55,45,BLACK); R(565,450,45,40,BLACK);
    R(300,680,55,50,BLACK); R(580,700,45,40,BLACK);
    R(360,900,70,55,BLACK); R(520,950,55,35,BLACK);
    /* Plataformas alternadas para sugerir subida e descida pelo labirinto. */
    platform(315,65,95); platform(485,115,100); platform(105,305,105);
    platform(230,365,115); platform(510,315,130); platform(650,400,95);
    platform(225,535,100); platform(365,600,120); platform(590,540,115);
    platform(105,770,125); platform(270,830,100); platform(530,800,135);
    platform(300,1010,110); platform(465,1080,100);
    /* Fendas, estalactites e um nicho secreto. */
    spikeCeiling(300,28,12,24,20); spikeCeiling(80,263,10,26,18);
    spikeCeiling(545,493,9,26,18); spikeCeiling(75,738,12,25,19);
    R(95,570,65,5,LIGHT); R(95,575,5,55,MID); R(155,575,5,55,MID);
    R(95,625,65,5,LIGHT); /* sala secreta */
    /* Escadaria de blocos na coluna central. */
    for(int i=0;i<8;i++) platform(400+(i%2)*28,170+i*105,55);
}
static void drawForest(void) {
    rect(0,0,SW,SH,BLACK);
    /* Floresta vertical: troncos como silhuetas e plataformas-raiz. */
    R(70,15,8,1160,MID); R(155,90,8,900,DARK); R(730,30,8,1100,MID);
    R(650,180,8,870,DARK); R(250,310,8,780,MID); R(480,80,8,650,DARK);
    for(int y=70;y<MAP_H;y+=170) {
        tri(25,y+65,110,y-5,195,y+65,DARK);
        tri(600,y+80,690,y-10,780,y+80,DARK);
        platform(110,y+95,115); platform(500,y+125,110);
    }
    caveRoom(280,80,290,175); caveRoom(300,420,270,180);
    caveRoom(280,760,300,180); doorway(385,240,80,70);
    doorway(385,585,80,70); doorway(385,925,80,70);
}
static void drawCity(void) {
    rect(0,0,SW,SH,BLACK);
    for(int y=35;y<MAP_H;y+=205) {
        caveRoom(45,y,180,170); caveRoom(270,y+25,175,145);
        caveRoom(490,y,190,170); caveRoom(710,y+25,145,145);
        for(int x=70;x<820;x+=110) {
            R(x,y+35,12,20,LIGHT); R(x+25,y+35,12,20,MID);
        }
        platform(80,y+115,95); platform(300,y+100,110);
        platform(520,y+120,115); platform(720,y+95,90);
    }
    /* Pontes estreitas entre ruínas suspensas. */
    for(int y=190;y<MAP_H;y+=205) { R(220,y,55,6,LIGHT); R(445,y+20,45,6,LIGHT); R(680,y,32,6,LIGHT); }
}
static void drawCastle(void) {
    rect(0,0,SW,SH,BLACK);
    R(30,0,8,MAP_H,MID); R(842,0,8,MAP_H,MID);
    for(int y=20;y<MAP_H;y+=180) {
        caveRoom(90,y,300,145); caveRoom(455,y+30,330,145);
        doorway(345,y+70,115,65);
        for(int x=110;x<370;x+=48) { R(x,y-8,28,10,LIGHT); tri(x,y-8,x+14,y-27,x+28,y-8,MID); }
        platform(125,y+95,105); platform(280,y+55,80);
        platform(510,y+115,120); platform(680,y+75,75);
    }
    /* Janelas altas e corredores estreitos. */
    for(int y=75;y<MAP_H;y+=180) { R(205,y,14,28,LIGHT); R(235,y,14,28,MID); }
}
static void drawTower(void) {
    rect(0,0,SW,SH,BLACK);
    R(35,0,10,MAP_H,MID); R(835,0,10,MAP_H,MID);
    for(int y=20;y<MAP_H;y+=125) {
        platform(55,y+80,170); platform(255,y+25,150);
        platform(465,y+90,150); platform(655,y+40,170);
        R(105,y+80,7,45,MID); R(320,y+25,7,55,MID);
        R(535,y+90,7,35,MID); R(730,y+40,7,50,MID);
        tri(75,y+80,100,y+45,125,y+80,DARK);
    }
    /* Vãos verticais e pequenas salas laterais. */
    for(int y=150;y<MAP_H;y+=250) { caveRoom(290,y,300,95); doorway(400,y+30,80,65); }
}
static void drawArena(void) {
    rect(0,0,SW,SH,BLACK);
    R(45,0,8,MAP_H,MID); R(827,0,8,MAP_H,MID);
    for(int y=30;y<MAP_H;y+=220) {
        caveRoom(90,y,700,175);
        platform(145,y+120,120); platform(320,y+70,110);
        platform(500,y+95,115); platform(665,y+125,80);
        tri(120,y+5,150,y-45,180,y+5,MID);
        tri(680,y+5,710,y-45,740,y+5,MID);
        R(430,y+8,12,30,LIGHT); R(450,y+8,12,30,MID);
    }
}
static void drawZone(void) {
    switch(zone) {
        case CAVE: drawCave(); break;
        case FOREST: drawForest(); break;
        case CITY: drawCity(); break;
        case CASTLE: drawCastle(); break;
        case TOWER: drawTower(); break;
        default: drawArena(); break;
    }
    /* Indicador simples das seis regioes. */
    for(int i=0;i<ZONES;i++) rect(190+i*19,8,13,5,(i==zone)?WHITE:MID);
    rect(8,8,70,4,DARK);
}
static void drawFrame(void) {
    sceGuStart(GU_DIRECT,guList);
    sceGuClearColor(BLACK); sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_TEXTURE_2D);
    drawZone(); sceGuFinish(); sceGuSync(0,0);
    sceDisplayWaitVblankStart(); sceGuSwapBuffers();
}
int main(void) {
    sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceGuInit(); sceGuStart(GU_DIRECT,guList);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,512);
    sceGuDispBuffer(SW,SH,(void*)0x88000,512);
    sceGuDepthBuffer((void*)0x110000,512);
    sceGuOffset(2048-SW/2,2048-SH/2); sceGuViewport(2048,2048,SW,SH);
    sceGuDepthRange(65535,0); sceGuScissor(0,0,SW,SH); sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST); sceGuFinish(); sceGuSync(0,0);
    sceDisplayWaitVblankStart(); sceGuDisplay(GU_TRUE);
    SceCtrlData pad;
    while(1) {
        sceCtrlReadBufferPositive(&pad,1);
        if(pressed(pad.Buttons,previousButtons,PSP_CTRL_LEFT)) { zone=(zone+ZONES-1)%ZONES; cameraY=0; }
        if(pressed(pad.Buttons,previousButtons,PSP_CTRL_RIGHT)) { zone=(zone+1)%ZONES; cameraY=0; }
        if(pad.Buttons&PSP_CTRL_UP) { cameraY-=3; if(cameraY<0) cameraY=0; }
        if(pad.Buttons&PSP_CTRL_DOWN) { cameraY+=3; if(cameraY>MAP_H-SH) cameraY=MAP_H-SH; }
        if(pressed(pad.Buttons,previousButtons,PSP_CTRL_START)) break;
        drawFrame(); previousButtons=pad.Buttons; sceKernelDelayThread(16667);
    }
    sceGuDisplay(GU_FALSE); sceGuTerm(); sceKernelExitGame(); return 0;
}
