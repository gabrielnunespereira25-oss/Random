/* RANDOM ALPHA - mapa contínuo monocromático para PSP / PSPSDK */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("RANDOM_ALPHA_MAP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define WORLD_W 7200.0f
#define WORLD_H 1800.0f
#define ZONE_W 1200.0f
#define ZONES 6

typedef unsigned int Color;
typedef struct { Color color; float x,y,z; } Vertex;
static unsigned int __attribute__((aligned(16))) guList[262144];
static float camX=0.0f, camY=700.0f;
static float playerX=120.0f, playerY=1080.0f;
static float playerVY=0.0f;
static int playerGrounded=0;
static unsigned int oldButtons=0;

/* Strict grayscale palette: black, white and gray only. */
#define BLACK 0xFF000000
#define DGRAY 0xFF292929
#define MGRAY 0xFF666666
#define LGRAY 0xFFB8B8B8
#define WHITE 0xFFFFFFFF

static int pressed(unsigned int now,unsigned int old,unsigned int b){return (now&b)&&!(old&b);}
static Color shade(int n){ if(n<=0)return BLACK; if(n==1)return DGRAY; if(n==2)return MGRAY; if(n==3)return LGRAY; return WHITE; }
static void rect(float x,float y,float w,float h,Color c){
    x-=camX; y-=camY;
    if(w<=0||h<=0||x>SW||y>SH||x+w<0||y+h<0)return;
    Vertex *v=(Vertex*)sceGuGetMemory(2*sizeof(Vertex));
    v[0]=(Vertex){c,x,y,0}; v[1]=(Vertex){c,x+w,y+h,0};
    sceGuDrawArray(GU_SPRITES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,0,v);
}
static void tri(float x1,float y1,float x2,float y2,float x3,float y3,Color c){
    x1-=camX;x2-=camX;x3-=camX;y1-=camY;y2-=camY;y3-=camY;
    Vertex *v=(Vertex*)sceGuGetMemory(3*sizeof(Vertex));
    v[0]=(Vertex){c,x1,y1,0};v[1]=(Vertex){c,x2,y2,0};v[2]=(Vertex){c,x3,y3,0};
    sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,3,0,v);
}
static void room(float x,float y,float w,float h,int style){
    /* open room outline with a deliberate doorway */
    rect(x,y,w,5,WHITE); rect(x,y+h-5,w,5,WHITE);
    rect(x,y,5,h,WHITE); rect(x+w-5,y,5,h,WHITE);
    rect(x+w*.35f,y+h-5,w*.18f,5,BLACK);
    if(style&1){rect(x+18,y+18, w*.22f,4,LGRAY);}
    if(style&2){rect(x+w*.62f,y+h*.45f,w*.18f,4,MGRAY);}
}
static void corridorH(float x,float y,float w){rect(x,y,w,5,WHITE);rect(x,y+34,w,5,WHITE);}
static void corridorV(float x,float y,float h){rect(x,y,5,h,WHITE);rect(x+34,y,5,h,WHITE);}
static void caveZone(float ox){
    /* tightly branching underground maze */
    corridorH(ox+20,1180,1120); corridorH(ox+80,1360,970); corridorH(ox+160,930,920);
    corridorV(ox+100,980,390); corridorV(ox+280,1180,260); corridorV(ox+470,950,410);
    corridorV(ox+690,1130,260); corridorV(ox+880,950,410); corridorV(ox+1040,1180,240);
    room(ox+60,1010,140,100,1); room(ox+320,1210,130,120,2); room(ox+520,980,150,120,3);
    room(ox+740,1210,120,100,1); room(ox+930,1000,150,110,2);
    corridorH(ox+200,1040,240); corridorH(ox+470,1260,230); corridorH(ox+690,1030,220);
    /* secret pocket */ room(ox+470,820,110,80,3); corridorV(ox+505,900,50);
    /* cave roof and hanging stone teeth */
    for(int i=0;i<8;i++)tri(ox+35+i*145,880,ox+55+i*145,930,ox+75+i*145,880,LGRAY);
}
static void forestZone(float ox){
    /* open sky above ground, roots and burrows below */
    rect(ox,700,ZONE_W,5,WHITE); rect(ox,1120,ZONE_W,5,WHITE);
    for(int i=0;i<7;i++){
        float x=ox+60+i*165; rect(x,520,9,180,WHITE);
        tri(x-45,565,x+5,440,x+55,565,LGRAY); tri(x-32,520,x+5,420,x+42,520,MGRAY);
        rect(x-20,700,48,5,WHITE); rect(x-5,705,5,90,LGRAY);
    }
    room(ox+70,780,170,100,1); room(ox+350,900,180,100,2); room(ox+760,780,180,100,3);
    corridorH(ox+150,1010,380); corridorV(ox+480,1000,115); corridorH(ox+480,1080,420);
    room(ox+900,970,180,100,2); corridorV(ox+1000,880,100);
    /* high sky platforms */ rect(ox+130,340,210,5,WHITE);rect(ox+420,270,130,5,LGRAY);
    rect(ox+690,390,180,5,WHITE); corridorV(ox+285,340,180); corridorV(ox+755,390,180);
}
static void cityZone(float ox){
    rect(ox,720,ZONE_W,5,WHITE); rect(ox,1120,ZONE_W,5,WHITE);
    for(int i=0;i<6;i++){
        float x=ox+35+i*190; float h=120+(i%3)*45;
        rect(x,720-h,130,h,WHITE); rect(x+12,735-h,24,24,LGRAY);rect(x+55,735-h,24,24,LGRAY);rect(x+92,735-h,24,24,LGRAY);
        rect(x+18,700,94,5,MGRAY);
    }
    corridorH(ox+30,850,1120); corridorH(ox+100,1000,950);
    corridorV(ox+180,850,150); corridorV(ox+410,850,150); corridorV(ox+680,850,150); corridorV(ox+930,850,150);
    room(ox+80,1160,180,120,1); room(ox+430,1190,160,100,2); room(ox+800,1150,190,130,3);
    corridorH(ox+240,1240,230); corridorH(ox+590,1260,230); corridorV(ox+710,1120,145);
}
static void castleZone(float ox){
    /* broken castle with upper battlements and deep dungeon */
    rect(ox+30,720,1120,5,WHITE); rect(ox+100,420,950,5,WHITE);
    for(int i=0;i<8;i++){rect(ox+70+i*145,390,45,30,WHITE);}
    room(ox+80,470,190,180,3); room(ox+350,500,220,150,1); room(ox+700,460,190,190,2); room(ox+960,510,150,140,1);
    corridorH(ox+260,570,110); corridorH(ox+560,590,150); corridorH(ox+890,560,80);
    corridorV(ox+165,650,170); corridorV(ox+445,650,210); corridorV(ox+785,650,170); corridorV(ox+1020,650,190);
    corridorH(ox+100,800,1000); corridorH(ox+180,970,870); corridorV(ox+280,800,175); corridorV(ox+620,800,175); corridorV(ox+900,800,175);
    room(ox+140,1180,190,120,1); room(ox+480,1210,220,100,2); room(ox+850,1170,200,140,3);
    corridorH(ox+330,1260,150); corridorH(ox+700,1250,150);
}
static void towerZone(float ox){
    /* vertical climb from underground foundations into sky */
    rect(ox+560,180,5,1450,WHITE);rect(ox+600,180,5,1450,WHITE);
    for(int i=0;i<10;i++){
        float y=1500-i*135; float x=(i%2)?ox+280:ox+610;
        rect(x,y,330,5,WHITE); rect(x+(i%2?0:300),y-70,5,75,LGRAY);
        room(x+(i%2?20:40),y-115,180,70,(i%3));
    }
    rect(ox+200,240,220,5,WHITE);rect(ox+760,300,250,5,WHITE);rect(ox+110,400,160,5,LGRAY);
    corridorV(ox+250,400,220); corridorV(ox+920,300,270);
    /* foundations beneath the tower */
    room(ox+70,1330,200,130,2);room(ox+850,1350,220,120,1);
    corridorH(ox+260,1410,590);corridorV(ox+550,1320,95);
}
static void arenaZone(float ox){
    /* isolated final area suspended above the void */
    rect(ox+100,680,1000,5,WHITE);rect(ox+170,900,860,5,WHITE);
    room(ox+300,470,600,200,3); room(ox+420,950,360,160,1);
    corridorV(ox+220,680,220); corridorV(ox+980,680,220);
    corridorH(ox+220,850,760); corridorV(ox+580,900,50);
    for(int i=0;i<9;i++)tri(ox+130+i*120,685,ox+150+i*120,730,ox+170+i*120,685,LGRAY);
    rect(ox+510,390,180,5,WHITE);rect(ox+550,350,100,5,LGRAY);
}
/* Human made from simple geometric shapes. Coordinates are world-space. */
static void circle(float cx, float cy, float r, Color c)
{
    int i;
    for (i = 0; i < 12; i++) {
        float a1 = (float)i * 6.2831853f / 12.0f;
        float a2 = (float)(i + 1) * 6.2831853f / 12.0f;
        tri(cx, cy,
            cx + r * (float)cosf(a1), cy + r * (float)sinf(a1),
            cx + r * (float)cosf(a2), cy + r * (float)sinf(a2), c);
    }
}
static void drawPlayer(void)
{
    float x = playerX, y = playerY;
    /* Head, neck, torso, arms and legs: monochrome geometric silhouette. */
    circle(x, y - 43, 9, WHITE);
    rect(x - 4, y - 34, 8, 6, LGRAY);
    rect(x - 10, y - 29, 20, 23, WHITE);
    tri(x - 10, y - 28, x - 17, y - 10, x - 9, y - 12, LGRAY);
    tri(x + 10, y - 28, x + 17, y - 10, x + 9, y - 12, LGRAY);
    rect(x - 9, y - 7, 7, 18, WHITE);
    rect(x + 2, y - 7, 7, 18, WHITE);
    /* small dark face mark keeps the head readable in grayscale */
    rect(x + 2, y - 45, 3, 3, DGRAY);
}

