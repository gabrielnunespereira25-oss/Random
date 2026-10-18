/*
 * RANDOM - PSP Homebrew
 * Ataque de meia-lua no lugar da espada
 * Desenhos feitos em C, sem PNG ou GIF
 *
 * Controles:
 * Direcional: mover
 * X: pular / pulo duplo
 * Quadrado: ataque de meia-lua
 * Círculo: magia, após pegar o cristal
 * Start: iniciar / pausar
 */

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
#define PLAYER_W 18.0f
#define PLAYER_H 30.0f

#define ENEMY_COUNT 12
#define SHOT_COUNT 6
#define SPARK_COUNT 32

#define BOSS_START 1920.0f
#define BOSS_END 2400.0f

#define MOON_DURATION 15
#define MOON_RADIUS 49.0f

#define GRAVITY 0.36f
#define MAX_FALL_SPEED 7.5f

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y;
    float vx, vy;
    float w, h;
    int active;
    int hp;
    int cooldown;
} Enemy;

typedef struct {
    float x, y;
    float vx, vy;
    int active;
    int life;
    int target;
} Shot;

typedef struct {
    float x, y;
    float vx, vy;
    int active;
    int life;
    unsigned int color;
} Spark;

typedef struct {
    float x, y;
    float vx, vy;
    int hp;
    int active;
    int timer;
    int attack;
    int contactCooldown;
    int damageCooldown;
    int grounded;
} MiniBoss;

typedef struct {
    int active;
    int timer;
    int direction;
    int hitMask;
    float x, y;
} MoonSlash;

static unsigned int __attribute__((aligned(16))) displayList[262144];

static int gameState = 0;
/* 0 = menu, 1 = jogo, 2 = fim, 3 = game over */

static float px = 80.0f;
static float py = 180.0f;
static float pvx = 0.0f;
static float pvy = 0.0f;

static int facing = 1;
static int grounded = 0;
static int jumps = 0;
static int playerHP = 4;
static int magicUnlocked = 0;
static int bossEncounterStarted = 0;
static int bossDefeated = 0;

static float cameraX = 0.0f;

static int moonCooldown = 0;
static MoonSlash moon = {0};

static Enemy enemies[ENEMY_COUNT];
static Shot shots[SHOT_COUNT];
static Spark sparks[SPARK_COUNT];

static MiniBoss boss = {
    2180.0f, 194.0f,
    0.0f, 0.0f,
    10, 1,
    0, 0,
    0, 0, 1
};

static unsigned int frameCount = 0;

/* Cores no formato usado pelo GU do PSP. */
#define C_BLACK       0xFF101018
#define C_DARK        0xFF292938
#define C_STONE       0xFF454552
#define C_STONE2      0xFF666675
#define C_BLUE        0xFFFF8040
#define C_LIGHT_BLUE  0xFFFFD080
#define C_WHITE       0xFFF4F4F4
#define C_SILVER      0xFFD0D0D8
#define C_RED         0xFF3030E8
#define C_GREEN       0xFF50D050
#define C_PURPLE      0xFFB060E0
#define C_YELLOW      0xFF40D8F8

static inline float maxf(float a, float b)
{
    return a > b ? a : b;
}

static inline float minf(float a, float b)
{
    return a < b ? a : b;
}

static inline float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float absf(float x)
{
    return x < 0.0f ? -x : x;
}

static int overlap(
    float ax, float ay, float aw, float ah,
    float bx, float by, float bw, float bh)
{
    return ax < bx + bw &&
           ax + aw > bx &&
           ay < by + bh &&
           ay + ah > by;
}

/* =========================
   GRÁFICOS
   ========================= */

static void rect(
    float x, float y,
    float w, float h,
    unsigned int color)
{
    if (w <= 0.0f || h <= 0.0f)
        return;

    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 2);

    if (!v)
        return;

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v);
}

static void beginFrame(void)
{
    sceGuStart(GU_DIRECT, displayList);

    sceGuClearColor(C_BLACK);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
}

static void endFrame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    frameCount++;
}

static void drawBackground(void)
{
    int i;

    rect(0, 0, SW, SH, C_BLACK);

    for (i = 0; i < 18; i++) {
        float x = (float)((i * 83) % SW);
        float y = (float)((i * 47) % 130);

        rect(x, y, 2, 2, C_STONE2);
    }

    rect(0, 225, SW, 47, C_DARK);
}

