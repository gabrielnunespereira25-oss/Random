/*
 * RANDOM - JOGO COMPLETO EM UM ARQUIVO
 * Plataforma: PSP / PSPSDK
 * Graficos: desenhados em C, sem imagens externas
 *
 * ZONAS:
 * 0 Caverna do Labirinto
 * 1 Floresta Esquecida
 * 2 Cidade Abandonada
 * 3 Castelo Caido
 * 4 Torre Antiga
 * 5 Arena Final
 *
 * CHEFES:
 * EIDOLON, o Fragmento
 * VERTICE, o Devorador
 * ???, o Desperto
 * NEXUS, o Ultimo Silencio
 *
 * CONTROLES:
 * Direcional: movimentar
 * X: pular
 * O: magia
 * START: melhorias
 * L: zona anterior (teste)
 * R: proxima zona (teste)
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

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
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
#define PLAYER_W 16
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
static int menuMode;
static int upgradeSelection;

static float playerX = 40;
static float playerY = GROUND - PLAYER_H;
static float velocityY;
static float cameraX;
static int grounded;
static int doubleJumpUnlocked;
static int doubleJumpUsed;
static int dashUnlocked;
static int dashCooldown;

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

static int bossActive;
static int bossHP;
static int bossMaxHP;
static int bossX;
static int bossY;
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

/* Fonte compacta: numeros e letras necessarias para HUD */
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

        for (col = 0; col < 5; col++)
            for (row = 0; row < 7; row++)
                if (font[i][col] & (1 << row))
                    rect(x + col, y + row, 1, 1, c);

        x += 6;
    }
}

/* =====================================================
   PLATAFORMAS DAS SEIS ZONAS
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
        rect(platforms[i].x-cameraX, platforms[i].y,
             platforms[i].w, platforms[i].h, GRAY);
        rect(platforms[i].x-cameraX, platforms[i].y,
             platforms[i].w, 3, WHITE);
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

    for (i=0;i<5;i++)
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

    for (i=0;i<16;i++) {
        float x=i*105.0f-cameraX;

        tri(x,0,x+52,65,x+105,0,DARK);
        tri(x+10,0,x+52,42,x+82,0,GRAY);
    }

    for (i=0;i<18;i++) {
        float x=i*91.0f+20-cameraX;
        tri(x,0,x+12,25+(i%4)*13,x+24,0,GRAY);
    }

    for (i=0;i<20;i++)
        rect(i*82.0f-cameraX,130+(i%3)*55,34,5,DARK);

    drawCrystal(565,455);
    drawCrystal(595,455);
    drawCrystal(1030,455);

    drawPlatforms();
    drawSign(30,455,0);
    drawSign(1510,455,1);

    rect(1210-cameraX,455,250,7,LIGHT);
    rect(1210-cameraX,462,250,38,DARK);
}

static void drawForest(void)
{
    int i;

    rect(0,0,SW,SH,BLACK);
    rect(1090-cameraX,45,42,42,LIGHT);
    rect(1100-cameraX,38,35,35,BLACK);

    for (i=0;i<18;i++)
        drawTree(i*96.0f,450,70+(i%3)*12);

    drawTree(30,GROUND,125);
    drawTree(390,GROUND,110);
    drawTree(760,GROUND,135);
    drawTree(1080,GROUND,115);
    drawTree(1500,GROUND,130);

    for (i=0;i<20;i++) {
        float x=i*80.0f-cameraX;
        tri(x,GROUND,x+20,GROUND-25,x+40,GROUND,GRAY);
    }

    drawPlatforms();

    /* Arena EIDOLON */
    rect(1120-cameraX,390,360,110,DARK);
    rect(1120-cameraX,390,360,5,LIGHT);
    drawTree(1135,390,85);
    drawTree(1430,390,90);
    rect(1260-cameraX,368,80,22,GRAY);
    rect(1270-cameraX,358,60,10,LIGHT);

    tri(1280-cameraX,320,1300-cameraX,290,
        1320-cameraX,320,WHITE);

    drawGate(1125,290);
    drawSign(30,455,0);
    drawSign(1510,455,1);
}

