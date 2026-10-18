
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512
#define WORLD_W 2400.0f
#define PW 18.0f
#define PH 30.0f
#define ENEMY_COUNT 12
#define SHOT_COUNT 6
#define BOSS_START 1920.0f
#define BOSS_END 2400.0f
#define PLAYER_MAX_HP 4

#define WHITE      0xFFFFFFFF
#define BLACK      0xFF000000
#define GRAY       0xFFAAAAAA
#define DARK_GRAY  0xFF555555
#define BLUE       0xFFFF8040
#define LIGHT_BLUE 0xFFFFD080

typedef struct {
    float x, y, w, h;
} Rect;

typedef struct {
    float x, y, vx, vy;
    int active, target;
} Shot;

typedef struct {
    float x, y, baseY, w, h, phase;
    int active, hp;
} Enemy;

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, vx, vy;
    int hp, active, timer, attack;
    int contactCooldown, damageCooldown, grounded;
} MiniBoss;

static unsigned int __attribute__((aligned(16))) displayList[262144];

static int state = 0;
static int magicUnlocked = 0;
static int grounded = 0;
static int jumps = 0;
static int magicCooldown = 0;
static int swordCooldown = 0;
static int swordTimer = 0;
static int swordFacing = 1;
static int room = 0;
static int frame = 0;
static int introPage = 0;
static int playerMoving = 0;
static int bossDefeated = 0;
static int bossEncounterStarted = 0;
static int swordHitDone = 0;

static int playerHP = PLAYER_MAX_HP;
static int playerDamageCooldown = 0;

static float px = 35.0f;
static float py = 190.0f;
static float pvy = 0.0f;
static float cameraX = 0.0f;

static unsigned int previousButtons = 0;
static Shot shots[SHOT_COUNT];

static MiniBoss boss = {
    2180.0f, 194.0f, 0, 0,
    10, 1, 0, 0, 0, 0, 1
};

static Enemy enemies[ENEMY_COUNT] = {
    {160,170,170,18,18,0.0f,1,1},
    {300,130,130,18,18,1.2f,1,1},
    {430,175,175,18,18,2.1f,1,1},
    {610,150,150,18,18,0.7f,1,1},
    {760,110,110,18,18,1.8f,1,1},
    {900,170,170,18,18,2.8f,1,1},
    {1080,140,140,18,18,0.3f,1,1},
    {1230,100,100,18,18,1.4f,1,1},
    {1400,160,160,18,18,2.5f,1,1},
    {1630,135,135,18,18,0.9f,1,1},
    {1870,100,100,18,18,1.7f,1,1},
    {2200,145,145,18,18,2.7f,0,1}
};

/* =========================================================
   GRAFICOS
   ========================================================= */