static void drawZoneLabel(int z){
    /* visual zone indicator: six monochrome markers, active marker is white */
    for(int i=0;i<ZONES;i++){
        rect(12+i*20,10,14,7,(i==z)?WHITE:MGRAY);
        rect(12+i*20,20,14,2,(i==z)?WHITE:DGRAY);
    }
    /* upper status line */ rect(8,5,132,2,WHITE);
}
static int currentZone(void){int z=(int)(camX/ZONE_W);if(z<0)z=0;if(z>=ZONES)z=ZONES-1;return z;}
static void drawWorld(void){
    sceGuStart(GU_DIRECT,guList); sceGuClearColor(BLACK); sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_TEXTURE_2D);
    /* World-space boundaries make each region part of one connected world. */
    rect(0,0,WORLD_W,WORLD_H,BLACK);
    caveZone(0);forestZone(ZONE_W);cityZone(ZONE_W*2);castleZone(ZONE_W*3);towerZone(ZONE_W*4);arenaZone(ZONE_W*5);
    /* zone joins: open passage bridges between the six areas */
    for(int i=1;i<ZONES;i++){
        float x=i*ZONE_W;
        corridorH(x-35,1060,70); corridorV(x-18,1020,45);
        corridorH(x-35,720,70);
    }
    drawPlayer();
    drawZoneLabel(currentZone());
    sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuSwapBuffers();
}
int main(void){
    sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceGuInit();sceGuStart(GU_DIRECT,guList);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,512);
    sceGuDispBuffer(SW,SH,(void*)0x88000,512);sceGuDepthBuffer((void*)0x110000,512);
    sceGuOffset(2048-SW/2,2048-SH/2);sceGuViewport(2048,2048,SW,SH);sceGuDepthRange(65535,0);
    sceGuScissor(0,0,SW,SH);sceGuEnable(GU_SCISSOR_TEST);sceGuDisable(GU_DEPTH_TEST);
    sceGuFinish();sceGuSync(0,0);sceDisplayWaitVblankStart();sceGuDisplay(GU_TRUE);
    SceCtrlData pad;
    while(1){
        sceCtrlReadBufferPositive(&pad,1);
        /* Character movement: D-pad left/right; X jumps. */
        if (pad.Buttons & PSP_CTRL_LEFT)  playerX -= 2.5f;
        if (pad.Buttons & PSP_CTRL_RIGHT) playerX += 2.5f;
        if (pressed(pad.Buttons, oldButtons, PSP_CTRL_CROSS) && playerGrounded) {
            playerVY = -8.0f;
            playerGrounded = 0;
        }

        /* Basic gravity and a simple ground plane for this map prototype. */
        playerVY += 0.28f;
        if (playerVY > 6.0f) playerVY = 6.0f;
        playerY += playerVY;
        if (playerY > 1080.0f) {
            playerY = 1080.0f;
            playerVY = 0.0f;
            playerGrounded = 1;
        }

        if (pressed(pad.Buttons, oldButtons, PSP_CTRL_LTRIGGER)) playerX -= 100.0f;
        if (pressed(pad.Buttons, oldButtons, PSP_CTRL_RTRIGGER)) playerX += 100.0f;
        if (pressed(pad.Buttons, oldButtons, PSP_CTRL_START)) break;

        if (playerX < 20.0f) playerX = 20.0f;
        if (playerX > WORLD_W - 20.0f) playerX = WORLD_W - 20.0f;

        /* Camera follows the player, keeping them near screen center. */
        camX = playerX - (SW * 0.5f);
        camY = playerY - (SH * 0.68f);
        if (camX < 0) camX = 0;
        if (camX > WORLD_W - SW) camX = WORLD_W - SW;
        if (camY < 0) camY = 0;
        if (camY > WORLD_H - SH) camY = WORLD_H - SH;

        drawWorld();
        oldButtons = pad.Buttons;
        sceKernelDelayThread(16667);
    }
    sceGuDisplay(GU_FALSE);sceGuTerm();sceKernelExitGame();return 0;
}