static void drawCaveRoom(int room)
{
    int i;
    float base = (float)(room * 800);
    float screenX = base - cameraX;

    rect(screenX, 0, 800, 20, C_DARK);

    for (i = 0; i < 10; i++) {
        float x = screenX + i * 91.0f;

        if (x > -30 && x < SW + 30) {
            rect(x, 0, 25, 15 + (i % 4) * 6, C_STONE);
            rect(x + 5, 5, 12, 5, C_STONE2);
        }
    }

    rect(screenX, 250, 800, 22, C_STONE);

    for (i = 0; i < 16; i++) {
        float x = screenX + i * 51.0f;

        if (x > -50 && x < SW + 50) {
            rect(x, 250, 2, 22, C_STONE2);
        }
    }

    if (room == 0) {
        rect(screenX + 210, 202, 100, 12, C_STONE);
        rect(screenX + 430, 166, 110, 12, C_STONE);
        rect(screenX + 610, 210, 90, 12, C_STONE);
    } else if (room == 1) {
        rect(screenX + 130, 205, 100, 12, C_STONE);
        rect(screenX + 360, 174, 110, 12, C_STONE);
        rect(screenX + 570, 202, 120, 12, C_STONE);
    }
}

static void drawBossRoom(void)
{
    int i;
    float sx = BOSS_START - cameraX;

    rect(sx, 0, 480, 272, C_DARK);
    rect(sx, 0, 480, 22, C_STONE);
    rect(sx, 250, 480, 22, C_STONE);

    for (i = 0; i < 10; i++) {
        float x = sx + i * 50.0f;
        rect(x, 22, 4, 25, C_STONE2);
        rect(x + 20, 225, 5, 25, C_STONE2);
    }

    rect(sx + 70, 200, 60, 10, C_STONE);
    rect(sx + 350, 190, 55, 10, C_STONE);

    if (!bossEncounterStarted) {
        rect(sx + 3, 20, 7, 230, C_PURPLE);
        rect(sx + 10, 20, 3, 230, C_LIGHT_BLUE);
    }
}

static void drawPlayer(void)
{
    float sx = px - cameraX;

    /* Corpo */
    rect(sx, py, PLAYER_W, PLAYER_H, C_BLUE);

    /* Cabeça */
    rect(sx + 3, py - 7, 12, 9, C_LIGHT_BLUE);

    /* Olho */
    if (facing > 0)
        rect(sx + 12, py - 4, 3, 3, C_WHITE);
    else
        rect(sx + 3, py - 4, 3, 3, C_WHITE);

    /* Pernas animadas */
    if (!grounded) {
        rect(sx + 2, py + 24, 5, 7, C_SILVER);
        rect(sx + 11, py + 23, 5, 8, C_SILVER);
    } else {
        int step = (int)(frameCount / 6) % 2;

        rect(sx + 2, py + 25, 5, step ? 5 : 7, C_SILVER);
        rect(sx + 11, py + 25, 5, step ? 7 : 5, C_SILVER);
    }
}

/* =========================
   MEIA-LUA
   ========================= */

static void startMoonSlash(void)
{
    if (moon.active || moonCooldown > 0)
        return;

    moon.active = 1;
    moon.timer = MOON_DURATION;
    moon.direction = facing;
    moon.hitMask = 0;
    moon.x = px + PLAYER_W / 2.0f;
    moon.y = py + PLAYER_H / 2.0f;

    moonCooldown = 22;
}

static void drawMoonSlash(void)
{
    int i;

    if (!moon.active)
        return;

    float progress =
        1.0f - (float)moon.timer / (float)MOON_DURATION;

    float radius = MOON_RADIUS + progress * 9.0f;

    float cx = moon.x - cameraX;
    float cy = moon.y;

    /*
       O arco é desenhado com pequenos quadrados.
       A espada não é desenhada em nenhum momento.
    */
    for (i = 0; i <= 20; i++) {
        float angle = -1.5707963f + i * 0.1570796f;

        float dx = cosf(angle) * radius;
        float dy = sinf(angle) * radius;

        if (moon.direction < 0)
            dx = -dx;

        float x = cx + dx;
        float y = cy + dy;

        rect(x - 4, y - 4, 8, 8, C_BLUE);
        rect(x - 2, y - 2, 4, 4, C_LIGHT_BLUE);
        rect(x - 1, y - 1, 2, 2, C_WHITE);
    }
}