static void drawCity(void)
{
    int i;

    rect(0,0,SW,SH,BLACK);

    for (i=0;i<16;i++) {
        float x=i*105.0f-cameraX;
        float h=100+(i%4)*28;

        rect(x,GROUND-h,75,h,DARK);
        rect(x+5,GROUND-h+10,65,4,GRAY);
        rect(x+14,GROUND-h+25,10,18,LIGHT);
        rect(x+44,GROUND-h+25,10,18,LIGHT);

        tri(x,GROUND-h,x+25,GROUND-h-15,
            x+75,GROUND-h,GRAY);
    }

    tri(250-cameraX,450,285-cameraX,390,
        315-cameraX,450,GRAY);

    tri(850-cameraX,450,900-cameraX,370,
        930-cameraX,450,GRAY);

    tri(1240-cameraX,450,1280-cameraX,405,
        1310-cameraX,450,GRAY);

    drawPlatforms();
    drawSign(30,455,0);
    drawSign(1510,455,1);
}

static void drawCastle(void)
{
    int i;

    rect(0,0,SW,SH,BLACK);

    for (i=0;i<12;i++) {
        float x=i*140.0f+20-cameraX;

        rect(x,100,20,400,DARK);
        rect(x-8,100,36,10,LIGHT);
        rect(x-8,490,36,10,GRAY);
    }

    for (i=0;i<8;i++) {
        float x=i*190.0f+40-cameraX;

        tri(x,180,x+65,100,x+130,180,GRAY);
        rect(x,180,130,8,DARK);
    }

    drawPlatforms();

    /* Arena VERTICE */
    rect(1120-cameraX,390,360,110,DARK);
    rect(1120-cameraX,390,360,6,LIGHT);

    for (i=0;i<4;i++) {
        float x=1145+i*90.0f-cameraX;

        rect(x,275,18,115,GRAY);
        rect(x-7,270,32,8,LIGHT);
        tri(x-8,275,x+9,245,x+26,275,DARK);
    }

    rect(1245-cameraX,378,110,12,LIGHT);
    rect(1260-cameraX,365,80,13,GRAY);

    tri(1275-cameraX,335,1300-cameraX,295,
        1325-cameraX,335,WHITE);

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

    for (i=0;i<12;i++) {
        float x=120+i*125.0f-cameraX;
        float y=35+(i%3)*20;

        rect(x,y,28,65,GRAY);
        tri(x,y,x+14,y-20,x+28,y,LIGHT);
    }

    for (i=0;i<8;i++) {
        float x=200+i*170.0f-cameraX;
        rect(x,0,3,160,GRAY);
        rect(x+15,160,3,120,DARK);
    }

    drawPlatforms();

    /* Arena do Desperto */
    rect(1120-cameraX,390,360,110,DARK);
    rect(1120-cameraX,390,360,5,WHITE);

    for (i=0;i<5;i++) {
        float x=1145+i*68.0f-cameraX;
        float top=110+(i%2)*25;

        rect(x,top,12,390-top,GRAY);
        rect(x-6,top,24,5,LIGHT);
    }

    rect(1160-cameraX,325,65,8,GRAY);
    rect(1375-cameraX,325,65,8,GRAY);

    tri(1265-cameraX,330,1300-cameraX,280,
        1335-cameraX,330,WHITE);

    drawGate(1125,290);
    drawSign(30,455,0);
}

