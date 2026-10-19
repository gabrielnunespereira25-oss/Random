
/*
 * RANDOM
 * Reconstrucao independente para PSP / PSPSDK
 *
 * Controles:
 * D-Pad esquerda/direita : mover
 * X                      : pular
 * Quadrado               : atacar
 * Circulo                : magia
 * Start                  : reiniciar
 *
 * Sistemas:
 * - Labirinto de cavernas com salas e bifurcacoes
 * - Personagem desenhado com linhas e curvas
 * - Zumbis perseguidores
 * - Boss com 500 HP
 * - Vida, dano, magia, camera e colisoes
 *
 * Arquivo: src/main.c
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272

#define WORLD_W 5200.0f
#define FLOOR_Y 226.0f

#define PLAYER_W 22.0f
#define PLAYER_H 38.0f

#define ZOMBIE_MAX 10
#define ROOM_COUNT 12

#define GRAVITY 650.0f
#define MOVE_SPEED 150.0f
#define JUMP_SPEED -300.0f

#define COLOR(r,g,b,a) \
    ((unsigned int)(r) | ((unsigned int)(g) << 8) | \
    ((unsigned int)(b) << 16) | ((unsigned int)(a) << 24))

#define BLACK COLOR(8, 8, 12, 255)
#define WHITE COLOR(245, 245, 245, 255)
#define RED COLOR(220, 45, 50, 255)
#define GREEN COLOR(50, 220, 100, 255)
#define GRAY COLOR(75, 78, 88, 255)
#define DARK COLOR(22, 24, 32, 255)
#define PURPLE COLOR(140, 65, 220, 255)
#define GOLD COLOR(240, 190, 65, 255)

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y;
    float vx, vy;
    float w, h;
    int hp, maxhp;
    int facing;
    int grounded;
    int attacking;
    float attackTime;
    int invincible;
    float invincibleTimer;
    int magicCooldown;
    float magicTimer;
} Player;

typedef struct {
    float x, y;
    float vx;
    int hp;
    int alive;
    int facing;
    float anim;
    float attackCooldown;
} Zombie;

typedef struct {
    float x, y;
    float vx, vy;
    int active;
    float life;
} MagicBolt;

typedef struct {
    float x, y;
    float vx, vy;
    int hp, maxhp;
    int active;
    int state;
    int facing;
    float timer;
    float attackCooldown;
    float anim;
    int grounded;
} FinalBoss;

typedef struct {
    float x, width;
    int secret;
} Room;

static Player p;
static Zombie zombies[ZOMBIE_MAX];
static MagicBolt bolts[4];
static FinalBoss boss;
static Room rooms[ROOM_COUNT];

static float cameraX = 0.0f;
static float gameTime = 0.0f;
static int gameOver = 0;
static int victory = 0;
static int bossStarted = 0;
static int previousButtons = 0;
static int attackFlash = 0;

/* ---------------------------------------------------------
   DESENHO BASICO
   --------------------------------------------------------- */

static void drawRect(float x, float y, float w, float h,
                     unsigned int color)
{
    Vertex v[2];

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.0f;

    sceGuDrawArray(GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, 0, v);
}

static void drawLine(float x1, float y1, float x2, float y2,
                     unsigned int color)
{
    Vertex v[2];

    v[0].color = color;
    v[0].x = x1;
    v[0].y = y1;
    v[0].z = 0.0f;

    v[1].color = color;
    v[1].x = x2;
    v[1].y = y2;
    v[1].z = 0.0f;

    sceGuDrawArray(GU_LINES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, 0, v);
}

static void drawCircle(float cx, float cy, float radius,
                       unsigned int color)
{
    int i;
    const int segments = 16;

    for (i = 0; i < segments; i++) {
        float a1 = (float)i * 6.2831853f / segments;
        float a2 = (float)(i + 1) * 6.2831853f / segments;

        drawLine(cx + cosf(a1) * radius,
                 cy + sinf(a1) * radius,
                 cx + cosf(a2) * radius,
                 cy + sinf(a2) * radius,
                 color);
    }
}

