/*
 * RANDOM ALPHA
 * Plataforma 2D para PSP / PSPSDK
 * Graficos desenhados em C, sem imagens externas.
 *
 * ZONAS:
 * 0 - Caverna do Labirinto
 * 1 - Floresta Esquecida
 * 2 - Cidade Abandonada
 * 3 - Castelo Caido
 * 4 - Torre Antiga
 * 5 - Arena Final
 *
 * CHEFES:
 * EIDOLON, o Fragmento
 * VERTICE, o Devorador
 * ???, o Desperto
 * NEXUS, o Ultimo Silencio
 *
 * CONTROLES:
 * Direcional: movimentar / navegar
 * X: pular / confirmar
 * O: magia / voltar
 * START: melhorias
 * SELECT: menu principal
 * L: zona anterior (teste)
 * R: proxima zona antes do dash; dash depois.
 *
 * Requer PSPSDK.
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspge.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

PSP_MODULE_INFO("RANDOM ALPHA", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512
#define WORLD_W 1600
#define WORLD_H 600
#define GROUND 500

#define MAX_PLATFORMS 40
#define MAX_ENEMIES 8
#define MAX_SHOTS 8
#define PLAYER_W 19
#define PLAYER_H 30
#define MAX_LEVEL 50

typedef unsigned int Color;

typedef struct {
    float x, y, w, h;
} Platform;

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, vx;
    int hp, active;
} Enemy;

typedef struct {
    float x, y, vx;
    int active;
} Shot;

#define BLACK 0xFF080808
#define DARK  0xFF292929
#define GRAY  0xFF666666
#define LIGHT 0xFFAAAAAA
#define WHITE 0xFFF0F0F0

static unsigned int __attribute__((aligned(16)))
    guList[262144];

static Platform platforms[MAX_PLATFORMS];
static Enemy enemies[MAX_ENEMIES];
static Shot shots[MAX_SHOTS];

static int platformCount;
static int zone;
static int running = 1;

static int menuMode = 0;
static int menuSelection = 0;
static int upgradeSelection = 0;
static int gameStarted = 0;

static float playerX = 40;
static float playerY = GROUND - PLAYER_H;
static float velocityY;
static float cameraX;

static int grounded;
static int playerFacing = 1;
static int playerFrame;
static int invulnerability;

static int doubleJumpUnlocked;
static int doubleJumpUsed;
static int dashUnlocked;
static int dashCooldown;
static int dashFrames;

static int maxHP = 8;
static int hp = 8;
static int damage = 2;
static int defense = 1;
static int speed = 3;

static int level = 1;
static int xp;
static int nextXP = 100;
static int upgradePoints;

static int magicUnlocked;
static int magicTaken;
static float magicX = 310;
static float magicY = 420;
static int shotCooldown;
static int itemPulse;

static int bossActive;
static int bossHP;
static int bossMaxHP;
static float bossX;
static float bossY;
static int bossTimer;
static int bossPhase;
static int bossDefeated[4];

static unsigned int previousButtons;

/* =====================================================
   FUNCOES BASICAS
   ===================================================== */

static int pressed(unsigned int b, unsigned int key)
{
    return (b & key) && !(previousButtons & key);
}

static float clampf(float n, float low, float high)
{
    if (n < low) return low;
    if (n > high) return high;
    return n;
}

static int overlap(float ax, float ay, float aw, float ah,
                   float bx, float by, float bw, float bh)
{
    return ax < bx + bw &&
           ax + aw > bx &&
           ay < by + bh &&
           ay + ah > by;
}

static void rect(float x, float y, float w, float h, Color c)
{
    Vertex *v;

    if (w <= 0 || h <= 0)
        return;

    v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 2);

    v[0].color = c;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].color = c;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v
    );
}

static void tri(float x1, float y1,
                float x2, float y2,
                float x3, float y3, Color c)
{
    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 3);

    v[0].color = c;
    v[0].x = x1; v[0].y = y1; v[0].z = 0;
    v[1].color = c;
    v[1].x = x2; v[1].y = y2; v[1].z = 0;
    v[2].color = c;
    v[2].x = x3; v[2].y = y3; v[2].z = 0;

    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        3, 0, v
    );
}

/* Fonte simples: numeros e letras maiusculas. */
static const unsigned char font[36][5] = {
    {62,81,73,69,62},{0,66,127,64,0},
    {66,97,81,73,70},{33,65,69,75,49},
    {24,20,18,127,16},{39,69,69,69,57},
    {60,74,73,73,48},{1,113,9,5,3},
    {54,73,73,73,54},{6,73,73,41,30},
    {126,17,17,17,126},{127,73,73,73,54},
    {62,65,65,65,34},{127,65,65,34,28},
    {127,73,73,73,65},{127,9,9,9,1},
    {62,65,73,73,122},{127,8,8,8,127},
    {0,65,127,65,0},{32,64,65,63,1},
    {127,8,20,34,65},{127,64,64,64,64},
    {127,2,12,2,127},{127,4,8,16,127},
    {62,65,65,65,62},{127,9,9,9,6},
    {62,65,81,33,94},{127,9,25,41,70},
    {70,73,73,73,49},{1,1,127,1,1},
    {63,64,64,64,63},{31,32,64,32,31},
    {63,64,56,64,63},{99,20,8,20,99},
    {7,8,112,8,7},{97,81,73,69,67}
};