/* =========================
   PARTÍCULAS
   ========================= */

static void spawnSparks(
    float x, float y,
    unsigned int color,
    int count)
{
    int i;

    for (i = 0; i < SPARK_COUNT && count > 0; i++) {
        if (!sparks[i].active) {
            float angle = (float)(i * 47 + frameCount * 13)
                        * 0.1047197f;

            sparks[i].active = 1;
            sparks[i].x = x;
            sparks[i].y = y;
            sparks[i].vx = cosf(angle) * (1.0f + (i % 3));
            sparks[i].vy = sinf(angle) * (1.0f + (i % 3));
            sparks[i].life = 12 + (i % 8);
            sparks[i].color = color;
            count--;
        }
    }
}

static void spawnPlayerSparks(void)
{
    spawnSparks(
        px + PLAYER_W / 2.0f,
        py + PLAYER_H / 2.0f,
        C_BLUE, 12);
}

static void spawnBossSparks(void)
{
    spawnSparks(
        boss.x + 20.0f,
        boss.y + 20.0f,
        C_SILVER, 14);
}

static void updateSparks(void)
{
    int i;

    for (i = 0; i < SPARK_COUNT; i++) {
        if (!sparks[i].active)
            continue;

        sparks[i].x += sparks[i].vx;
        sparks[i].y += sparks[i].vy;

        sparks[i].vx *= 0.94f;
        sparks[i].vy *= 0.94f;
        sparks[i].vy += 0.08f;

        sparks[i].life--;

        if (sparks[i].life <= 0)
            sparks[i].active = 0;
    }
}

static void drawSparks(void)
{
    int i;

    for (i = 0; i < SPARK_COUNT; i++) {
        if (sparks[i].active) {
            rect(
                sparks[i].x - cameraX,
                sparks[i].y,
                3, 3,
                sparks[i].color);
        }
    }
}

/* =========================
   INIMIGOS
   ========================= */

static void initEnemies(void)
{
    int i;

    memset(enemies, 0, sizeof(enemies));

    for (i = 0; i < ENEMY_COUNT; i++) {
        enemies[i].x = 180.0f + i * 145.0f;
        enemies[i].y = 218.0f;
        enemies[i].w = 20.0f;
        enemies[i].h = 22.0f;
        enemies[i].vx = (i % 2) ? -1.0f : 1.0f;
        enemies[i].active = 1;
        enemies[i].hp = 2;
        enemies[i].cooldown = 0;
    }

    /* Um cristal mágico na primeira parte do mapa. */
    magicUnlocked = 0;
}

static void drawEnemies(void)
{
    int i;

    for (i = 0; i < ENEMY_COUNT; i++) {
        if (!enemies[i].active)
            continue;

        float sx = enemies[i].x - cameraX;

        rect(sx, enemies[i].y, enemies[i].w, enemies[i].h, C_PURPLE);
        rect(sx + 3, enemies[i].y + 4, 5, 4, C_WHITE);
        rect(sx + 12, enemies[i].y + 4, 5, 4, C_WHITE);
    }
}

static void updateEnemies(void)
{
    int i;

    for (i = 0; i < ENEMY_COUNT; i++) {
        Enemy *e = &enemies[i];

        if (!e->active)
            continue;

        e->x += e->vx;

        if (e->x < 40.0f || e->x > WORLD_W - 80.0f)
            e->vx = -e->vx;

        if (e->cooldown > 0)
            e->cooldown--;

        if (playerHP > 0 &&
            e->cooldown == 0 &&
            overlap(px, py, PLAYER_W, PLAYER_H,
                    e->x, e->y, e->w, e->h)) {
            playerHP--;
            e->cooldown = 55;
            pvx = (px < e->x) ? -4.0f : 4.0f;
            pvy = -4.0f;
            spawnPlayerSparks();

            if (playerHP <= 0)
                gameState = 3;
        }
    }
}

/* =========================
   MAGIA HOMING
   ========================= */