static void initGraphics(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(GU_PSM_8888, (void*)0, STRIDE);
    sceGuDispBuffer(SW, SH, (void*)0x88000, STRIDE);
    sceGuDepthBuffer((void*)0x110000, STRIDE);

    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);
    sceGuScissor(0, 0, SW, SH);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);

    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void beginFrame(void) {
    sceGuStart(GU_DIRECT, displayList);
    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

static void endFrame(void) {
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void rect(float x, float y, float w, float h,
                 unsigned int color) {
    Vertex *v = (Vertex*)sceGuGetMemory(2 * sizeof(Vertex));

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDrawArray(GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, 0, v);
}

static void wr(float x, float y, float w, float h,
               unsigned int color) {
    rect(x - cameraX, y, w, h, color);
}

static int overlap(float ax, float ay, float aw, float ah,
                   float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

/* =========================================================
   TEXTO PIXEL
   ========================================================= */

static void letter(int x, int y, char c, unsigned int color) {
    static const unsigned char g[26][5] = {
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

    if (c < 'A' || c > 'Z') return;

    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if (g[c-'A'][col] & (1 << row))
                rect(x + col*2, y + row*2, 2, 2, color);
}

static void text(int x, int y, const char *s,
                 unsigned int color) {
    while (*s) {
        if (*s == ' ') x += 12;
        else {
            letter(x, y, *s, color);
            x += 12;
        }
        s++;
    }
}

/* =========================================================
   CENARIO
   ========================================================= */

static void platform(float x, float y, float w) {
    wr(x, y, w, 4, WHITE);
    wr(x + 3, y + 4, w - 6, 2, BLACK);

    for (float i = 9; i < w - 5; i += 21)
        wr(x + i, y + 1, 2, 3, BLACK);
}

static void stalactite(float x, float width, float length) {
    wr(x, 0, width, 5, WHITE);
    wr(x + width*.25f, 5, width*.5f, length*.55f, WHITE);
    wr(x + width*.4f, length*.55f,
       width*.2f, length*.45f, WHITE);
}

static void crack(float x, float y) {
    wr(x, y, 14, 2, WHITE);
    wr(x + 5, y + 2, 2, 7, WHITE);
    wr(x + 5, y + 9, 9, 2, WHITE);
    wr(x + 12, y + 11, 2, 5, WHITE);
}

static void crystalDecoration(float x, float y) {
    wr(x + 5, y, 5, 5, WHITE);
    wr(x + 2, y + 5, 11, 6, WHITE);
    wr(x, y + 11, 15, 5, WHITE);
    wr(x + 4, y + 16, 7, 4, WHITE);
    wr(x + 6, y + 5, 2, 7, BLACK);
}

static void caveRoom(int r) {
    float s = r * 800.0f;

    stalactite(s+15,54,30);
    stalactite(s+105,35,19);
    stalactite(s+210,60,43);
    stalactite(s+335,42,24);
    stalactite(s+460,65,36);
    stalactite(s+610,45,27);
    stalactite(s+730,52,33);

    crack(s+90,76);
    crack(s+280,104);
    crack(s+510,70);
    crack(s+675,95);

    crystalDecoration(s+170,204);
    crystalDecoration(s+560,190);

    platform(s,238,120);
    platform(s+145,238,105);
    platform(s+275,238,165);
    platform(s+470,238,115);
    platform(s+620,238,180);

    platform(s+65,197,47);
    platform(s+150,166,43);
    platform(s+245,192,40);
    platform(s+330,151,52);
    platform(s+430,185,42);
    platform(s+535,160,46);
    platform(s+650,129,55);
    platform(s+740,177,40);

    wr(s+126,232,5,6,WHITE);
    wr(s+132,226,5,12,WHITE);
    wr(s+138,232,5,6,WHITE);
    wr(s+442,232,5,6,WHITE);
    wr(s+448,226,5,12,WHITE);
    wr(s+454,232,5,6,WHITE);

    wr(s+390,48,50,4,WHITE);
    wr(s+390,52,4,24,WHITE);
    wr(s+436,52,4,24,WHITE);
}

static void drawTransitionCorridor(void) {
    stalactite(1630,24,24);
    stalactite(1740,30,30);
    stalactite(1850,20,20);

    crack(1660,95);
    crack(1810,80);

    platform(1600,238,320);
    platform(1665,197,47);
    platform(1750,166,43);
    platform(1845,192,40);

    wr(1900,105,8,133,DARK_GRAY);
    wr(1908,105,5,133,GRAY);
    wr(1895,101,23,5,WHITE);
    wr(1910,118,3,25,WHITE);
    wr(1910,160,3,25,WHITE);
}

static void drawBossRoom(void) {
    wr(BOSS_START,15,480,5,GRAY);
    wr(BOSS_START,20,480,3,DARK_GRAY);

    for (int i = 0; i < 4; i++) {
        float x = 1960 + i*125;
        wr(x,38,24,100,DARK_GRAY);
        wr(x+5,38,14,100,GRAY);
        wr(x-4,38,32,5,WHITE);
        wr(x-2,138,28,5,GRAY);
        wr(x+8,55,3,19,BLACK);
        wr(x+8,78,3,15,WHITE);
    }

    for (int i = 0; i < 5; i++) {
        float x = 1980 + i*88;
        wr(x,155,18,2,GRAY);
        wr(x+8,157,2,12,WHITE);
        wr(x+8,169,10,2,GRAY);
    }

    wr(2110,205,100,8,DARK_GRAY);
    wr(2120,193,80,12,GRAY);
    wr(2130,183,60,10,WHITE);
    wr(2140,173,40,10,GRAY);
    wr(2145,177,5,5,WHITE);
    wr(2155,177,5,5,BLACK);
    wr(2165,177,5,5,WHITE);

    platform(BOSS_START,238,480);
    platform(2030,185,42);
    platform(2135,160,46);
    platform(2250,129,55);

    wr(1920,105,4,133,GRAY);
    wr(1925,105,3,133,DARK_GRAY);

    if (bossDefeated) {
        wr(2360,185,5,53,WHITE);
        wr(2368,175,5,63,GRAY);
        wr(2376,185,5,53,WHITE);
    } else {
        wr(2360,185,5,53,DARK_GRAY);
        wr(2368,175,5,63,GRAY);
        wr(2376,185,5,53,DARK_GRAY);
    }
}

static void drawCave(void) {
    caveRoom(0);
    caveRoom(1);
    drawTransitionCorridor();
    drawBossRoom();

    wr(790,200,20,38,BLACK);
    wr(1590,200,20,38,BLACK);
    wr(0,0,5,238,WHITE);
    wr(WORLD_W-5,0,5,238,WHITE);
}

/* =========================================================
   JOGADOR
   ========================================================= */

static void drawPlayer(void) {
    float x = px-cameraX;
    float y = py;
    int step = playerMoving ? ((frame/6)%4) : 0;

    rect(x+3,y,12,3,GRAY);
    rect(x+2,y+2,14,3,WHITE);
    rect(x+3,y+4,12,6,WHITE);
    rect(x+4,y+6,10,3,BLACK);
    rect(x+(swordFacing>0?11:5),y+6,2,2,WHITE);
    rect(x+7,y+9,5,2,GRAY);
    rect(x+3,y+11,12,9,WHITE);
    rect(x+5,y+12,8,7,BLACK);
    rect(x+7,y+13,4,4,GRAY);

    if (!grounded) {
        rect(x,y+10,4,7,WHITE);
        rect(x+14,y+10,4,7,WHITE);
    } else {
        rect(x+(step==1?1:0),y+(step==2?14:12),4,7,WHITE);
        rect(x+14,y+(step==1?14:(step==2?10:12)),4,7,WHITE);
    }

    rect(x+3,y+19,12,2,GRAY);

    if (!grounded) {
        rect(x+3,y+21,6,5,WHITE);
        rect(x+10,y+22,6,5,WHITE);
        rect(x+1,y+25,6,3,GRAY);
        rect(x+12,y+26,5,3,GRAY);
    } else {
        rect(x+3,y+21,5,7,WHITE);
        rect(x+10,y+21,5,7,WHITE);
        rect(x+2,y+28,6,2,GRAY);
        rect(x+10,y+28,6,2,GRAY);
    }
}

/* =========================================================
   VIDA DO JOGADOR
   ========================================================= */

static void drawPlayerHealth(void) {
    text(8,25,"HP",WHITE);

    for (int i=0; i<PLAYER_MAX_HP; i++) {
        int x = 34 + i*18;

        rect(x,25,14,9,DARK_GRAY);

        if (i < playerHP) {
            rect(x+2,27,10,5,WHITE);
        }
    }
}

static void playerTakeDamage(void) {
    if (playerDamageCooldown > 0 || playerHP <= 0)
        return;

    playerHP--;
    playerDamageCooldown = 60;

    pvy = -3.0f;
    grounded = 0;
    jumps = 0;

    if (playerHP <= 0) {
        playerHP = 0;
        state = 0;
    }
}

/* =========================================================
   INIMIGOS
   ========================================================= */

static void drawEnemies(void) {
    for (int i=0; i<ENEMY_COUNT; i++) {
        Enemy *e = &enemies[i];
        if (!e->active) continue;

        float x = e->x-cameraX;
        float y = e->y;

        rect(x+7,y,5,4,WHITE);
        rect(x+3,y+4,13,5,WHITE);
        rect(x,y+9,18,5,WHITE);
        rect(x+3,y+14,12,4,WHITE);
        rect(x+6,y+7,3,3,BLACK);
        rect(x+11,y+7,3,3,BLACK);
        rect(x+1,y+9,2,4,BLACK);
        rect(x+15,y+9,2,4,BLACK);
    }
}

static void updateEnemies(void) {
    for (int i=0; i<ENEMY_COUNT; i++) {
        Enemy *e = &enemies[i];
        if (!e->active) continue;

        e->phase += .045f;
        e->y = e->baseY + sinf(e->phase)*10.0f;

        float dx = px-e->x;

        if (fabsf(dx) < 150) {
            if (dx > 4) e->x += .25f;
            else if (dx < -4) e->x -= .25f;
        }

        if (e->x+e->w > BOSS_START-10)
            e->x = BOSS_START-e->w-10;

        if (overlap(px,py,PW,PH,e->x,e->y,e->w,e->h)) {
            playerTakeDamage();

            px = 35;
            py = 190;
            pvy = 0;
            grounded = 0;
            jumps = 0;
            playerMoving = 0;
        }
    }
}

/* =========================================================
   CRISTAL AZUL E MAGIA
   ========================================================= */

static void drawMagicPickup(void) {
    if (magicUnlocked) return;

    float x = 150-cameraX;
    float y = 151;
    int pulse = (frame/8)%2;

    rect(x+4,y-3,9,4,DARK_GRAY);
    rect(x+6,y,5,5,LIGHT_BLUE);
    rect(x+3,y+5,11,7,BLUE);
    rect(x,y+10,17,5,BLUE);
    rect(x+4,y+15,9,5,DARK_GRAY);
    rect(x+7,y+3,3,10,LIGHT_BLUE);

    if (pulse) {
        rect(x-4,y+7,3,3,LIGHT_BLUE);
        rect(x+18,y+4,3,3,LIGHT_BLUE);
        rect(x+8,y-7,3,3,LIGHT_BLUE);
    }
}

static int nearestEnemy(float x,float y) {
    int result = -1;
    float best = 9999999.0f;

    for (int i=0; i<ENEMY_COUNT; i++) {
        if (!enemies[i].active) continue;

        float dx = enemies[i].x+enemies[i].w*.5f-x;
        float dy = enemies[i].y+enemies[i].h*.5f-y;
        float d = dx*dx+dy*dy;

        if (d < best) {
            best = d;
            result = i;
        }
    }

    if (boss.active) {
        float dx = boss.x+18-x;
        float dy = boss.y+22-y;
        float d = dx*dx+dy*dy;

        if (d < best)
            result = ENEMY_COUNT;
    }

    return result;
}

static void fireMagic(void) {
    int target = nearestEnemy(px+PW*.5f,py+PH*.5f);
    if (target < 0) return;

    for (int i=0; i<SHOT_COUNT; i++) {
        if (shots[i].active) continue;

        shots[i].x = px+PW*.5f;
        shots[i].y = py+PH*.5f;

        shots[i].vx = (target==ENEMY_COUNT)
            ? ((boss.x>px)?3.5f:-3.5f)
            : 3.5f*swordFacing;

        shots[i].vy = 0;
        shots[i].target = target;
        shots[i].active = 1;
        magicCooldown = 45;
        return;
    }
}

static void updateShots(void) {
    for (int i=0; i<SHOT_COUNT; i++) {
        Shot *s = &shots[i];
        if (!s->active) continue;

        int valid = 0;

        if (s->target>=0 && s->target<ENEMY_COUNT)
            valid = enemies[s->target].active;
        else if (s->target==ENEMY_COUNT)
            valid = boss.active;

        if (!valid) {
            s->target = nearestEnemy(s->x,s->y);

            if (s->target < 0) {
                s->active = 0;
                continue;
            }
        }

        float tx,ty;

        if (s->target==ENEMY_COUNT) {
            tx = boss.x+18;
            ty = boss.y+22;
        } else {
            Enemy *e = &enemies[s->target];
            tx = e->x+e->w*.5f;
            ty = e->y+e->h*.5f;
        }

        float dx = tx-s->x;
        float dy = ty-s->y;
        float d = sqrtf(dx*dx+dy*dy);

        if (d < 12) {
            if (s->target==ENEMY_COUNT) {
                if (boss.active && boss.damageCooldown==0) {
                    boss.hp--;
                    boss.damageCooldown = 12;

                    if (boss.hp <= 0) {
                        boss.hp = 0;
                        boss.active = 0;
                        bossDefeated = 1;
                    }
                }
            } else {
                Enemy *e = &enemies[s->target];
                e->hp--;

                if (e->hp <= 0)
                    e->active = 0;
            }

            s->active = 0;
            continue;
        }

        if (d > .01f) {
            float vx = dx/d*4.8f;
            float vy = dy/d*4.8f;

            s->vx += (vx-s->vx)*.18f;
            s->vy += (vy-s->vy)*.18f;
        }

        s->x += s->vx;
        s->y += s->vy;

        if (s->x<0 || s->x>WORLD_W ||
            s->y< -10 || s->y>SH+20)
            s->active = 0;
    }
}

static void drawShots(void) {
    for (int i=0; i<SHOT_COUNT; i++) {
        Shot *s = &shots[i];
        if (!s->active) continue;

        float x = s->x-cameraX;
        float y = s->y;

        rect(x-s->vx*2,y-s->vy*2,12,6,DARK_GRAY);
        rect(x-4,y-4,9,9,BLUE);
        rect(x-1,y-1,3,3,LIGHT_BLUE);
    }
}

/* =========================================================
   ESPADA E MEIA-LUA
   ========================================================= */

static void attackSword(void) {
    if (swordCooldown > 0) return;

    swordCooldown = 20;
    swordTimer = 10;
    swordHitDone = 0;
}

static void applySwordDamage(void) {
    if (swordHitDone || swordTimer < 4) return;

    swordHitDone = 1;

    float ax = (swordFacing>0) ? px+PW : px-34;
    float ay = py+3;

    for (int i=0; i<ENEMY_COUNT; i++) {
        Enemy *e = &enemies[i];
        if (!e->active) continue;

        if (overlap(ax,ay,34,28,e->x,e->y,e->w,e->h)) {
            e->hp--;

            if (e->hp <= 0)
                e->active = 0;
        }
    }

    if (boss.active && boss.damageCooldown==0 &&
        overlap(ax,ay,34,28,boss.x,boss.y,40,44)) {
        boss.hp--;
        boss.damageCooldown = 12;

        if (boss.hp <= 0) {
            boss.hp = 0;
            boss.active = 0;
            bossDefeated = 1;
        }
    }
}

static void drawSword(void) {
    if (swordTimer <= 0) return;

    float x = px-cameraX;
    float y = py+11;
    int t = swordTimer;

    if (swordFacing > 0) {
        rect(x+15,y-8,4,18,GRAY);
        rect(x+18,y-13,4,22,WHITE);
        rect(x+22,y-17,4,22,GRAY);
        rect(x+26,y-20,4,20,WHITE);
        rect(x+14,y+1,19,3,DARK_GRAY);
        rect(x+17,y-2,3,8,WHITE);

        if (t > 2) {
            /* Meia-lua do golpe, desenhada em C. */
            rect(x+25,y-29,5,4,LIGHT_BLUE);
            rect(x+33,y-26,5,4,WHITE);
            rect(x+40,y-20,5,4,GRAY);
            rect(x+44,y-12,5,4,WHITE);
            rect(x+45,y-4,5,4,LIGHT_BLUE);

            if (t > 6) {
                rect(x+29,y-32,4,3,GRAY);
                rect(x+37,y-28,4,3,LIGHT_BLUE);
                rect(x+46,y-16,4,3,GRAY);
            }
        }
    } else {
        rect(x-1,y-8,4,18,GRAY);
        rect(x-5,y-13,4,22,WHITE);
        rect(x-9,y-17,4,22,GRAY);
        rect(x-13,y-20,4,20,WHITE);
        rect(x-14,y+1,19,3,DARK_GRAY);
        rect(x-5,y-2,3,8,WHITE);

        if (t > 2) {
            rect(x-30,y-29,5,4,LIGHT_BLUE);
            rect(x-38,y-26,5,4,WHITE);
            rect(x-45,y-20,5,4,GRAY);
            rect(x-49,y-12,5,4,WHITE);
            rect(x-50,y-4,5,4,LIGHT_BLUE);

            if (t > 6) {
                rect(x-34,y-32,4,3,GRAY);
                rect(x-42,y-28,4,3,LIGHT_BLUE);
                rect(x-51,y-16,4,3,GRAY);
            }
        }
    }
}

/* =========================================================
   MINI-CHEFE: HOLLOW GUARD
   ========================================================= */

static void drawMiniBoss(void) {
    if (!boss.active) return;

    float x = boss.x-cameraX;
    float y = boss.y;

    rect(x-5,y+41,46,3,BLACK);
    rect(x+4,y-5,6,8,WHITE);
    rect(x+26,y-5,6,8,WHITE);

    rect(x+5,y+2,26,12,GRAY);
    rect(x+2,y+7,32,8,WHITE);
    rect(x+8,y+8,6,4,BLACK);
    rect(x+22,y+8,6,4,BLACK);
    rect(x+10,y+9,3,2,WHITE);
    rect(x+24,y+9,3,2,WHITE);

    rect(x+6,y+15,24,17,WHITE);
    rect(x+10,y+18,16,10,DARK_GRAY);

    rect(x-3,y+15,9,17,GRAY);
    rect(x+30,y+15,9,17,GRAY);
    rect(x-5,y+28,11,10,WHITE);
    rect(x+29,y+28,11,10,WHITE);

    if (!boss.grounded) {
        rect(x+2,y+34,10,7,WHITE);
        rect(x+25,y+34,10,7,WHITE);
        rect(x-2,y+40,12,3,GRAY);
        rect(x+27,y+40,12,3,GRAY);
    } else {
        rect(x+7,y+32,9,10,GRAY);
        rect(x+20,y+32,9,10,GRAY);
    }

    rect(x+17,y+17,3,4,BLACK);
    rect(x+14,y+21,3,5,BLACK);
    rect(x+17,y+26,5,2,BLACK);

    if (boss.attack==1) {
        rect(x-9,y+17,5,18,WHITE);
        rect(x+39,y+17,5,18,WHITE);
        rect(x+5,y-9,26,3,GRAY);
    } else if (boss.attack==2) {
        rect(x-8,y+18,7,16,WHITE);
        rect(x+37,y+18,7,16,WHITE);
        rect(x+2,y+14,32,5,GRAY);
    }
}

static void updateMiniBoss(void) {
    if (!boss.active || bossDefeated) return;

    if (boss.contactCooldown > 0)
        boss.contactCooldown--;

    if (boss.damageCooldown > 0)
        boss.damageCooldown--;

    boss.timer++;

    float dx = px-boss.x;

    /*
       0 = perseguir
       1 = preparar salto
       2 = salto/ataque
       3 = recuperar apos aterrissar
    */

    if (boss.attack==0) {
        if (dx > 5) boss.x += .65f;
        else if (dx < -5) boss.x -= .65f;

        if (boss.x < 1945) boss.x = 1945;
        if (boss.x > 2320) boss.x = 2320;

        if (boss.timer >= 75 && fabsf(dx) < 360) {
            boss.attack = 1;
            boss.timer = 0;
            boss.vx = 0;
        }
    } else if (boss.attack==1) {
        if (boss.timer >= 22) {
            boss.attack = 2;
            boss.timer = 0;
            boss.grounded = 0;
            boss.vy = -8.0f;
            boss.vx = (px<boss.x) ? -2.4f : 2.4f;
        }
    } else if (boss.attack==2) {
        boss.x += boss.vx;
        boss.vy += .32f;

        if (boss.vy > 7.0f)
            boss.vy = 7.0f;

        boss.y += boss.vy;

        if (boss.x < 1940) {
            boss.x = 1940;
            boss.vx = 1.0f;
        }

        if (boss.x > 2340) {
            boss.x = 2340;
            boss.vx = -1.0f;
        }

        if (boss.y+44 >= 238 && boss.vy >= 0) {
            boss.y = 194;
            boss.vy = 0;
            boss.grounded = 1;
            boss.attack = 3;
            boss.timer = 0;
        }
    } else if (boss.attack==3) {
        if (boss.timer >= 40) {
            boss.attack = 0;
            boss.timer = 0;
        }
    }

    if (overlap(px,py,PW,PH,boss.x,boss.y,40,44) &&
        boss.contactCooldown==0) {
        playerTakeDamage();

        if (px+PW*.5f < boss.x+20)
            px = boss.x-PW-2;
        else
            px = boss.x+42;

        if (px < BOSS_START) px = BOSS_START;
        if (px > WORLD_W-PW) px = WORLD_W-PW;

        pvy = -3.0f;
        grounded = 0;
        jumps = 0;
        playerMoving = 0;
        boss.contactCooldown = 45;
    }
}

/* =========================================================
   HUD DO CHEFE
   ========================================================= */

static void drawBossHUD(void) {
    if (!bossEncounterStarted || bossDefeated)
        return;

    /* Barra e nome do chefe na parte inferior. */
    rect(70,235,340,29,BLACK);
    rect(70,235,340,2,DARK_GRAY);

    text(78,240,"HOLLOW GUARD",WHITE);

    rect(78,253,324,7,DARK_GRAY);

    if (boss.hp > 0) {
        rect(80,255,320.0f*boss.hp/10.0f,3,WHITE);
    }
}

/* =========================================================
   ATUALIZACAO DO JOGO
   ========================================================= */

static void updateGame(SceCtrlData pad,unsigned int pressed) {
    float vx = 0;

    if (pad.Buttons & PSP_CTRL_LEFT) {
        vx = -2.4f;
        swordFacing = -1;
    }

    if (pad.Buttons & PSP_CTRL_RIGHT) {
        vx = 2.4f;
        swordFacing = 1;
    }

    playerMoving = (vx!=0 && grounded);
    px += vx;

    if (px < 0) px = 0;
    if (px > WORLD_W-PW) px = WORLD_W-PW;

    if ((pressed & PSP_CTRL_CROSS) && jumps < 2) {
        pvy = -7.2f;
        grounded = 0;
        jumps++;
    }

    float oldBottom = py+PH;

    pvy += .30f;
    if (pvy > 7) pvy = 7;

    py += pvy;
    grounded = 0;

    Rect floors[] = {
        {0,238,120,34},{145,238,105,34},
        {275,238,165,34},{470,238,115,34},
        {620,238,180,34},{800,238,120,34},
        {945,238,105,34},{1075,238,165,34},
        {1270,238,115,34},{1420,238,180,34},
        {1600,238,320,34},{1920,238,480,34}
    };

    for (int i=0; i<(int)(sizeof(floors)/sizeof(floors[0])); i++) {
        Rect f = floors[i];

        if (pvy>=0 && oldBottom<=f.y+2 &&
            py+PH>=f.y && px+PW>f.x && px<f.x+f.w) {
            py = f.y-PH;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }
    }

    Rect plats[] = {
        {65,197,47,6},{150,166,43,6},{245,192,40,6},
        {330,151,52,6},{430,185,42,6},{535,160,46,6},
        {650,129,55,6},{740,177,40,6},
        {865,197,47,6},{950,166,43,6},{1045,192,40,6},
        {1130,151,52,6},{1230,185,42,6},{1335,160,46,6},
        {1450,129,55,6},{1540,177,40,6},
        {1665,197,47,6},{1750,166,43,6},{1845,192,40,6},
        {2030,185,42,6},{2135,160,46,6},{2250,129,55,6}
    };

    for (int i=0; i<(int)(sizeof(plats)/sizeof(plats[0])); i++) {
        Rect p = plats[i];

        if (pvy>=0 && oldBottom<=p.y+2 &&
            py+PH>=p.y && px+PW>p.x && px<p.x+p.w) {
            py = p.y-PH;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }
    }

    if (!magicUnlocked &&
        overlap(px,py,PW,PH,145,151,22,22)) {
        magicUnlocked = 1;
    }

    if (swordCooldown > 0) swordCooldown--;
    if (swordTimer > 0) swordTimer--;
    if (magicCooldown > 0) magicCooldown--;

    if (playerDamageCooldown > 0)
        playerDamageCooldown--;

    if (pressed & PSP_CTRL_SQUARE)
        attackSword();

    applySwordDamage();

    if (magicUnlocked &&
        (pressed & PSP_CTRL_CIRCLE) &&
        magicCooldown==0) {
        fireMagic();
    }

    updateEnemies();

    if (px >= BOSS_START)
        bossEncounterStarted = 1;

    updateMiniBoss();
    updateShots();

    if (py > SH+60) {
        playerTakeDamage();

        px = 35;
        py = 190;
        pvy = 0;
        grounded = 0;
        jumps = 0;
        playerMoving = 0;
    }

    /* A arena fica fechada ate o chefe ser derrotado. */
    if (boss.active && bossEncounterStarted) {
        if (px < BOSS_START)
            px = BOSS_START;

        if (px > BOSS_END-PW-30)
            px = BOSS_END-PW-30;
    }

    cameraX = px-210;

    if (cameraX < 0)
        cameraX = 0;

    if (cameraX > WORLD_W-SW)
        cameraX = WORLD_W-SW;

    room = px<800 ? 0 : (px<1600 ? 1 : 2);

    if (px > WORLD_W-45 && bossDefeated)
        state = 3;
}

/* =========================================================
   MENUS
   ========================================================= */

static void drawMenu(void) {
    rect(0,0,SW,SH,BLACK);
    rect(65,45,350,3,WHITE);
    rect(65,48,3,52,WHITE);
    rect(412,48,3,52,WHITE);
    rect(65,100,350,3,WHITE);

    text(174,63,"RANDOM",WHITE);
    text(132,124,"PRESS START",WHITE);
    text(126,157,"FORGOTTEN CAVE",WHITE);

    rect(120,215,240,3,WHITE);
}

static void drawIntro(void) {
    rect(0,0,SW,SH,BLACK);
    text(174,32,"RANDOM",WHITE);

    if (introPage==0) {
        text(66,85,"WHERE AM I",WHITE);
        text(60,126,"FIND THE SWORD",WHITE);
    } else {
        text(54,82,"FOLLOW BLUE LIGHT",WHITE);
        text(60,126,"PRESS X TO BEGIN",WHITE);
    }

    text(120,225,"PRESS X",WHITE);
}

static void drawGame(void) {
    beginFrame();

    drawCave();
    drawEnemies();
    drawMagicPickup();
    drawShots();
    drawMiniBoss();
    drawPlayer();
    drawSword();

    /* Topo esquerdo: apenas magia. */
    if (magicUnlocked) {
        if (magicCooldown==0)
            text(8,8,"MAGIC READY",LIGHT_BLUE);
        else
            text(8,8,"MAGIC RECHARGING",BLUE);
    } else {
        text(8,8,"FIND BLUE CRYSTAL",WHITE);
    }

    /* Vida do jogador: quatro barrinhas. */
    drawPlayerHealth();

    /* Topo direito: localizacao. */
    if (room==0)
        text(350,8,"CAVE",WHITE);
    else if (room==1)
        text(350,8,"DEPTHS",WHITE);
    else
        text(350,8,bossDefeated ? "EXIT OPEN" : "SANCTUARY",WHITE);

    /* Parte inferior: vida do chefe. */
    drawBossHUD();

    endFrame();
}

static void drawEnding(void) {
    rect(0,0,SW,SH,BLACK);
    text(174,52,"RANDOM",WHITE);
    text(66,105,"THE WORLD CONTINUES",WHITE);
    text(102,145,"PRESS START",WHITE);
}

/* =========================================================
   RESET
   ========================================================= */

static void resetWorld(void) {
    px = 35;
    py = 190;
    pvy = 0;
    cameraX = 0;

    magicUnlocked = 0;
    jumps = 0;
    grounded = 0;
    playerMoving = 0;

    magicCooldown = 0;
    swordCooldown = 0;
    swordTimer = 0;
    swordHitDone = 0;

    playerHP = PLAYER_MAX_HP;
    playerDamageCooldown = 0;

    bossEncounterStarted = 0;

    memset(shots,0,sizeof(shots));

    for (int i=0; i<ENEMY_COUNT; i++) {
        enemies[i].active = 1;
        enemies[i].hp = 1;
    }

    enemies[ENEMY_COUNT-1].active = 0;

    boss.x = 2180;
    boss.y = 194;
    boss.vx = 0;
    boss.vy = 0;
    boss.hp = 10;
    boss.active = 1;
    boss.timer = 0;
    boss.attack = 0;
    boss.contactCooldown = 0;
    boss.damageCooldown = 0;
    boss.grounded = 1;

    bossDefeated = 0;
}

/* =========================================================
   PRINCIPAL
   ========================================================= */

int main(void) {
    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    memset(shots,0,sizeof(shots));

    initGraphics();

    while (1) {
        sceCtrlReadBufferPositive(&pad,1);

        unsigned int pressed = pad.Buttons & ~previousButtons;
        previousButtons = pad.Buttons;

        if (state==0) {
            beginFrame();
            drawMenu();
            endFrame();

            if (pressed & (PSP_CTRL_START|PSP_CTRL_CROSS)) {
                state = 1;
                introPage = 0;
            }
        } else if (state==1) {
            beginFrame();
            drawIntro();
            endFrame();

            if (pressed & PSP_CTRL_CROSS) {
                if (introPage==0) {
                    introPage = 1;
                } else {
                    resetWorld();
                    state = 2;
                }
            }

            if (pressed & PSP_CTRL_START) {
                resetWorld();
                state = 2;
            }
        } else if (state==2) {
            if (pressed & PSP_CTRL_START) {
                state = 0;
            } else {
                updateGame(pad,pressed);
                drawGame();
            }
        } else {
            beginFrame();
            drawEnding();
            endFrame();

            if (pressed & PSP_CTRL_START) {
                resetWorld();
                state = 0;
            }
        }

        frame++;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