static void drawText(const char *s, int x, int y, Color c)
{
    int i, col, row;
    char ch;

    while (*s) {
        ch = *s++;

        if (ch >= 'a' && ch <= 'z')
            ch = ch - 'a' + 'A';

        if (ch == ' ') {
            x += 6;
            continue;
        }

        if (ch >= '0' && ch <= '9')
            i = ch - '0';
        else if (ch >= 'A' && ch <= 'Z')
            i = 10 + ch - 'A';
        else {
            x += 6;
            continue;
        }

        for (col = 0; col < 5; col++) {
            for (row = 0; row < 7; row++) {
                if (font[i][col] & (1 << row))
                    rect(x + col, y + row, 1, 1, c);
            }
        }

        x += 6;
    }
}

/* =====================================================
   MENU PRINCIPAL
   ===================================================== */

static void drawMainMenu(void)
{
    rect(0, 0, SW, SH, BLACK);
    rect(36, 24, 408, 224, GRAY);
    rect(40, 28, 400, 216, BLACK);

    drawText("RANDOM", 204, 43, WHITE);
    drawText("ALPHA", 216, 61, LIGHT);
    rect(110, 82, 260, 2, GRAY);

    if (menuSelection == 0) rect(125,103,4,9,WHITE);
    if (menuSelection == 1) rect(125,125,4,9,WHITE);
    if (menuSelection == 2) rect(125,147,4,9,WHITE);
    if (menuSelection == 3) rect(125,169,4,9,WHITE);

    drawText("NOVO JOGO",140,104,
             menuSelection == 0 ? WHITE : LIGHT);
    drawText("CONTINUAR",140,126,
             menuSelection == 1 ? WHITE : LIGHT);
    drawText("MELHORIAS",140,148,
             menuSelection == 2 ? WHITE : LIGHT);
    drawText("SAIR",140,170,
             menuSelection == 3 ? WHITE : LIGHT);

    drawText("DIRECIONAL ESCOLHER",126,204,LIGHT);
    drawText("X CONFIRMAR",156,220,WHITE);
}

/* =====================================================
   PLATAFORMAS
   ===================================================== */

static void addPlatform(float x, float y, float w, float h)
{
    if (platformCount >= MAX_PLATFORMS)
        return;

    platforms[platformCount].x = x;
    platforms[platformCount].y = y;
    platforms[platformCount].w = w;
    platforms[platformCount].h = h;
    platformCount++;
}

static void buildPlatforms(void)
{
    platformCount = 0;
    addPlatform(0, GROUND, WORLD_W, 100);

    switch (zone) {
    case 0:
        addPlatform(130,445,130,14);
        addPlatform(300,400,110,14);
        addPlatform(440,350,120,14);
        addPlatform(590,310,100,14);
        addPlatform(730,360,130,14);
        addPlatform(900,320,120,14);
        addPlatform(1050,270,120,14);
        addPlatform(1210,330,120,14);
        addPlatform(1370,420,110,14);
        addPlatform(220,290,90,14);
        addPlatform(390,245,90,14);
        addPlatform(570,220,100,14);
        addPlatform(780,250,100,14);
        addPlatform(1000,205,100,14);
        break;

    case 1:
        addPlatform(130,440,120,14);
        addPlatform(290,390,120,14);
        addPlatform(450,345,130,14);
        addPlatform(620,400,120,14);
        addPlatform(790,350,120,14);
        addPlatform(960,390,130,14);
        addPlatform(1120,440,360,14);
        addPlatform(200,300,90,14);
        addPlatform(390,255,100,14);
        addPlatform(580,285,100,14);
        addPlatform(780,235,100,14);
        addPlatform(1000,285,100,14);
        break;

    case 2:
        addPlatform(140,440,130,14);
        addPlatform(320,390,120,14);
        addPlatform(490,335,130,14);
        addPlatform(680,400,120,14);
        addPlatform(850,345,120,14);
        addPlatform(1020,300,130,14);
        addPlatform(1200,390,120,14);
        addPlatform(1390,430,100,14);
        addPlatform(250,280,100,14);
        addPlatform(470,230,100,14);
        addPlatform(710,260,100,14);
        addPlatform(930,220,100,14);
        addPlatform(1170,250,100,14);
        break;

    case 3:
        addPlatform(130,440,120,14);
        addPlatform(300,390,110,14);
        addPlatform(460,340,110,14);
        addPlatform(630,290,110,14);
        addPlatform(800,350,110,14);
        addPlatform(970,300,110,14);
        addPlatform(1120,440,360,14);
        addPlatform(230,280,90,14);
        addPlatform(470,220,90,14);
        addPlatform(720,205,90,14);
        addPlatform(950,220,90,14);
        break;

    case 4:
        addPlatform(120,440,120,14);
        addPlatform(290,385,110,14);
        addPlatform(460,330,110,14);
        addPlatform(630,275,110,14);
        addPlatform(800,220,110,14);
        addPlatform(970,275,110,14);
        addPlatform(1120,440,360,14);
        addPlatform(210,300,80,14);
        addPlatform(390,245,80,14);
        addPlatform(570,190,80,14);
        addPlatform(750,145,80,14);
        addPlatform(930,160,80,14);
        break;

    case 5:
        addPlatform(0,440,WORLD_W,60);
        addPlatform(150,410,130,14);
        addPlatform(350,390,130,14);
        addPlatform(550,410,130,14);
        addPlatform(750,390,130,14);
        addPlatform(950,410,130,14);
        addPlatform(1150,390,130,14);
        addPlatform(1350,410,130,14);
        addPlatform(300,300,100,14);
        addPlatform(600,270,100,14);
        addPlatform(900,300,100,14);
        addPlatform(1200,270,100,14);
        break;
    }
}