static int nearestEnemy(float x, float y)
{
    int i;
    int best = -1;
    float bestDist = 999999.0f;

    for (i = 0; i < ENEMY_COUNT; i++) {
        if (!enemies[i].active)
            continue;

        float dx = enemies[i].x - x;
        float dy = enemies[i].y - y;
        float d = dx * dx + dy * dy;

        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }

    if (boss.active && boss.hp > 0) {
        float dx = boss.x - x;
        float dy = boss.y - y;
        float d = dx * dx + dy * dy;

        if (d < bestDist)
            best = ENEMY_COUNT;
    }

    return best;
}

static void fireMagic(void)
{
    int i;

    if (!magicUnlocked)
        return;

    for (i = 0; i < SHOT_COUNT; i++) {
        if (!shots[i].active) {
            shots[i].active = 1;
            shots[i].x = px + PLAYER_W / 2.0f;
            shots[i].y = py + PLAYER_H / 2.0f;
            shots[i].vx = facing * 4.0f;
            shots[i].vy = 0.0f;
            shots[i].life = 100;
            shots[i].target = nearestEnemy(px, py);
            break;
        }
    }
}

static void updateShots(void)
{
    int i;

    for (i = 0; i < SHOT_COUNT; i++) {
        Shot *s = &shots[i];

        if (!s->active)
            continue;

        float tx = s->x + s->vx * 10.0f;
        float ty = s->y + s->vy * 10.0f;

        if (s->target >= 0 && s->target < ENEMY_COUNT &&
            enemies[s->target].active) {
            tx = enemies[s->target].x + enemies[s->target].w / 2.0f;
            ty = enemies[s->target].y + enemies[s->target].h / 2.0f;
        } else if (s->target == ENEMY_COUNT &&
                   boss.active && boss.hp > 0) {
            tx = boss.x + 20.0f;
            ty = boss.y + 20.0f;
        } else {
            s->target = nearestEnemy(s->x, s->y);
        }

        float dx = tx - s->x;
        float dy = ty - s->y;
        float d = sqrtf(dx * dx + dy * dy);

        if (d > 0.1f) {
            s->vx += (dx / d) * 0.22f;
            s->vy += (dy / d) * 0.22f;
        }

        s->vx = clampf(s->vx, -5.0f, 5.0f);
        s->vy = clampf(s->vy, -5.0f, 5.0f);

        s->x += s->vx;
        s->y += s->vy;
        s->life--;

        int hit = 0;

        for (int j = 0; j < ENEMY_COUNT; j++) {
            if (enemies[j].active &&
                overlap(s->x, s->y, 8, 8,
                        enemies[j].x, enemies[j].y,
                        enemies[j].w, enemies[j].h)) {
                enemies[j].hp--;
                spawnSparks(s->x, s->y, C_LIGHT_BLUE, 5);

                if (enemies[j].hp <= 0)
                    enemies[j].active = 0;

                hit = 1;
                break;
            }
        }

        if (!hit && boss.active && boss.hp > 0 &&
            overlap(s->x, s->y, 8, 8,
                    boss.x, boss.y, 42, 45)) {
            if (boss.damageCooldown == 0) {
                boss.hp--;
                boss.damageCooldown = 10;
                spawnBossSparks();

                if (boss.hp <= 0) {
                    boss.active = 0;
                    bossDefeated = 1;
                }
            }

            hit = 1;
        }

        if (hit || s->life <= 0 ||
            s->x < 0 || s->x > WORLD_W ||
            s->y < -20 || s->y > SH + 20) {
            s->active = 0;
        }
    }
}

static void drawShots(void)
{
    int i;

    for (i = 0; i < SHOT_COUNT; i++) {
        if (!shots[i].active)
            continue;

        rect(
            shots[i].x - cameraX,
            shots[i].y,
            9, 9,
            C_LIGHT_BLUE);

        rect(
            shots[i].x - cameraX + 2,
            shots[i].y + 2,
            5, 5,
            C_WHITE);
    }
}

/* =========================
   COLISÃO DO MAPA
   ========================= */