static void drawFinalArena(void)
{
    int i;

    rect(0,0,SW,SH,BLACK);
    rect(0,100,SW,3,GRAY);

    for (i=0;i<14;i++) {
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

    /* Arena NEXUS */
    rect(600-cameraX,390,400,110,DARK);
    rect(600-cameraX,390,400,6,WHITE);

    for (i=0;i<6;i++) {
        float x=615+i*72.0f-cameraX;
        rect(x,320,16,70,GRAY);
        rect(x-5,312,26,8,LIGHT);
    }

    rect(730-cameraX,378,140,12,GRAY);
    rect(745-cameraX,365,110,13,LIGHT);

    tri(750-cameraX,325,800-cameraX,270,
        850-cameraX,325,WHITE);

    rect(792-cameraX,325,16,40,BLACK);

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
   EXPERIENCIA E MELHORIAS
   ===================================================== */

static void giveXP(int amount)
{
    xp += amount;

    while (level < MAX_LEVEL && xp >= nextXP) {
        xp -= nextXP;
        level++;
        upgradePoints++;

        /* Bonus de 50 XP ao subir de nivel */
        xp += 50;
        nextXP = 100 + level * 25;

        if (xp >= nextXP)
            xp = nextXP - 1;
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
    char text[20];
    int width;

    rect(8,8,112,12,DARK);

    width = maxHP ? 112 * hp / maxHP : 0;
    rect(8,8,width,12,WHITE);

    sprintf(text,"%d/%d",hp,maxHP);
    drawText(text,126,8,WHITE);

    rect(8,23,112,3,DARK);
    width = nextXP ? 112 * xp / nextXP : 0;
    rect(8,23,width,3,LIGHT);

    sprintf(text,"LV %d",level);
    drawText(text,420,8,WHITE);
}

static void drawUpgradeMenu(void)
{
    char text[20];

    rect(50,35,380,205,DARK);
    drawText("UPGRADES",175,45,WHITE);
    drawText("UP VIDA +2",80,80,WHITE);
    drawText("RIGHT DANO +1",80,105,WHITE);
    drawText("DOWN DEFESA +1",80,130,WHITE);
    drawText("LEFT VELOCIDADE +1",80,155,WHITE);
    drawText("X COMPRAR",80,190,WHITE);
    drawText("O VOLTAR",250,190,WHITE);

    sprintf(text,"PONTOS %d",upgradePoints);
    drawText(text,280,50,WHITE);
}

/* =====================================================
   JOGADOR
   ===================================================== */

static void drawPlayer(void)
{
    float x=playerX-cameraX;
    float y=playerY;

    rect(x+4,y,10,10,WHITE);
    rect(x+3,y+10,12,11,LIGHT);
    rect(x,y+11,4,10,GRAY);
    rect(x+15,y+11,4,10,GRAY);
    rect(x+3,y+21,5,9,WHITE);
    rect(x+10,y+21,5,9,WHITE);
    rect(x+6,y+4,2,2,BLACK);
    rect(x+11,y+4,2,2,BLACK);
}

/* =====================================================
   INIMIGOS
   ===================================================== */

static void spawnEnemies(void)
{
    int i;

    memset(enemies,0,sizeof(enemies));

    for (i=0;i<MAX_ENEMIES;i++) {
        enemies[i].active=1;
        enemies[i].hp=4+zone*2;
        enemies[i].x=180+i*145.0f;
        enemies[i].y=GROUND-24;
        enemies[i].vx=(i%2)?0.6f:-0.6f;
    }
}

static void drawEnemies(void)
{
    int i;

    for (i=0;i<MAX_ENEMIES;i++) {
        Enemy *e=&enemies[i];

        if (!e->active) continue;

        rect(e->x-cameraX,e->y,20,24,GRAY);
        rect(e->x-cameraX+4,e->y+6,4,4,WHITE);
        rect(e->x-cameraX+12,e->y+6,4,4,WHITE);
    }
}

static void updateEnemies(void)
{
    int i;

    for (i=0;i<MAX_ENEMIES;i++) {
        Enemy *e=&enemies[i];
        int hit;

        if (!e->active) continue;

        e->x += e->vx;

        if (e->x<30 || e->x>WORLD_W-30)
            e->vx=-e->vx;

        if (playerX+PLAYER_W>e->x &&
            playerX<e->x+20 &&
            playerY+PLAYER_H>e->y &&
            playerY<e->y+24) {

            hit=2-defense/3;
            if (hit<1) hit=1;

            hp-=hit;
            if (hp<0) hp=0;

            playerX += (playerX<e->x)?-18:18;
            playerX=clampf(playerX,0,WORLD_W-PLAYER_W);
        }
    }
}

/* =====================================================
   MAGIA E PROJETEIS
   ===================================================== */

static void drawMagicPickup(void)
{
    if (magicTaken) return;

    tri(magicX-cameraX,magicY+18,
        magicX+12-cameraX,magicY,
        magicX+24-cameraX,magicY+18,LIGHT);

    tri(magicX+4-cameraX,magicY+18,
        magicX+12-cameraX,magicY+6,
        magicX+20-cameraX,magicY+18,WHITE);
}

static void fireMagic(void)
{
    int i;

    if (!magicUnlocked || shotCooldown>0)
        return;

    for (i=0;i<MAX_SHOTS;i++) {
        if (!shots[i].active) {
            shots[i].active=1;
            shots[i].x=playerX+PLAYER_W;
            shots[i].y=playerY+12;
            shots[i].vx=6;
            shotCooldown=18;
            return;
        }
    }
}

static void drawShots(void)
{
    int i;

    for (i=0;i<MAX_SHOTS;i++)
        if (shots[i].active)
            rect(shots[i].x-cameraX,shots[i].y,12,5,WHITE);
}

static void updateShots(void)
{
    int i,j;

    for (i=0;i<MAX_SHOTS;i++) {
        Shot *s=&shots[i];

        if (!s->active) continue;

        s->x+=s->vx;

        if (s->x>WORLD_W)
            s->active=0;

        for (j=0;j<MAX_ENEMIES;j++) {
            Enemy *e=&enemies[j];

            if (!s->active || !e->active)
                continue;

            if (s->x>=e->x && s->x<=e->x+20 &&
                s->y>=e->y && s->y<=e->y+24) {

                e->hp-=damage;
                s->active=0;

                if (e->hp<=0) {
                    e->active=0;
                    giveXP(20);
                }
            }
        }

        if (bossActive && s->active &&
            s->x>=bossX && s->x<=bossX+60 &&
            s->y>=bossY && s->y<=bossY+60) {

            bossHP-=damage;
            s->active=0;

            if (bossHP<=0) {
                int idx=-1;

                bossHP=0;
                bossActive=0;

                if (zone==1) idx=0;
                if (zone==3) idx=1;
                if (zone==4) idx=2;
                if (zone==5) idx=3;

                if (idx>=0)
                    bossDefeated[idx]=1;

                giveXP(150);
            }
        }
    }
}

/* =====================================================
   CHEFES
   ===================================================== */

static int bossIndex(void)
{
    if (zone==1) return 0;
    if (zone==3) return 1;
    if (zone==4) return 2;
    if (zone==5) return 3;
    return -1;
}

static void startBoss(void)
{
    int idx=bossIndex();

    if (idx<0 || bossDefeated[idx])
        return;

    bossActive=1;
    bossMaxHP=100+zone*50;
    bossHP=bossMaxHP;
    bossX=(zone==5)?780:1300;
    bossY=350;
    bossTimer=0;
    bossPhase=0;
}

static void drawBoss(void)
{
    if (!bossActive) return;

    rect(bossX-cameraX,bossY,60,60,DARK);
    rect(bossX-cameraX+10,bossY+12,10,8,WHITE);
    rect(bossX-cameraX+40,bossY+12,10,8,WHITE);

    rect(100,24,280,10,DARK);

    if (bossMaxHP>0)
        rect(100,24,280.0f*bossHP/bossMaxHP,10,WHITE);
}

static void updateBoss(void)
{
    int hit;

    if (!bossActive) return;

    bossTimer++;

    /* Cada chefe alterna o ritmo dos ataques */
    if (bossTimer >= 75-zone*3) {
        bossTimer=0;
        bossPhase=!bossPhase;

        if (playerX+PLAYER_W>bossX &&
            playerX<bossX+60 &&
            playerY+PLAYER_H>bossY &&
            playerY<bossY+60) {

            hit=3-defense/3;
            if (hit<1) hit=1;

            hp-=hit;
            if (hp<0) hp=0;
        }
    }

    /* Movimento simples dos chefes */
    if (bossPhase)
        bossX += (playerX<bossX)?-1:1;

    bossX=(int)clampf(bossX,100,WORLD_W-100);
}

/* =====================================================
   MOVIMENTACAO E COLISOES
   ===================================================== */

static void updatePlayer(SceCtrlData *pad)
{
    int i;
    unsigned int b=pad->Buttons;
    float oldY=playerY;

    if (b&PSP_CTRL_LEFT) playerX-=speed;
    if (b&PSP_CTRL_RIGHT) playerX+=speed;

    playerX=clampf(playerX,0,WORLD_W-PLAYER_W);

    if (pressed(b,PSP_CTRL_CROSS)) {
        if (grounded) {
            velocityY=-8.5f;
            grounded=0;
        } else if (doubleJumpUnlocked && !doubleJumpUsed) {
            velocityY=-8.0f;
            doubleJumpUsed=1;
        }
    }

    if (dashUnlocked && dashCooldown==0 &&
        pressed(b,PSP_CTRL_RTRIGGER)) {

        playerX+=(b&PSP_CTRL_LEFT)?-70:70;
        playerX=clampf(playerX,0,WORLD_W-PLAYER_W);
        dashCooldown=45;
    }

    if (pressed(b,PSP_CTRL_CIRCLE) && magicUnlocked)
        fireMagic();

    if (shotCooldown>0) shotCooldown--;
    if (dashCooldown>0) dashCooldown--;

    velocityY+=0.35f;
    if (velocityY>7) velocityY=7;

    playerY+=velocityY;
    grounded=0;

    for (i=0;i<platformCount;i++) {
        Platform *p=&platforms[i];

        if (playerX+PLAYER_W>p->x &&
            playerX<p->x+p->w &&
            oldY+PLAYER_H<=p->y &&
            playerY+PLAYER_H>=p->y &&
            velocityY>=0) {

            playerY=p->y-PLAYER_H;
            velocityY=0;
            grounded=1;
            doubleJumpUsed=0;
        }
    }

    if (playerY>WORLD_H) {
        playerX=40;
        playerY=GROUND-PLAYER_H;
        velocityY=0;
        if (hp>0) hp--;
    }

    if (zone==0 && !magicTaken &&
        playerX+PLAYER_W>magicX &&
        playerX<magicX+24 &&
        playerY+PLAYER_H>magicY &&
        playerY<magicY+38) {

        magicTaken=1;
        magicUnlocked=1;
        giveXP(25);
    }

    if (zone>=2) doubleJumpUnlocked=1;
    if (zone>=3) dashUnlocked=1;

    cameraX=clampf(playerX-220,0,WORLD_W-SW);
}

/* =====================================================
   MUDAR DE ZONA
   ===================================================== */

static void changeZone(int direction)
{
    zone+=direction;

    if (zone<0) zone=0;
    if (zone>=6) zone=5;

    buildPlatforms();

    playerX=(direction>0)?30:WORLD_W-60;
    playerY=GROUND-PLAYER_H;
    velocityY=0;
    cameraX=clampf(playerX-220,0,WORLD_W-SW);

    memset(shots,0,sizeof(shots));
    spawnEnemies();

    bossActive=0;
    startBoss();
}

/* =====================================================
   GRAFICOS
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

    zone=0;
    buildPlatforms();
    spawnEnemies();
    initGraphics();

    playerY=GROUND-PLAYER_H;

    while (running) {
        unsigned int b;

        sceCtrlReadBufferPositive(&pad,1);
        b=pad.Buttons;

        if (pressed(b,PSP_CTRL_START))
            menuMode=(menuMode==1)?0:1;

        if (menuMode==1) {
            if (pressed(b,PSP_CTRL_UP))
                upgradeSelection=0;

            if (pressed(b,PSP_CTRL_RIGHT))
                upgradeSelection=1;

            if (pressed(b,PSP_CTRL_DOWN))
                upgradeSelection=2;

            if (pressed(b,PSP_CTRL_LEFT))
                upgradeSelection=3;

            if (pressed(b,PSP_CTRL_CROSS))
                buyUpgrade();

            if (pressed(b,PSP_CTRL_CIRCLE))
                menuMode=0;
        } else {
            updatePlayer(&pad);
            updateEnemies();
            updateShots();
            updateBoss();

            /* L e R: atalhos de teste das zonas */
            if (pressed(b,PSP_CTRL_LTRIGGER))
                changeZone(-1);

            if (pressed(b,PSP_CTRL_RTRIGGER) &&
                !dashUnlocked)
                changeZone(1);

            if (hp<=0) {
                hp=maxHP;
                playerX=40;
                playerY=GROUND-PLAYER_H;
                velocityY=0;
            }
        }

        sceGuStart(GU_DIRECT,guList);
        sceGuClearColor(BLACK);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        drawScene();

        if (zone==0)
            drawMagicPickup();

        drawEnemies();
        drawShots();
        drawBoss();
        drawPlayer();
        drawHUD();

        if (menuMode==1)
            drawUpgradeMenu();

        sceGuFinish();
        sceGuSync(0,0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        previousButtons=b;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