static void drawPlatforms(void)
{
    int i;

    for (i = 1; i < platformCount; i++) {
        float x = platforms[i].x - cameraX;

        if (x > -platforms[i].w && x < SW) {
            rect(x,platforms[i].y,platforms[i].w,
                 platforms[i].h,GRAY);
            rect(x,platforms[i].y,platforms[i].w,3,WHITE);
        }
    }
}

/* =====================================================
   CENARIOS
   ===================================================== */

static void drawTree(float x, float base, float size)
{
    float tw = size * 0.18f;
    x -= cameraX;

    rect(x,base-size,tw,size,DARK);
    tri(x-size*.48f,base-size*.62f,
        x+tw*.5f,base-size*1.48f,
        x+size*.68f,base-size*.62f,DARK);
    tri(x-size*.35f,base-size*.9f,
        x+tw*.5f,base-size*1.62f,
        x+size*.46f,base-size*.9f,GRAY);
}

static void drawCrystal(float x, float y)
{
    x -= cameraX;
    tri(x,y+28,x+12,y,x+24,y+28,LIGHT);
    tri(x+12,y,x+24,y+28,x+12,y+38,WHITE);
    tri(x,y+28,x+12,y+38,x+12,y,GRAY);
}

static void drawGate(float x, float y)
{
    int i;
    x -= cameraX;

    rect(x,y,10,95,DARK);
    rect(x+72,y,10,95,DARK);
    rect(x,y,82,10,GRAY);

    for (i=0; i<5; i++)
        rect(x+15+i*12,y+12,3,78,LIGHT);
}

static void drawSign(float x, float y, int right)
{
    x -= cameraX;
    rect(x,y,4,36,DARK);
    rect(x-12,y,32,14,LIGHT);

    if (right)
        tri(x+3,y+2,x+3,y+12,x+13,y+7,BLACK);
    else
        tri(x-7,y+7,x+3,y+2,x+3,y+12,BLACK);
}

static void drawCave(void)
{
    int i;

    rect(0,0,SW,SH,BLACK);

    for (i=0; i<16; i++) {
        float x=i*105.0f-cameraX;
        tri(x,0,x+52,65,x+105,0,DARK);
        tri(x+10,0,x+52,42,x+82,0,GRAY);
    }

    for (i=0; i<18; i++) {
        float x=i*91.0f+20-cameraX;
        tri(x,0,x+12,25+(i%4)*13,x+24,0,GRAY);
    }

    for (i=0; i<20; i++)
        rect(i*82.0f-cameraX,130+(i%3)*55,34,5,DARK);

    drawCrystal(565,455);
    drawCrystal(595,455);
    drawCrystal(1030,455);

    drawPlatforms();
    drawSign(30,455,0);
    drawSign(1510,455,1);
}

static void drawForest(void)
{
    int i;

    rect(0,0,SW,SH,BLACK);
    rect(1090-cameraX,45,42,42,LIGHT);
    rect(1100-cameraX,38,35,35,BLACK);

    for (i=0; i<18; i++)
        drawTree(i*96.0f,450,70+(i%3)*12);

    drawTree(30,GROUND,125);
    drawTree(390,GROUND,110);
    drawTree(760,GROUND,135);
    drawTree(1080,GROUND,115);
    drawTree(1500,GROUND,130);

    for (i=0; i<20; i++) {
        float x=i*80.0f-cameraX;
        tri(x,GROUND,x+20,GROUND-25,x+40,GROUND,GRAY);
    }

    drawPlatforms();
    rect(1120-cameraX,390,360,110,DARK);
    rect(1120-cameraX,390,360,5,LIGHT);
    drawTree(1135,390,85);
    drawTree(1430,390,90);
    drawGate(1125,290);
    drawSign(30,455,0);
    drawSign(1510,455,1);
}

static void drawCity(void)
{
    int i;

    rect(0,0,SW,SH,BLACK);

    for (i=0; i<16; i++) {
        float x=i*105.0f-cameraX;
        float h=100+(i%4)*28;

        rect(x,GROUND-h,75,h,DARK);
        rect(x+5,GROUND-h+10,65,4,GRAY);
        rect(x+14,GROUND-h+25,10,18,LIGHT);
        rect(x+44,GROUND-h+25,10,18,LIGHT);
        tri(x,GROUND-h,x+25,GROUND-h-15,
            x+75,GROUND-h,GRAY);
    }

    tri(250-cameraX,450,285-cameraX,390,315-cameraX,450,GRAY);
    tri(850-cameraX,450,900-cameraX,370,930-cameraX,450,GRAY);
    tri(1240-cameraX,450,1280-cameraX,405,1310-cameraX,450,GRAY);

    drawPlatforms();
    drawSign(30,455,0);
    drawSign(1510,455,1);
}

static void drawCastle(void)
{
    int i;

    rect(0,0,SW,SH,BLACK);

    for (i=0; i<12; i++) {
        float x=i*140.0f+20-cameraX;
        rect(x,100,20,400,DARK);
        rect(x-8,100,36,10,LIGHT);
        rect(x-8,490,36,10,GRAY);
    }

    for (i=0; i<8; i++) {
        float x=i*190.0f+40-cameraX;
        tri(x,180,x+65,100,x+130,180,GRAY);
        rect(x,180,130,8,DARK);
    }

    drawPlatforms();
    rect(1120-cameraX,390,360,110,DARK);
    rect(1120-cameraX,390,360,6,LIGHT);

    for (i=0; i<4; i++) {
        float x=1145+i*90.0f-cameraX;
        rect(x,275,18,115,GRAY);
        rect(x-7,270,32,8,LIGHT);
        tri(x-8,275,x+9,245,x+26,275,DARK);
    }

    drawGate(1125,290);
    drawSign(30,455,0);
    drawSign(1510,455,1);
}