static void resolvePlatforms(float oldBottom)
{
    float bottom = py + PLAYER_H;

    /* Piso principal. */
    if (bottom >= 250.0f && pvy >= 0.0f) {
        py = 250.0f - PLAYER_H;
        pvy = 0.0f;
        grounded = 1;
        jumps = 0;
    }

    /* Plataformas da sala inicial. */
    if (px < 800.0f) {
        if (px + PLAYER_W > 210 && px < 310 &&
            oldBottom <= 202 && bottom >= 202 && pvy >= 0) {
            py = 202 - PLAYER_H;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }

        if (px + PLAYER_W > 430 && px < 540 &&
            oldBottom <= 166 && bottom >= 166 && pvy >= 0) {
            py = 166 - PLAYER_H;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }

        if (px + PLAYER_W > 610 && px < 700 &&
            oldBottom <= 210 && bottom >= 210 && pvy >= 0) {
            py = 210 - PLAYER_H;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }
    }

    /* Plataformas da segunda sala. */
    if (px >= 800 && px < 1600) {
        if (px + PLAYER_W > 930 && px < 1030 &&
            oldBottom <= 205 && bottom >= 205 && pvy >= 0) {
            py = 205 - PLAYER_H;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }

        if (px + PLAYER_W > 1160 && px < 1270 &&
            oldBottom <= 174 && bottom >= 174 && pvy >= 0) {
            py = 174 - PLAYER_H;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }

        if (px + PLAYER_W > 1370 && px < 1490 &&
            oldBottom <= 202 && bottom >= 202 && pvy >= 0) {
            py = 202 - PLAYER_H;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }
    }

    /* Plataformas da arena. */
    if (px >= BOSS_START) {
        if (px + PLAYER_W > 1990 && px < 2050 &&
            oldBottom <= 200 && bottom >= 200 && pvy >= 0) {
            py = 200 - PLAYER_H;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }

        if (px + PLAYER_W > 2270 && px < 2335 &&
            oldBottom <= 190 && bottom >= 190 && pvy >= 0) {
            py = 190 - PLAYER_H;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }
    }
}

/* =========================
   MINIBOSS
   ========================= */

static void drawBoss(void)
{
    if (!boss.active || boss.hp <= 0)
        return;

    float sx = boss.x - cameraX;

    rect(sx, boss.y, 42, 45, C_PURPLE);
    rect(sx + 5, boss.y + 7, 10, 7, C_RED);
    rect(sx + 27, boss.y + 7, 10, 7, C_RED);
    rect(sx + 9, boss.y + 31, 25, 6, C_SILVER);

    /* Barra de vida do miniboss. */
    rect(130, 12, 220, 10, C_DARK);
    rect(132, 14, 216.0f * boss.hp / 10.0f, 6, C_RED);
}

static void updateBoss(void)
{
    if (!bossEncounterStarted || !boss.active || boss.hp <= 0)
        return;

    if (boss.damageCooldown > 0)
        boss.damageCooldown--;

    if (boss.contactCooldown > 0)
        boss.contactCooldown--;

    boss.timer++;

    if (boss.attack == 0) {
        boss.vx = (px < boss.x) ? -1.25f : 1.25f;

        if (boss.timer > 90) {
            boss.attack = 1;
            boss.timer = 0;
        }
    } else {
        boss.vx = 0.0f;

        if (boss.timer == 1) {
            boss.vy = -5.0f;
        }

        if (boss.timer > 45) {
            boss.attack = 0;
            boss.timer = 0;
        }
    }

    boss.x += boss.vx;
    boss.y += boss.vy;
    boss.vy += GRAVITY;

    if (boss.y + 45 >= 250) {
        boss.y = 205;
        boss.vy = 0;
        boss.grounded = 1;
    } else {
        boss.grounded = 0;
    }

    boss.x = clampf(boss.x, BOSS_START + 50, BOSS_END - 60);

    if (boss.contactCooldown == 0 &&
        overlap(px, py, PLAYER_W, PLAYER_H,
                boss.x, boss.y, 42, 45)) {
        if (playerHP > 0) {
            playerHP--;
            boss.contactCooldown = 60;
            pvx = (px < boss.x) ? -4.5f : 4.5f;
            pvy = -4.0f;
            spawnPlayerSparks();

            if (playerHP <= 0)
                gameState = 3;
        }
    }
}

/* =========================
   DANO DA MEIA-LUA
   ========================= */

