/*
 * RANDOM ALPHA - MAPA APENAS PRETO E BRANCO (PSP / PSPSDK)
 * Versao de teste visual: seis regioes, cenarios e plataformas.
 * Controles: ESQUERDA/DIREITA troca a visualizacao de zona; START sai.
 * Sem personagem, inimigos, combate ou habilidades nesta etapa.
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>

PSP_MODULE_INFO("RANDOM_MAPA", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define WW 1600
#define GROUND 250
#define ZONES 6
#define CAVE 0
#define FOREST 1
#define CITY 2
#define CASTLE 3
#define TOWER 4
#define ARENA 5

typedef unsigned int Color;
typedef struct { Color color; float x, y, z; } Vertex;
static unsigned int __attribute__((aligned(16))) guList[262144];
static int zone = CAVE;
static unsigned int previousButtons = 0;

#define BLACK  0xFF101018
#define DARK   0xFF252535
#define STONE  0xFF555568
#define GRAY   0xFF858596
#define LIGHT  0xFFBFC4D0
#define WHITE  0xFFFFFFFF
#define LIGHT  0xFFFFFFFF
#define DLIGHT 0xFF777777
#define LIGHT 0xFFBBBBBB
#define WHITE 0xFFFFFFFF
#define DARK  0xFF333333

static int pressed(unsigned int now, unsigned int old, unsigned int button) {
    return (now & button) && !(old & button);
}

static void rect(float x, float y, float w, float h, Color color) {
    if (w <= 0 || h <= 0) return;
    Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
    v[0] = (Vertex){color, x, y, 0};
    v[1] = (Vertex){color, x + w, y + h, 0};
    sceGuDrawArray(GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, 0, v);
}

static void tri(float x1,float y1,float x2,float y2,float x3,float y3,Color c) {
    Vertex *v = (Vertex *)sceGuGetMemory(3 * sizeof(Vertex));
    v[0] = (Vertex){c,x1,y1,0};
    v[1] = (Vertex){c,x2,y2,0};
    v[2] = (Vertex){c,x3,y3,0};
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        3, 0, v);
}

static void arch(float x,float y,float w,float h) {
    rect(x,y,10,h,STONE); rect(x+w-10,y,10,h,STONE);
    rect(x,y,w,8,GRAY);
    tri(x-4,y+8,x+5,y-8,x+14,y+8,LIGHT);
    tri(x+w-14,y+8,x+w-5,y-8,x+w+4,y+8,LIGHT);
}

static void crystal(float x,float y,float s) {
    tri(x,y+s,x+s*.5f,y,x+s,y+s,LIGHT);
    tri(x+s*.5f,y,x+s,y+s,x+s*.5f,y+s*1.35f,WHITE);
    tri(x,y+s,x+s*.5f,y+s*1.35f,x+s*.5f,y,GRAY);
}

static void gate(float x,float y,float w,float h) {
    rect(x,y,8,h,DARK); rect(x+w-8,y,8,h,DARK); rect(x,y,w,8,GRAY);
    for (int i=0;i<6;i++) rect(x+14+i*((w-28)/6.0f),y+12,3,h-14,STONE);
}

static void rubble(float x,float y,float s) {
    tri(x,y+s,x+s*.4f,y,x+s,y+s,DARK);
    rect(x+s*.25f,y+s*.65f,s*.6f,s*.2f,GRAY);
}

static void drawCave(void) {
    rect(0,0,SW,SH,BLACK); rect(0,0,SW,GROUND,DARK);
    rect(0,GROUND,SW,22,STONE); rect(0,GROUND+22,SW,SH-GROUND-22,GRAY);
    rect(0,0,SW,24,STONE);
    for(int x=25;x<SW;x+=65){
        tri(x,0,x+8,18+(x%5)*3,x+17,0,GRAY);
        rect(x+20,45,5,45+(x%4)*8,DARK);
    }
    rect(50,190,100,10,STONE); rect(190,160,110,10,STONE);
    rect(330,190,100,10,STONE); rect(345,135,70,10,GRAY);
    rect(65,210,12,40,STONE); rect(125,145,12,105,STONE);
    rect(137,145,72,8,GRAY); rect(205,145,12,48,STONE);
    rect(225,205,12,45,STONE); rect(270,120,12,130,STONE);
    rect(282,120,55,8,GRAY); rect(330,160,12,90,STONE);
    rect(360,150,80,8,GRAY); rect(420,95,12,60,STONE);
    rect(130,55,100,8,LIGHT); rect(130,55,8,45,LIGHT); rect(222,55,8,45,LIGHT);
    crystal(300,GROUND-53,20); crystal(370,GROUND-55,18);
    gate(400,GROUND-110,70,110);
    rect(280,100,45,40,LIGHT); rect(291,92,24,50,WHITE);
}

static void drawForest(void) {
    rect(0,0,SW,SH,0xFF101010); rect(0,GROUND,SW,22,DLIGHT);
    rect(0,GROUND+22,SW,SH-GROUND-22,DARK);
    for(int x=20;x<SW;x+=62){
        rect(x+20,GROUND-80-(x%3)*8,10,80+(x%3)*8,DARK);
        tri(x-2,GROUND-70-(x%3)*8,x+25,GROUND-140-(x%4)*5,x+54,GROUND-70-(x%3)*8,DLIGHT);
        tri(x+4,GROUND-95,x+25,GROUND-155,x+46,GROUND-95,LIGHT);
    }
    rect(55,190,90,10,DARK); rect(190,160,90,10,DARK);
    rect(320,190,100,10,DARK); rect(380,150,65,10,DARK);
    crystal(405,65,20); rect(40,185,22,65,WHITE);
}

static void drawCity(void) {
    rect(0,0,SW,SH,0xFF181818); rect(0,GROUND,SW,22,STONE);
    rect(0,GROUND+22,SW,SH-GROUND-22,DARK);
    for(int x=15;x<SW;x+=58){
        int h=85+(x%5)*18; rect(x,GROUND-h,43,h,GRAY);
        rect(x+7,GROUND-h+14,8,12,WHITE); rect(x+27,GROUND-h+14,8,12,WHITE);
        rect(x+7,GROUND-h+38,8,12,WHITE); rect(x+27,GROUND-h+38,8,12,WHITE);
    }
    for(int x=20;x<SW;x+=95) rubble(x,GROUND-35,28+(x%3)*5);
    rect(70,190,90,10,STONE); rect(220,155,100,10,STONE); rect(365,185,80,10,STONE);
}

static void drawCastle(void) {
    rect(0,0,SW,SH,0xFF202020); rect(0,GROUND,SW,22,STONE);
    rect(0,GROUND+22,SW,SH-GROUND-22,DARK);
    for(int x=20;x<SW;x+=78){
        arch(x,105,52,GROUND-105);
        rect(x-5,95,62,8,GRAY);
    }
    rect(45,190,75,10,GRAY); rect(165,155,75,10,GRAY);
    rect(280,180,85,10,GRAY); rect(380,135,70,10,GRAY);
    gate(390,GROUND-105,65,105);
}

static void drawTower(void) {
    rect(0,0,SW,SH,0xFF080808); rect(0,GROUND,SW,22,STONE);
    rect(0,GROUND+22,SW,SH-GROUND-22,DARK);
    rect(0,0,22,SH,DARK); rect(SW-22,0,22,SH,DARK);
    for(int y=40;y<220;y+=48){
        rect(65+(y%3)*45,y,110,7,LIGHT);
        tri(190,y+7,202,y-12,214,y+7,LIGHT);
    }
    for(int x=40;x<SW;x+=95){rect(x,35,7,215,GRAY);rect(x+35,35,7,215,GRAY);}
    rect(75,195,80,10,STONE); rect(205,165,80,10,STONE);
    rect(320,125,85,10,STONE); crystal(400,45,24); gate(390,GROUND-105,65,105);
}

static void drawArena(void) {
    rect(0,0,SW,SH,0xFF000000); rect(0,GROUND,SW,22,LIGHT);
    rect(0,GROUND+22,SW,SH-GROUND-22,DARK);
    for(int x=20;x<SW;x+=55){tri(x,GROUND,x+22,GROUND-70-(x%4)*8,x+50,GROUND,DARK);rect(x+20,GROUND-8,4,8,WHITE);}
    rect(100,140,280,110,DARK); rect(112,150,256,5,STONE);
    for(int x=120;x<370;x+=48){rect(x,85,12,60,GRAY);rect(x-5,78,22,7,LIGHT);}
    tri(200,95,240,35,280,95,WHITE); gate(90,GROUND-105,60,105); gate(330,GROUND-105,60,105);
}

static void drawZone(void) {
    switch(zone){
        case CAVE: drawCave(); break;
        case FOREST: drawForest(); break;
        case CITY: drawCity(); break;
        case CASTLE: drawCastle(); break;
        case TOWER: drawTower(); break;
        default: drawArena(); break;
    }
    /* Indicador visual da regiao atual, sem texto nem personagem. */
    for(int i=0;i<ZONES;i++) rect(174+i*23,8,17,6,(i==zone)?WHITE:STONE);
    rect(8,8,100,5,DARK); rect(8,18,464,2,GRAY);
}

static void drawFrame(void) {
    sceGuStart(GU_DIRECT, guList);
    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    drawZone();
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

int main(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceGuInit();
    sceGuStart(GU_DIRECT, guList);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, 512);
    /* Offsets convencionais de framebuffer para PSP. */
    sceGuDispBuffer(SW, SH, (void*)0x88000, 512);
    sceGuDepthBuffer((void*)0x110000, 512);
    sceGuOffset(2048-SW/2, 2048-SH/2);
    sceGuViewport(2048,2048,SW,SH);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,SW,SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData pad;
    while(1) {
        sceCtrlReadBufferPositive(&pad,1);
        if(pressed(pad.Buttons,previousButtons,PSP_CTRL_LEFT)) zone=(zone+ZONES-1)%ZONES;
        if(pressed(pad.Buttons,previousButtons,PSP_CTRL_RIGHT)) zone=(zone+1)%ZONES;
        if(pressed(pad.Buttons,previousButtons,PSP_CTRL_START)) break;
        drawFrame();
        previousButtons=pad.Buttons;
        sceKernelDelayThread(16667);
    }
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