static void drawTower(void)
{
    int i;

    rect(0,0,SW,SH,BLACK);
    rect(0,0,55,SH,DARK);
    rect(1545-cameraX,0,55,SH,DARK);

    for (i=0; i<12; i++) {
        float x=120+i*125.0f-cameraX;
        float y=35+(i%3)*20;

        rect(x,y,28,65,GRAY);
        tri(x,y,x+14,y-20,x+28,y,LIGHT);
    }

    for (i=0; i<8; i++) {
        float x=200+i*170.0f-cameraX;
        rect(x,0,3,160,GRAY);
        rect(x+15,160,3,120,DARK);
    }

    drawPlatforms();
    rect(1120-cameraX,390,360,110,DARK);
    rect(1120-cameraX,390,360,5,WHITE);
    drawGate(1125,290);
    drawSign(30,455,0);
}

static void drawFinalArena(void)
{
    int i;

    rect(0,0,SW,SH,BLACK);
    rect(0,100,SW,3,GRAY);

    for (i=0; i<14; i++) {
        float x=i*120.0f-cameraX;
        tri(x,500,x+60,430,x+120,500,DARK);
    }

    rect(100-cameraX,280,45,220,DARK);
    rect(90-cameraX,270,65,12,GRAY);
    rect(450-cameraX,340,40,160,DARK);
    rect(440-cameraX,330,60,12,GRAY);
    rect(1110-cameraX,320,40,180,DARK);
    rect(1100-cameraX,310,60,12,GRAY);
    rect(1400-cameraX,260,45,240,DARK);
    rect(1390-cameraX,250,65,12,GRAY);

    drawPlatforms();
    rect(600-cameraX,390,400,110,DARK);
    rect(600-cameraX,390,400,6,WHITE);
    drawGate(605,290);
    drawGate(915,290);
    drawSign(30,455,0);
}

static void drawScene(void)
{
    switch (zone) {
    case 0: drawCave(); break;
    case 1: drawForest(); break;
    case 2: drawCity(); break;
    case 3: drawCastle(); break;
    case 4: drawTower(); break;
    case 5: drawFinalArena(); break;
    default: drawCave(); break;
    }
}

/* =====================================================
   XP E MELHORIAS
   ===================================================== */

static void giveXP(int amount)
{
    xp += amount;

    while (level < MAX_LEVEL && xp >= nextXP) {
        xp -= nextXP;
        level++;
        upgradePoints++;

        /* Bonus de 50 XP por nivel */
        xp += 50;
        nextXP = 100 + level * 25;
    }

    if (level >= MAX_LEVEL) {
        level = MAX_LEVEL;
        if (xp >= nextXP)
            xp = nextXP - 1;
    }
}

static void buyUpgrade(void)
{
    if (upgradePoints <= 0)
        return;

    switch (upgradeSelection) {
    case 0:
        if (maxHP < 16) {
            maxHP += 2;
            if (maxHP > 16) maxHP = 16;
            hp = maxHP;
            upgradePoints--;
        }
        break;

    case 1:
        if (damage < 9) {
            damage++;
            upgradePoints--;
        }
        break;

    case 2:
        if (defense < 8) {
            defense++;
            upgradePoints--;
        }
        break;

    case 3:
        if (speed < 6) {
            speed++;
            upgradePoints--;
        }
        break;
    }
}

static void drawHUD(void)
{
    char text[24];
    int width;

    rect(8,8,112,12,DARK);

    width = maxHP > 0 ? 112 * hp / maxHP : 0;
    rect(8,8,width,12,WHITE);

    sprintf(text,"%d/%d",hp,maxHP);
    drawText(text,126,8,WHITE);

    rect(8,23,112,3,DARK);
    width = nextXP > 0 ? 112 * xp / nextXP : 0;
    rect(8,23,width,3,LIGHT);

    sprintf(text,"LV %d",level);
    drawText(text,420,8,WHITE);

    if (bossActive) {
        rect(100,35,280,8,DARK);
        width = bossMaxHP > 0 ?
            280 * bossHP / bossMaxHP : 0;
        rect(100,35,width,8,WHITE);
    }
}

static void drawUpgradeMenu(void)
{
    char text[24];

    rect(50,35,380,205,DARK);
    rect(54,39,372,197,BLACK);
    drawText("MELHORIAS",175,47,WHITE);

    sprintf(text,"PONTOS %d",upgradePoints);
    drawText(text,280,50,WHITE);

    drawText("VIDA +2",100,82,
             upgradeSelection == 0 ? WHITE : LIGHT);
    drawText("DANO +1",100,106,
             upgradeSelection == 1 ? WHITE : LIGHT);
    drawText("DEFESA +1",100,130,
             upgradeSelection == 2 ? WHITE : LIGHT);
    drawText("VELOCIDADE +1",100,154,
             upgradeSelection == 3 ? WHITE : LIGHT);

    drawText("X COMPRAR",90,192,WHITE);
    drawText("O VOLTAR",260,192,LIGHT);
}