static void moonDamage(void)
{
    int i;

    if (!moon.active)
        return;

    float centerX = moon.x;
    float centerY = moon.y;

    for (i = 0; i < ENEMY_COUNT; i++) {
        if (!enemies[i].active)
            continue;

        if (moon.hitMask & (1 << (i % 16)))
            continue;

        float ex = enemies[i].x + enemies[i].w / 2.0f;
        float ey = enemies[i].y + enemies[i].h / 2.0f;

        float dx = ex - centerX;
        float dy = ey - centerY;

        if (moon.direction > 0 && dx < -5)
            continue;

        if (moon.direction < 0 && dx > 5)
            continue;

        if (dx * dx + dy * dy <= MOON_RADIUS * MOON_RADIUS) {
            enemies[i].hp--;
            moon.hitMask |= (1 << (i % 16));

            spawnSparks(ex, ey, C_SILVER, 8);

            if (enemies[i].hp <= 0)
                enemies[i].active = 0;
        }
    }

    if (bossEncounterStarted && boss.active && boss.hp > 0) {
        float bx = boss.x + 21.0f;
        float by = boss.y + 22.0f;

        float dx = bx - centerX;
        float dy = by - centerY;

        int inDirection =
            (moon.direction > 0 && dx >= -5) ||
            (moon.direction < 0 && dx <= 5);

        if (inDirection &&
            dx * dx + dy * dy <= MOON_RADIUS * MOON_RADIUS &&
            boss.damageCooldown == 0) {
            boss.hp--;
            boss.damageCooldown = 12;
            spawnBossSparks();

            if (boss.hp <= 0) {
                boss.active = 0;
                bossDefeated = 1;
            }
        }
    }
}

/* =========================
   CRISTAL MÁGICO
   ========================= */

static void drawMagicCrystal(void)
{
    if (magicUnlocked)
        return;

    float sx = 350.0f - cameraX;
    float sy = 218.0f + sinf((float)frameCount * 0.08f) * 4.0f;

    rect(sx + 4, sy - 8, 10, 17, C_LIGHT_BLUE);
    rect(sx + 1, sy - 3, 16, 7, C_BLUE);
    rect(sx + 5, sy - 6, 8, 12, C_WHITE);
}

static void checkCrystalPickup(void)
{
    if (magicUnlocked)
        return;

    if (overlap(px, py, PLAYER_W, PLAYER_H,
                350, 210, 22, 25)) {
        magicUnlocked = 1;
        spawnSparks(360, 220, C_LIGHT_BLUE, 18);
    }
}

/* =========================
   HUD
   ========================= */

static void drawHUD(void)
{
    int i;

    rect(10, 10, 84, 12, C_DARK);

    for (i = 0; i < 4; i++) {
        rect(
            13 + i * 19,
            13,
            14, 6,
            i < playerHP ? C_RED : C_STONE);
    }

    /* Indicador da magia desbloqueada. */
    rect(405, 10, 60, 12, C_DARK);

    if (magicUnlocked)
        rect(409, 13, 52, 6, C_LIGHT_BLUE);
    else
        rect(409, 13, 52, 6, C_STONE);
}

/* =========================
   CENAS
   ========================= */

static void drawMenu(void)
{
    drawBackground();

    rect(95, 62, 290, 58, C_DARK);
    rect(105, 72, 270, 38, C_PURPLE);
    rect(115, 82, 250, 18, C_BLUE);

    rect(145, 157, 190, 10, C_STONE2);
    rect(170, 181, 140, 8, C_STONE);

    rect(175, 211, 130, 6, C_LIGHT_BLUE);
}

static void drawEnding(void)
{
    drawBackground();

    rect(90, 65, 300, 70, C_DARK);
    rect(105, 80, 270, 12, C_LIGHT_BLUE);
    rect(125, 103, 230, 8, C_WHITE);

    rect(145, 175, 190, 10, C_STONE2);
    rect(170, 198, 140, 8, C_PURPLE);
}

static void drawGameOver(void)
{
    drawBackground();

    rect(80, 70, 320, 80, C_DARK);
    rect(100, 88, 280, 15, C_RED);
    rect(130, 116, 220, 8, C_STONE2);
    rect(170, 184, 140, 8, C_LIGHT_BLUE);
}

/* =========================
   ATUALIZAÇÃO DO JOGO
   ========================= */