static float clampf(float v, float min, float max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static int overlap(float ax, float ay, float aw, float ah,
                   float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

/* ---------------------------------------------------------
   PERSONAGEM ORGANICO
   --------------------------------------------------------- */

static void drawPlayer(void)
{
    float sx = p.x - cameraX;
    float sy = p.y;
    float dir = (float)p.facing;
    float step = sinf(gameTime * 12.0f) * 3.0f;
    unsigned int ink = p.invincible ? GOLD : WHITE;

    if (p.invincible && ((int)(gameTime * 18.0f) % 2))
        ink = RED;

    /* Corpo */
    drawLine(sx - 5, sy + 12, sx - 7, sy + 22, ink);
    drawLine(sx - 7, sy + 22, sx - 4, sy + 31, ink);
    drawLine(sx - 4, sy + 31, sx + 3, sy + 34, ink);
    drawLine(sx + 3, sy + 34, sx + 7, sy + 26, ink);
    drawLine(sx + 7, sy + 26, sx + 5, sy + 13, ink);

    /* Cabeca */
    drawCircle(sx, sy + 8, 7.0f, ink);
    drawLine(sx - 5, sy + 4, sx, sy + 1, ink);
    drawLine(sx, sy + 1, sx + 5, sy + 4, ink);

    /* Olho */
    drawLine(sx + dir * 2, sy + 7,
             sx + dir * 5, sy + 7, RED);

    /* Bracos */
    if (p.attacking) {
        drawLine(sx + dir * 2, sy + 15,
                 sx + dir * 11, sy + 12, ink);
        drawLine(sx + dir * 11, sy + 12,
                 sx + dir * 19, sy + 12, WHITE);

        drawLine(sx - dir * 3, sy + 15,
                 sx - dir * 8, sy + 22, ink);
    } else {
        drawLine(sx - 4, sy + 15,
                 sx - 10, sy + 20 + step, ink);
        drawLine(sx + 4, sy + 15,
                 sx + 10, sy + 20 - step, ink);
    }

    /* Pernas animadas */
    if (p.grounded) {
        drawLine(sx - 3, sy + 31,
                 sx - 6 - step, sy + 38, ink);
        drawLine(sx + 3, sy + 31,
                 sx + 7 + step, sy + 38, ink);
    } else {
        drawLine(sx - 3, sy + 31, sx - 8, sy + 35, ink);
        drawLine(sx + 3, sy + 31, sx + 8, sy + 35, ink);
    }

    /* Silhueta de energia da magia */
    if (p.magicTimer > 0.0f) {
        drawCircle(sx, sy + 18, 18.0f, PURPLE);
        drawCircle(sx, sy + 18, 22.0f, PURPLE);
    }
}

/* ---------------------------------------------------------
   LABIRINTO
   --------------------------------------------------------- */

static void initCave(void)
{
    int i;

    for (i = 0; i < ROOM_COUNT; i++) {
        rooms[i].x = 120.0f + i * 420.0f;
        rooms[i].width = 260.0f + (float)((i * 37) % 100);
        rooms[i].secret = (i % 3 == 1);
    }
}

static int caveFloorAt(float x)
{
    int i;

    /* A maior parte do mapa possui piso continuo. */
    for (i = 0; i < ROOM_COUNT; i++) {
        if (x >= rooms[i].x &&
            x <= rooms[i].x + rooms[i].width)
            return 1;
    }

    return 1;
}

static void drawCaveScene(void)
{
    int i;
    float sx;

    /* Fundo */
    drawRect(0, 0, SCREEN_W, SCREEN_H, DARK);

    /* Camadas distantes da caverna */
    for (i = 0; i < 9; i++) {
        sx = (float)(i * 110) - fmodf(cameraX * 0.18f, 110.0f);
        drawLine(sx, 0, sx + 55, 55, GRAY);
        drawLine(sx + 55, 55, sx + 90, 20, GRAY);
    }

    /* Teto irregular */
    for (i = 0; i < 15; i++) {
        float x = (float)(i * 45) -
                  fmodf(cameraX * 0.35f, 45.0f);
        float tip = 22.0f + (float)((i * 29) % 48);

        drawLine(x, 0, x + 20, tip, GRAY);
        drawLine(x + 20, tip, x + 35, 0, GRAY);
    }

    /* Plataformas e paredes de salas */
    for (i = 0; i < ROOM_COUNT; i++) {
        float rx = rooms[i].x - cameraX;
        float rw = rooms[i].width;

        if (rx > SCREEN_W || rx + rw < 0)
            continue;

        drawRect(rx, FLOOR_Y, rw, SCREEN_H - FLOOR_Y,
                 COLOR(43, 44, 54, 255));
        drawLine(rx, FLOOR_Y, rx + rw, FLOOR_Y, GRAY);

        /* Pedras na parede */
        drawLine(rx + 20, FLOOR_Y - 2,
                 rx + 45, FLOOR_Y - 15, GRAY);
        drawLine(rx + 45, FLOOR_Y - 15,
                 rx + 66, FLOOR_Y - 2, GRAY);

        /* Entrada de sala secreta */
        if (rooms[i].secret) {
            drawLine(rx + rw * 0.5f, FLOOR_Y - 4,
                     rx + rw * 0.5f, FLOOR_Y - 42, PURPLE);
            drawLine(rx + rw * 0.5f - 10, FLOOR_Y - 42,
                     rx + rw * 0.5f + 10, FLOOR_Y - 42,
                     PURPLE);
        }

        /* Cristais decorativos */
        drawLine(rx + rw * 0.25f, FLOOR_Y - 2,
                 rx + rw * 0.25f + 6, FLOOR_Y - 20, PURPLE);
        drawLine(rx + rw * 0.25f + 6, FLOOR_Y - 20,
                 rx + rw * 0.25f + 12, FLOOR_Y - 2, PURPLE);
    }

    /* Chao principal */
    drawRect(0, FLOOR_Y, SCREEN_W,
             SCREEN_H - FLOOR_Y, COLOR(36, 37, 47, 255));
    drawLine(0, FLOOR_Y, SCREEN_W, FLOOR_Y, WHITE);
}

/* ---------------------------------------------------------
   ZUMBIS
   --------------------------------------------------------- */

static void initZombies(void)
{
    int i;

    memset(zombies, 0, sizeof(zombies));

    for (i = 0; i < ZOMBIE_MAX; i++) {
        zombies[i].x = 360.0f + i * 420.0f;
        zombies[i].y = FLOOR_Y - 32.0f;
        zombies[i].hp = 3;
        zombies[i].alive = 1;
        zombies[i].facing = -1;
        zombies[i].anim = (float)i;
    }
}

static void updateZombies(float dt)
{
    int i;

    for (i = 0; i < ZOMBIE_MAX; i++) {
        Zombie *z = &zombies[i];
        float dx;

        if (!z->alive)
            continue;

        dx = p.x - z->x;
        z->anim += dt * 8.0f;

        if (fabsf(dx) < 300.0f) {
            z->facing = dx >= 0 ? 1 : -1;

            if (fabsf(dx) > 28.0f)
                z->x += z->facing * 48.0f * dt;
        }

        z->x = clampf(z->x, 0.0f, WORLD_W - 24.0f);

        if (z->attackCooldown > 0.0f)
            z->attackCooldown -= dt;

        if (overlap(p.x, p.y, p.w, p.h,
                    z->x, z->y, 22.0f, 32.0f) &&
            z->attackCooldown <= 0.0f &&
            p.invincibleTimer <= 0.0f) {
            p.hp -= 1;
            p.invincibleTimer = 0.8f;
            z->attackCooldown = 1.0f;
        }
    }
}

static void drawZombie(Zombie *z)
{
    float sx = z->x - cameraX;
    float sy = z->y;
    float walk = sinf(z->anim) * 3.0f;

    if (!z->alive || sx < -40 || sx > SCREEN_W + 40)
        return;

    /* Corpo irregular */
    drawLine(sx - 7, sy + 5, sx - 8, sy + 19, GREEN);
    drawLine(sx - 8, sy + 19, sx - 4, sy + 27, GREEN);
    drawLine(sx - 4, sy + 27, sx + 6, sy + 27, GREEN);
    drawLine(sx + 6, sy + 27, sx + 8, sy + 9, GREEN);

    drawCircle(sx, sy + 3, 8.0f, GREEN);
    drawLine(sx - 5, sy + 1, sx - 2, sy + 1, RED);
    drawLine(sx + 2, sy + 1, sx + 5, sy + 1, RED);

    drawLine(sx - 5, sy + 11,
             sx - 13, sy + 17 + walk, GREEN);
    drawLine(sx + 5, sy + 11,
             sx + 13, sy + 17 - walk, GREEN);

    drawLine(sx - 3, sy + 27,
             sx - 6 - walk, sy + 32, GREEN);
    drawLine(sx + 3, sy + 27,
             sx + 6 + walk, sy + 32, GREEN);

    drawRect(sx - 10, sy - 10, 20, 2, BLACK);
    drawRect(sx - 10, sy - 10,
             20.0f * z->hp / 3.0f, 2, RED);
}

static void drawZombies(void)
{
    int i;

    for (i = 0; i < ZOMBIE_MAX; i++)
        drawZombie(&zombies[i]);
}

/* ---------------------------------------------------------
   MAGIA
   --------------------------------------------------------- */

static void castMagic(void)
{
    int i;

    if (p.magicCooldown > 0)
        return;

    for (i = 0; i < 4; i++) {
        if (!bolts[i].active) {
            bolts[i].active = 1;
            bolts[i].x = p.x + p.facing * 16.0f;
            bolts[i].y = p.y + 17.0f;
            bolts[i].vx = p.facing * 280.0f;
            bolts[i].vy = 0.0f;
            bolts[i].life = 1.5f;
            p.magicCooldown = 1;
            p.magicTimer = 0.25f;
            return;
        }
    }
}

static void updateMagic(float dt)
{
    int i, j;

    for (i = 0; i < 4; i++) {
        MagicBolt *b = &bolts[i];

        if (!b->active)
            continue;

        b->x += b->vx * dt;
        b->y += b->vy * dt;
        b->life -= dt;

        if (b->life <= 0.0f ||
            b->x < 0.0f || b->x > WORLD_W) {
            b->active = 0;
            continue;
        }

        for (j = 0; j < ZOMBIE_MAX; j++) {
            Zombie *z = &zombies[j];

            if (z->alive &&
                overlap(b->x - 5, b->y - 5, 10, 10,
                        z->x - 10, z->y, 22, 32)) {
                z->hp -= 2;
                b->active = 0;

                if (z->hp <= 0)
                    z->alive = 0;

                break;
            }
        }

        if (b->active && bossStarted && boss.active &&
            overlap(b->x - 5, b->y - 5, 10, 10,
                    boss.x - 24, boss.y - 65, 48, 65)) {
            boss.hp -= 20;
            b->active = 0;

            if (boss.hp <= 0) {
                boss.hp = 0;
                boss.active = 0;
                victory = 1;
            }
        }
    }
}

static void drawMagic(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (bolts[i].active) {
            float sx = bolts[i].x - cameraX;
            drawCircle(sx, bolts[i].y, 7.0f, PURPLE);
            drawLine(sx - 10, bolts[i].y,
                     sx + 10, bolts[i].y, WHITE);
        }
    }
}

/* ---------------------------------------------------------
   BOSS FINAL
   --------------------------------------------------------- */

#define BOSS_IDLE 0
#define BOSS_CHASE 1
#define BOSS_DASH 2
#define BOSS_JUMP 3

static void initBoss(void)
{
    memset(&boss, 0, sizeof(boss));

    boss.x = 4700.0f;
    boss.y = FLOOR_Y - 65.0f;
    boss.hp = 500;
    boss.maxhp = 500;
    boss.active = 1;
    boss.state = BOSS_IDLE;
    boss.facing = -1;
    boss.grounded = 1;
    boss.attackCooldown = 1.5f;
}

static void updateBoss(float dt)
{
    float dx;

    if (!boss.active || !bossStarted)
        return;

    dx = p.x - boss.x;
    boss.anim += dt * 5.0f;
    boss.timer += dt;

    if (boss.attackCooldown > 0.0f)
        boss.attackCooldown -= dt;

    boss.facing = dx >= 0 ? 1 : -1;

    if (boss.state == BOSS_IDLE) {
        if (fabsf(dx) < 520.0f)
            boss.state = BOSS_CHASE;
    }
    else if (boss.state == BOSS_CHASE) {
        if (fabsf(dx) > 80.0f)
            boss.x += boss.facing * 65.0f * dt;

        if (boss.attackCooldown <= 0.0f) {
            boss.state = BOSS_DASH;
            boss.timer = 0.0f;
            boss.attackCooldown = 2.0f;
        }
    }
    else if (boss.state == BOSS_DASH) {
        boss.x += boss.facing * 230.0f * dt;

        if (boss.timer > 0.55f) {
            boss.state = BOSS_CHASE;
            boss.timer = 0.0f;
        }
    }

    /* Gravidade e pulo do boss */
    if (!boss.grounded) {
        boss.vy += GRAVITY * dt;
        boss.y += boss.vy * dt;

        if (boss.y >= FLOOR_Y - 65.0f) {
            boss.y = FLOOR_Y - 65.0f;
            boss.vy = 0.0f;
            boss.grounded = 1;
        }
    }

    boss.x = clampf(boss.x, 4300.0f, 5100.0f);

    /* Contato do boss causa dano */
    if (overlap(p.x, p.y, p.w, p.h,
                boss.x - 22, boss.y - 60, 44, 60) &&
        p.invincibleTimer <= 0.0f) {
        p.hp -= 2;
        p.invincibleTimer = 1.0f;
    }
}

static void damageBoss(int damage)
{
    if (!boss.active)
        return;

    boss.hp -= damage;

    if (boss.hp <= 0) {
        boss.hp = 0;
        boss.active = 0;
        victory = 1;
    }
}

static void drawBoss(void)
{
    float sx, sy, walk;

    if (!boss.active)
        return;

    sx = boss.x - cameraX;
    sy = boss.y;
    walk = sinf(boss.anim) * 5.0f;

    if (sx < -80 || sx > SCREEN_W + 80)
        return;

    /* Silhueta grande, desenhada com linhas */
    drawLine(sx - 22, sy + 8, sx - 26, sy + 36, RED);
    drawLine(sx - 26, sy + 36, sx - 18, sy + 55, RED);
    drawLine(sx - 18, sy + 55, sx + 18, sy + 55, RED);
    drawLine(sx + 18, sy + 55, sx + 26, sy + 36, RED);
    drawLine(sx + 26, sy + 36, sx + 22, sy + 8, RED);

    drawCircle(sx, sy + 2, 18.0f, RED);

    /* Chifres */
    drawLine(sx - 12, sy - 8, sx - 25, sy - 24, RED);
    drawLine(sx - 25, sy - 24, sx - 20, sy - 7, RED);
    drawLine(sx + 12, sy - 8, sx + 25, sy - 24, RED);
    drawLine(sx + 25, sy - 24, sx + 20, sy - 7, RED);

    /* Olhos */
    drawLine(sx - 10, sy, sx - 3, sy, GOLD);
    drawLine(sx + 3, sy, sx + 10, sy, GOLD);

    /* Bracos */
    drawLine(sx - 18, sy + 15,
             sx - 38, sy + 30 + walk, RED);
    drawLine(sx - 38, sy + 30 + walk,
             sx - 42, sy + 42 + walk, RED);

    drawLine(sx + 18, sy + 15,
             sx + 38, sy + 30 - walk, RED);
    drawLine(sx + 38, sy + 30 - walk,
             sx + 42, sy + 42 - walk, RED);

    /* Pernas */
    drawLine(sx - 12, sy + 53,
             sx - 17 - walk, sy + 65, RED);
    drawLine(sx + 12, sy + 53,
             sx + 17 + walk, sy + 65, RED);

    /* Barra de vida */
    drawRect(100, 18, 280, 10, BLACK);
    drawRect(100, 18,
             280.0f * boss.hp / boss.maxhp,
             10, RED);
    drawRect(100, 18, 280, 1, WHITE);
}

/* ---------------------------------------------------------
   JOGADOR, ATAQUE E COLISOES
   --------------------------------------------------------- */

static void initPlayer(void)
{
    memset(&p, 0, sizeof(p));

    p.x = 60.0f;
    p.y = FLOOR_Y - PLAYER_H;
    p.w = PLAYER_W;
    p.h = PLAYER_H;
    p.hp = 10;
    p.maxhp = 10;
    p.facing = 1;
    p.grounded = 1;
}

static void playerAttack(void)
{
    int i;
    float ax;

    p.attacking = 1;
    p.attackTime = 0.18f;
    attackFlash = 1;

    ax = p.x + (p.facing > 0 ? p.w : -28.0f);

    for (i = 0; i < ZOMBIE_MAX; i++) {
        Zombie *z = &zombies[i];

        if (z->alive &&
            overlap(ax, p.y + 5, 28, 28,
                    z->x - 10, z->y, 22, 32)) {
            z->hp -= 1;

            if (z->hp <= 0)
                z->alive = 0;
        }
    }

    if (bossStarted && boss.active &&
        overlap(ax, p.y + 5, 28, 28,
                boss.x - 24, boss.y - 60, 48, 60)) {
        damageBoss(15);
    }
}

static void updatePlayer(float dt, unsigned int buttons)
{
    float oldY = p.y;

    p.vx = 0.0f;

    if (buttons & PSP_CTRL_LEFT) {
        p.vx = -MOVE_SPEED;
        p.facing = -1;
    }

    if (buttons & PSP_CTRL_RIGHT) {
        p.vx = MOVE_SPEED;
        p.facing = 1;
    }

    p.x += p.vx * dt;
    p.x = clampf(p.x, 0.0f, WORLD_W - p.w);

    if ((buttons & PSP_CTRL_CROSS) && p.grounded) {
        p.vy = JUMP_SPEED;
        p.grounded = 0;
    }

    p.vy += GRAVITY * dt;
    p.y += p.vy * dt;

    if (p.y + p.h >= FLOOR_Y && caveFloorAt(p.x)) {
        p.y = FLOOR_Y - p.h;
        p.vy = 0.0f;
        p.grounded = 1;
    } else {
        p.grounded = 0;
    }

    /* Evita ficar preso acima do limite superior. */
    if (p.y < 0.0f) {
        p.y = 0.0f;
        p.vy = 0.0f;
    }

    (void)oldY;

    if (p.attackTime > 0.0f) {
        p.attackTime -= dt;

        if (p.attackTime <= 0.0f)
            p.attacking = 0;
    }

    if (p.invincibleTimer > 0.0f)
        p.invincibleTimer -= dt;

    p.invincible = p.invincibleTimer > 0.0f;

    if (p.magicCooldown > 0)
        p.magicCooldown = 0;

    if (p.magicTimer > 0.0f)
        p.magicTimer -= dt;

    if (p.x > 4250.0f)
        bossStarted = 1;
}

/* ---------------------------------------------------------
   INTERFACE
   --------------------------------------------------------- */

static void drawHUD(void)
{
    int i;

    drawRect(8, 8, 84, 12, BLACK);
    drawRect(10, 10, 80.0f * p.hp / p.maxhp,
             8, GREEN);

    for (i = 0; i < 3; i++)
        drawCircle(16.0f + i * 13.0f, 34.0f,
                   4.0f, i == 0 ? PURPLE : GRAY);

    /* Marcador da posicao no mundo */
    drawRect(8, 48, 100, 3, GRAY);
    drawRect(8 + 100.0f * p.x / WORLD_W,
             46, 4, 7, WHITE);

    if (gameOver) {
        drawRect(100, 100, 280, 60, BLACK);
        drawLine(100, 100, 380, 100, RED);
        drawLine(100, 160, 380, 160, RED);
    }

    if (victory) {
        drawRect(100, 100, 280, 60, BLACK);
        drawLine(100, 100, 380, 100, GOLD);
        drawLine(100, 160, 380, 160, GOLD);
    }
}

/* ---------------------------------------------------------
   INICIALIZACAO GRAFICA PSP
   --------------------------------------------------------- */

static unsigned int __attribute__((aligned(16))) list[262144];

static void initGraphics(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(GU_PSM_8888,
                    (void *)0, 512);
    sceGuDispBuffer(SCREEN_W, SCREEN_H,
                    (void *)(512 * 272 * 4), 512);
    sceGuDepthBuffer((void *)(512 * 272 * 8), 512);

    sceGuOffset(2048 - SCREEN_W / 2,
                2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);

    sceGuDepthRange(0xc350, 0x2710);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void renderFrame(void)
{
    int i;

    sceGuStart(GU_DIRECT, list);

    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    sceGuDisable(GU_TEXTURE_2D);

    drawCaveScene();
    drawZombies();
    drawMagic();

    if (bossStarted)
        drawBoss();

    drawPlayer();
    drawHUD();

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();

    (void)i;
}

/* ---------------------------------------------------------
   REINICIO E LOOP PRINCIPAL
   --------------------------------------------------------- */

static void resetGame(void)
{
    cameraX = 0.0f;
    gameTime = 0.0f;
    gameOver = 0;
    victory = 0;
    bossStarted = 0;
    attackFlash = 0;

    memset(bolts, 0, sizeof(bolts));

    initPlayer();
    initCave();
    initZombies();
    initBoss();
}

int main(void)
{
    SceCtrlData pad;
    unsigned int buttons;
    unsigned int pressed;
    unsigned long long lastTick;
    unsigned long long nowTick;
    float dt;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    pspDebugScreenInit();

    initGraphics();
    resetGame();

    lastTick = sceKernelGetSystemTimeWide();

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        buttons = pad.Buttons;
        pressed = buttons & ~previousButtons;
        previousButtons = buttons;

        if (pressed & PSP_CTRL_START)
            resetGame();

        nowTick = sceKernelGetSystemTimeWide();
        dt = (float)(nowTick - lastTick) / 1000000.0f;
        lastTick = nowTick;

        /* Evita saltos grandes si o jogo travar por um instante. */
        dt = clampf(dt, 0.0f, 0.04f);
        gameTime += dt;

        if (!gameOver && !victory) {
            updatePlayer(dt, buttons);
            updateZombies(dt);
            updateMagic(dt);

            if (pressed & PSP_CTRL_SQUARE)
                playerAttack();

            if (pressed & PSP_CTRL_CIRCLE)
                castMagic();

            if (p.hp <= 0) {
                p.hp = 0;
                gameOver = 1;
            }

            /* Camera acompanha o personagem */
            cameraX = p.x - SCREEN_W * 0.42f;
            cameraX = clampf(cameraX, 0.0f,
                             WORLD_W - SCREEN_W);

            if (bossStarted)
                updateBoss(dt);
        }

        renderFrame();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