/* =====================================================
   PROTAGONISTA DESENHADO EM C
   Sem PNG, JPG, GIF ou sprites externos.
   ===================================================== */

#define HERO_OUTLINE 0xFF101018
#define HERO_HAIR    0xFF15151F
#define HERO_RED     0xFFB51F32
#define HERO_RED2    0xFF731323
#define HERO_SKIN    0xFFE6C5B0
#define HERO_CLOTH   0xFF20202A
#define HERO_CLOTH2  0xFF373744
#define HERO_WHITE   0xFFE5E5EB
#define HERO_EYE     0xFFFF263F

static void heroRect(float x, float y, float w, float h, Color c)
{
    rect(x,y,w,h,c);
}

static void drawHeroHead(float x, float y, int frame)
{
    /* Contorno e rosto */
    heroRect(x+4,y+1,13,12,HERO_OUTLINE);
    heroRect(x+5,y+2,11,10,HERO_SKIN);
    heroRect(x+4,y+2,3,5,HERO_HAIR);
    heroRect(x+7,y,9,3,HERO_HAIR);
    heroRect(x+13,y+2,5,4,HERO_HAIR);

    /* Franja */
    heroRect(x+5,y+1,4,5,HERO_HAIR);
    heroRect(x+8,y+2,4,4,HERO_HAIR);
    heroRect(x+11,y+1,3,5,HERO_HAIR);

    /* Olhos vermelhos */
    if (playerFacing > 0) {
        heroRect(x+13,y+6,3,2,HERO_EYE);
        heroRect(x+15,y+6,1,2,HERO_WHITE);
    } else {
        heroRect(x+6,y+6,3,2,HERO_EYE);
        heroRect(x+6,y+6,1,2,HERO_WHITE);
    }

    /* Sombra do rosto */
    heroRect(x+7,y+10,7,1,0xFFB58F80);

    /* Mechas que se movem durante a corrida */
    if (frame == 1) {
        heroRect(x+1,y+3,5,3,HERO_HAIR);
        heroRect(x+2,y+6,4,2,HERO_HAIR);
    } else if (frame == 2) {
        heroRect(x+2,y+1,4,3,HERO_HAIR);
        heroRect(x+1,y+4,4,3,HERO_HAIR);
    }
}

static void drawHeroBody(float x, float y, int frame)
{
    /* Casaco preto */
    heroRect(x+3,y,14,12,HERO_OUTLINE);
    heroRect(x+5,y+1,10,10,HERO_CLOTH);
    heroRect(x+4,y+2,3,8,HERO_CLOTH2);
    heroRect(x+13,y+2,3,8,HERO_CLOTH2);

    /* Gola vermelha */
    heroRect(x+7,y,6,3,HERO_RED);
    heroRect(x+8,y+3,4,2,HERO_RED2);

    /* Linha vermelha central */
    heroRect(x+9,y+4,2,6,HERO_RED);

    /* Braços animados */
    if (frame == 1) {
        heroRect(x+1,y+2,4,7,HERO_CLOTH);
        heroRect(x+15,y+4,5,5,HERO_CLOTH);
        heroRect(x,y+8,4,3,HERO_SKIN);
        heroRect(x+18,y+7,3,3,HERO_SKIN);
    } else if (frame == 2) {
        heroRect(x+1,y+4,5,5,HERO_CLOTH);
        heroRect(x+15,y+2,4,7,HERO_CLOTH);
        heroRect(x+2,y+8,3,3,HERO_SKIN);
        heroRect(x+17,y+8,3,3,HERO_SKIN);
    } else {
        heroRect(x+1,y+3,4,7,HERO_CLOTH);
        heroRect(x+15,y+3,4,7,HERO_CLOTH);
        heroRect(x+1,y+9,3,2,HERO_SKIN);
        heroRect(x+16,y+9,3,2,HERO_SKIN);
    }
}

static void drawHeroLegs(float x, float y, int frame)
{
    /* Calcas escuras */
    if (frame == 1) {
        heroRect(x+5,y,5,7,HERO_CLOTH);
        heroRect(x+11,y+1,5,6,HERO_CLOTH2);
        heroRect(x+3,y+6,7,3,HERO_WHITE);
        heroRect(x+11,y+6,6,3,HERO_WHITE);
    } else if (frame == 2) {
        heroRect(x+5,y+1,5,6,HERO_CLOTH2);
        heroRect(x+11,y,5,7,HERO_CLOTH);
        heroRect(x+4,y+6,6,3,HERO_WHITE);
        heroRect(x+11,y+6,7,3,HERO_WHITE);
    } else {
        heroRect(x+5,y,5,8,HERO_CLOTH);
        heroRect(x+11,y,5,8,HERO_CLOTH);
        heroRect(x+4,y+7,7,2,HERO_WHITE);
        heroRect(x+10,y+7,7,2,HERO_WHITE);
    }
}

static void drawPlayer(void)
{
    float x = playerX - cameraX;
    float y = playerY;
    int frame;

    if (invulnerability > 0 && (invulnerability / 4) % 2)
        return;

    if (!grounded)
        frame = 2;
    else if ((playerFrame / 7) % 2)
        frame = 1;
    else
        frame = 0;

    /* Sombra no chao */
    rect(x-2,y+PLAYER_H,23,2,DARK);

    /*
     * Personagem de 19x30 pixels:
     * cabelo preto, olhos vermelhos, roupa escura
     * e detalhes vermelhos e brancos.
     */
    drawHeroHead(x,y,frame);
    drawHeroBody(x,y+12,frame);
    drawHeroLegs(x,y+21,frame);

    /* Efeito de dash */
    if (dashFrames > 0) {
        if (playerFacing > 0) {
            rect(x-8,y+8,7,3,HERO_RED);
            rect(x-13,y+13,10,2,HERO_WHITE);
        } else {
            rect(x+PLAYER_W+1,y+8,7,3,HERO_RED);
            rect(x+PLAYER_W+3,y+13,10,2,HERO_WHITE);
        }
    }
}