static void updateGame(SceCtrlData pad, unsigned int pressed)
{
    float oldBottom;
    float speed = 2.8f;

    if (gameState != 1)
        return;

    if (moonCooldown > 0)
        moonCooldown--;

    if (pad.Buttons & PSP_CTRL_LEFT) {
        pvx = -speed;
        facing = -1;
    } else if (pad.Buttons & PSP_CTRL_RIGHT) {
        pvx = speed;
        facing = 1;
    } else {
        pvx *= 0.72f;

        if (absf(pvx) < 0.15f)
            pvx = 0.0f;
    }

    if ((pressed & PSP_CTRL_CROSS) && jumps < 2) {
        pvy = (jumps == 0) ? -8.4f : -7.6f;
        grounded = 0;
        jumps++;
    }

    if (pressed & PSP_CTRL_SQUARE)
        startMoonSlash();

    if (pressed & PSP_CTRL_CIRCLE)
        fireMagic();

    oldBottom = py + PLAYER_H;

    pvy += GRAVITY;
    pvy = minf(pvy, MAX_FALL_SPEED);

    px += pvx;
    py += pvy;

    px = clampf(px, 0.0f, WORLD_W - PLAYER_W);

    grounded = 0;
    resolvePlatforms(oldBottom);

    if (py < 20)
        py = 20;

    checkCrystalPickup();

    /* Inicia o combate quando o jogador chega à arena. */
    if (px >= BOSS_START && !bossEncounterStarted) {
        bossEncounterStarted = 1;
    }

    /* Impede sair da arena durante o combate. */
    if (bossEncounterStarted && !bossDefeated) {
        px = clampf(px, BOSS_START, BOSS_END - PLAYER_W);
    }

    /* Atualiza a meia-lua e verifica dano. */
    if (moon.active) {
        moon.timer--;
        moonDamage();

        if (moon.timer <= 0)
            moon.active = 0;
    }

    updateEnemies();
    updateShots();
    updateBoss();
    updateSparks();

    /* Vitória quando o miniboss é derrotado. */
    if (bossDefeated && px >= BOSS_END - 100.0f) {
        gameState = 2;
    }

    cameraX = px - SW / 2.0f;
    cameraX = clampf(cameraX, 0.0f, WORLD_W - SW);

    if (pad.Buttons & PSP_CTRL_START) {
        /* Reiniciar a partir do menu é feito no game over/fim. */
    }
}

/* =========================
   INICIALIZAÇÃO
   ========================= */

static void resetGame(void)
{
    px = 80.0f;
    py = 180.0f;
    pvx = 0.0f;
    pvy = 0.0f;

    facing = 1;
    grounded = 0;
    jumps = 0;
    playerHP = 4;

    magicUnlocked = 0;
    bossEncounterStarted = 0;
    bossDefeated = 0;

    cameraX = 0.0f;
    moonCooldown = 0;
    moon.active = 0;

    memset(shots, 0, sizeof(shots));
    memset(sparks, 0, sizeof(sparks));

    boss.x = 2180.0f;
    boss.y = 205.0f;
    boss.vx = 0.0f;
    boss.vy = 0.0f;
    boss.hp = 10;
    boss.active = 1;
    boss.timer = 0;
    boss.attack = 0;
    boss.contactCooldown = 0;
    boss.damageCooldown = 0;
    boss.grounded = 1;

    initEnemies();
}

int main(void)
{
    SceCtrlData pad;
    unsigned int previousButtons = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();

    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, STRIDE);
    sceGuDispBuffer(SW, SH, (void *)0x88000, STRIDE);
    sceGuDepthBuffer((void *)0x110000, STRIDE);

    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);
    sceGuScissor(0, 0, SW, SH);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    resetGame();

    while (1) {
        unsigned int pressed;

        sceCtrlReadBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~previousButtons;
        previousButtons = pad.Buttons;

        if (gameState == 0) {
            if (pressed & PSP_CTRL_START) {
                resetGame();
                gameState = 1;
            }

            beginFrame();
            drawMenu();
            endFrame();
        } else if (gameState == 1) {
            updateGame(pad, pressed);

            beginFrame();

            drawBackground();

            drawCaveRoom(0);
            drawCaveRoom(1);
            drawBossRoom();

            drawMagicCrystal();
            drawEnemies();
            drawShots();
            drawBoss();
            drawMoonSlash();
            drawPlayer();
            drawSparks();
            drawHUD();

            endFrame();
        } else if (gameState == 2) {
            if (pressed & PSP_CTRL_START)
                gameState = 0;

            beginFrame();
            drawEnding();
            endFrame();
        } else if (gameState == 3) {
            if (pressed & PSP_CTRL_START)
                gameState = 0;

            beginFrame();
            drawGameOver();
            endFrame();
        }
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