/* =====================================================
   INIMIGOS
   ===================================================== */

static void spawnEnemies(void)
{
    int i;

    memset(enemies,0,sizeof(enemies));

    for (i=0; i<MAX_ENEMIES; i++) {
        enemies[i].active = 1;
        enemies[i].hp = 4 + zone * 2;
        enemies[i].x = 180 + i * 145.0f;
        enemies[i].y = GROUND - 24;
        enemies[i].vx = (i%2) ? 0.6f : -0.6f;
    }
}

static void drawEnemies(void)
{
    int i;

    for (i=0; i<MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        float x = e->x-cameraX;

        if (!e->active || x < -30 || x > SW+30)
            continue;

        rect(x,e->y,20,24,GRAY);
        rect(x+4,e->y+6,4,4,WHITE);
        rect(x+12,e->y+6,4,4,WHITE);
    }
}

static void updateEnemies(void)
{
    int i;

    for (i=0; i<MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        int hit;

        if (!e->active)
            continue;

        e->x += e->vx;

        if (e->x < 30 || e->x > WORLD_W-30)
            e->vx = -e->vx;

        if (invulnerability > 0)
            continue;

        if (overlap(playerX,playerY,PLAYER_W,PLAYER_H,
                    e->x,e->y,20,24)) {
            hit = 2 - defense/3;
            if (hit < 1) hit = 1;

            hp -= hit;
            if (hp < 0) hp = 0;

            invulnerability = 45;
            playerX += (playerX < e->x) ? -18 : 18;
            playerX = clampf(playerX,0,WORLD_W-PLAYER_W);
        }
    }
}

/* =====================================================
   MAGIA E PROJETEIS
   ===================================================== */

static void drawMagicPickup(void)
{
    float x, y;

    if (magicTaken)
        return;

    itemPulse++;

    x = magicX-cameraX;
    y = magicY + ((itemPulse/8)%2 ? 2 : 0);

    rect(x+5,y+8,14,20,DARK);
    tri(x,y+18,x+12,y,x+24,y+18,LIGHT);
    tri(x+4,y+18,x+12,y+6,x+20,y+18,WHITE);
    rect(x+10,y+18,4,12,GRAY);
}

static void fireMagic(void)
{
    int i;

    if (!magicUnlocked || shotCooldown > 0)
        return;

    for (i=0; i<MAX_SHOTS; i++) {
        if (!shots[i].active) {
            shots[i].active = 1;
            shots[i].x = playerX +
                (playerFacing > 0 ? PLAYER_W : -8);
            shots[i].y = playerY + 12;
            shots[i].vx = playerFacing > 0 ? 6.0f : -6.0f;
            shotCooldown = 18;
            return;
        }
    }
}

static void drawShots(void)
{
    int i;

    for (i=0; i<MAX_SHOTS; i++) {
        if (shots[i].active)
            rect(shots[i].x-cameraX,
                 shots[i].y,12,5,WHITE);
    }
}

static void defeatBoss(void)
{
    int idx = -1;

    if (!bossActive)
        return;

    bossHP = 0;
    bossActive = 0;

    if (zone == 1) idx = 0;
    if (zone == 3) idx = 1;
    if (zone == 4) idx = 2;
    if (zone == 5) idx = 3;

    if (idx >= 0)
        bossDefeated[idx] = 1;

    giveXP(150);
}

static void updateShots(void)
{
    int i, j;

    for (i=0; i<MAX_SHOTS; i++) {
        Shot *s = &shots[i];

        if (!s->active)
            continue;

        s->x += s->vx;

        if (s->x < 0 || s->x > WORLD_W) {
            s->active = 0;
            continue;
        }

        for (j=0; j<MAX_ENEMIES; j++) {
            Enemy *e = &enemies[j];

            if (!s->active || !e->active)
                continue;

            if (overlap(s->x,s->y,12,5,e->x,e->y,20,24)) {
                e->hp -= damage;
                s->active = 0;

                if (e->hp <= 0) {
                    e->active = 0;
                    giveXP(20);
                }
            }
        }

        if (bossActive && s->active &&
            overlap(s->x,s->y,12,5,bossX,bossY,60,60)) {
            bossHP -= damage;
            s->active = 0;

            if (bossHP <= 0)
                defeatBoss();
        }
    }
}

/* =====================================================
   CHEFES
   ===================================================== */

static int bossIndex(void)
{
    if (zone == 1) return 0;
    if (zone == 3) return 1;
    if (zone == 4) return 2;
    if (zone == 5) return 3;
    return -1;
}

static const char *bossName(void)
{
    switch (bossIndex()) {
    case 0: return "EIDOLON";
    case 1: return "VERTICE";
    case 2: return "DESPERTO";
    case 3: return "NEXUS";
    default: return "";
    }
}

static void startBoss(void)
{
    int idx = bossIndex();

    if (idx < 0 || bossDefeated[idx])
        return;

    bossActive = 1;
    bossMaxHP = 100 + zone*50;
    bossHP = bossMaxHP;
    bossX = (zone == 5) ? 780 : 1300;
    bossY = 350;
    bossTimer = 0;
    bossPhase = 0;
}

static void drawBoss(void)
{
    float x;

    if (!bossActive)
        return;

    x = bossX-cameraX;

    if (x < -70 || x > SW+70)
        return;

    rect(x,bossY,60,60,DARK);
    rect(x+6,bossY+4,48,6,GRAY);
    rect(x+10,bossY+12,10,8,WHITE);
    rect(x+40,bossY+12,10,8,WHITE);
    rect(x+18,bossY+34,24,5,LIGHT);

    drawText(bossName(),SW/2-30,49,WHITE);
}

static void updateBoss(void)
{
    int hit;

    if (!bossActive)
        return;

    bossTimer++;

    if (bossTimer >= 75-zone*3) {
        bossTimer = 0;
        bossPhase = !bossPhase;

        if (invulnerability == 0 &&
            overlap(playerX,playerY,PLAYER_W,PLAYER_H,
                    bossX,bossY,60,60)) {
            hit = 3 - defense/3;
            if (hit < 1) hit = 1;

            hp -= hit;
            if (hp < 0) hp = 0;
            invulnerability = 45;
        }
    }

    if (bossPhase)
        bossX += (playerX < bossX) ? -1 : 1;

    bossX = clampf(bossX,100,WORLD_W-100);
}

/* =====================================================
   MOVIMENTACAO E COLISOES
   ===================================================== */

static void updatePlayer(SceCtrlData *pad)
{
    int i;
    unsigned int b = pad->Buttons;
    float oldY = playerY;
    float oldX = playerX;

    if (dashFrames > 0) {
        playerX += playerFacing * 9;
        dashFrames--;
        playerX = clampf(playerX,0,WORLD_W-PLAYER_W);
    } else {
        if (b & PSP_CTRL_LEFT) {
            playerX -= speed;
            playerFacing = -1;
            playerFrame++;
        }

        if (b & PSP_CTRL_RIGHT) {
            playerX += speed;
            playerFacing = 1;
            playerFrame++;
        }
    }

    playerX = clampf(playerX,0,WORLD_W-PLAYER_W);

    if (pressed(b,PSP_CTRL_CROSS)) {
        if (grounded) {
            velocityY = -8.5f;
            grounded = 0;
            doubleJumpUsed = 0;
        } else if (doubleJumpUnlocked && !doubleJumpUsed) {
            velocityY = -8.0f;
            doubleJumpUsed = 1;
        }
    }

    if (pressed(b,PSP_CTRL_CIRCLE) && magicUnlocked)
        fireMagic();

    if (shotCooldown > 0)
        shotCooldown--;

    if (dashCooldown > 0)
        dashCooldown--;

    if (invulnerability > 0)
        invulnerability--;

    velocityY += 0.35f;
    if (velocityY > 7)
        velocityY = 7;

    playerY += velocityY;
    grounded = 0;

    for (i=0; i<platformCount; i++) {
        Platform *p = &platforms[i];

        if (playerX+PLAYER_W > p->x &&
            playerX < p->x+p->w &&
            oldY+PLAYER_H <= p->y &&
            playerY+PLAYER_H >= p->y &&
            velocityY >= 0) {
            playerY = p->y-PLAYER_H;
            velocityY = 0;
            grounded = 1;
            doubleJumpUsed = 0;
        }
    }

    /* Impede atravessar o teto das plataformas ao subir. */
    if (velocityY < 0) {
        for (i=0; i<platformCount; i++) {
            Platform *p = &platforms[i];

            if (playerX+PLAYER_W > p->x &&
                playerX < p->x+p->w &&
                oldY >= p->y+p->h &&
                playerY <= p->y+p->h) {
                playerY = p->y+p->h;
                velocityY = 0;
            }
        }
    }

    if (playerY > WORLD_H) {
        playerX = 40;
        playerY = GROUND-PLAYER_H;
        velocityY = 0;
        if (hp > 0) hp--;
    }

    if (zone == 0 && !magicTaken &&
        overlap(playerX,playerY,PLAYER_W,PLAYER_H,
                magicX,magicY,24,38)) {
        magicTaken = 1;
        magicUnlocked = 1;
        giveXP(25);
    }

    /*
     * Habilidades:
     * Pulo duplo liberado ao chegar na Floresta.
     * Dash liberado ao chegar na Cidade.
     */
    if (zone >= 1)
        doubleJumpUnlocked = 1;

    if (zone >= 2)
        dashUnlocked = 1;

    /* Mantem a camera seguindo o jogador. */
    cameraX = clampf(playerX-220,0,WORLD_W-SW);

    (void)oldX;
}

/* =====================================================
   MUDAR DE ZONA
   ===================================================== */

static void changeZone(int direction)
{
    int oldZone = zone;

    zone += direction;

    if (zone < 0) zone = 0;
    if (zone > 5) zone = 5;

    if (zone == oldZone)
        return;

    buildPlatforms();

    playerX = direction > 0 ? 30 : WORLD_W-60;
    playerY = GROUND-PLAYER_H;
    velocityY = 0;
    grounded = 0;
    doubleJumpUsed = 0;

    cameraX = clampf(playerX-220,0,WORLD_W-SW);

    memset(shots,0,sizeof(shots));
    spawnEnemies();

    bossActive = 0;
    startBoss();
}

/* =====================================================
   GRAFICOS PSP
   ===================================================== */

static void initGraphics(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT,guList);

    sceGuDrawBuffer(GU_PSM_8888,(void *)0,STRIDE);
    sceGuDispBuffer(SW,SH,(void *)0x88000,STRIDE);
    sceGuDepthBuffer((void *)0x110000,STRIDE);

    sceGuOffset(2048-(SW/2),2048-(SH/2));
    sceGuViewport(2048,2048,SW,SH);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,SW,SH);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0,0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* =====================================================
   NOVO JOGO
   ===================================================== */

static void newGame(void)
{
    zone = 0;

    playerX = 40;
    playerY = GROUND-PLAYER_H;
    velocityY = 0;
    cameraX = 0;
    grounded = 0;

    playerFacing = 1;
    playerFrame = 0;
    invulnerability = 0;

    maxHP = 8;
    hp = 8;
    damage = 2;
    defense = 1;
    speed = 3;

    level = 1;
    xp = 0;
    nextXP = 100;
    upgradePoints = 0;

    magicUnlocked = 0;
    magicTaken = 0;
    magicX = 310;
    magicY = 420;
    shotCooldown = 0;
    itemPulse = 0;

    doubleJumpUnlocked = 0;
    doubleJumpUsed = 0;
    dashUnlocked = 0;
    dashCooldown = 0;
    dashFrames = 0;

    bossActive = 0;
    bossHP = 0;
    bossMaxHP = 0;
    bossTimer = 0;
    bossPhase = 0;

    memset(bossDefeated,0,sizeof(bossDefeated));
    memset(shots,0,sizeof(shots));

    buildPlatforms();
    spawnEnemies();

    gameStarted = 1;
    menuMode = 1;
}

/* =====================================================
   LOOP PRINCIPAL
   ===================================================== */

int main(void)
{
    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    memset(platforms,0,sizeof(platforms));
    memset(enemies,0,sizeof(enemies));
    memset(shots,0,sizeof(shots));
    memset(bossDefeated,0,sizeof(bossDefeated));

    zone = 0;
    buildPlatforms();
    spawnEnemies();

    menuMode = 0;
    menuSelection = 0;
    gameStarted = 0;

    initGraphics();

    while (running) {
        unsigned int b;

        sceCtrlReadBufferPositive(&pad,1);
        b = pad.Buttons;

        /* MENU PRINCIPAL */
        if (menuMode == 0) {
            if (pressed(b,PSP_CTRL_UP)) {
                menuSelection--;
                if (menuSelection < 0)
                    menuSelection = 3;
            }

            if (pressed(b,PSP_CTRL_DOWN)) {
                menuSelection++;
                if (menuSelection > 3)
                    menuSelection = 0;
            }

            if (pressed(b,PSP_CTRL_CROSS)) {
                switch (menuSelection) {
                case 0:
                    newGame();
                    break;

                case 1:
                    if (gameStarted)
                        menuMode = 1;
                    break;

                case 2:
                    if (gameStarted)
                        menuMode = 2;
                    break;

                case 3:
                    running = 0;
                    break;
                }
            }
        }

        /* JOGO */
        else if (menuMode == 1) {
            if (pressed(b,PSP_CTRL_START))
                menuMode = 2;

            if (pressed(b,PSP_CTRL_SELECT))
                menuMode = 0;

            updatePlayer(&pad);
            updateEnemies();
            updateShots();
            updateBoss();

            /* L: zona anterior para testes. */
            if (pressed(b,PSP_CTRL_LTRIGGER))
                changeZone(-1);

            /*
             * R: antes do desbloqueio, muda de zona.
             * Depois do desbloqueio, executa o dash.
             */
            if (pressed(b,PSP_CTRL_RTRIGGER)) {
                if (dashUnlocked) {
                    if (dashCooldown == 0) {
                        dashFrames = 8;
                        dashCooldown = 45;
                        invulnerability = 10;
                    }
                } else {
                    changeZone(1);
                }
            }

            if (hp <= 0) {
                hp = maxHP;
                playerX = 40;
                playerY = GROUND-PLAYER_H;
                velocityY = 0;
                cameraX = 0;
                invulnerability = 60;
            }
        }

        /* MELHORIAS */
        else if (menuMode == 2) {
            if (pressed(b,PSP_CTRL_UP))
                upgradeSelection = 0;

            if (pressed(b,PSP_CTRL_RIGHT))
                upgradeSelection = 1;

            if (pressed(b,PSP_CTRL_DOWN))
                upgradeSelection = 2;

            if (pressed(b,PSP_CTRL_LEFT))
                upgradeSelection = 3;

            if (pressed(b,PSP_CTRL_CROSS))
                buyUpgrade();

            if (pressed(b,PSP_CTRL_CIRCLE) ||
                pressed(b,PSP_CTRL_START))
                menuMode = 1;

            if (pressed(b,PSP_CTRL_SELECT))
                menuMode = 0;
        }

        /* DESENHO */
        sceGuStart(GU_DIRECT,guList);
        sceGuClearColor(BLACK);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        if (menuMode == 0) {
            drawMainMenu();
        } else {
            drawScene();

            if (zone == 0)
                drawMagicPickup();

            drawEnemies();
            drawShots();
            drawBoss();
            drawPlayer();
            drawHUD();

            if (menuMode == 2)
                drawUpgradeMenu();
        }

        sceGuFinish();
        sceGuSync(0,0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        previousButtons = b;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
