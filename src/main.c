/*
 * RANDOM ALPHA
 * PSP / PSPSDK
 *
 * Código-base em C:
 * - 6 zonas
 * - Movimento e pulo
 * - Câmera horizontal
 * - Plataformas e colisões
 * - Inimigos e chefes
 * - Ataque físico e projétil mágico
 * - Magia desbloqueada na caverna
 * - Gráficos desenhados em C
 *
 * Controles:
 * Analógico/D-pad: mover
 * X: pular
 * Quadrado: ataque físico
 * Círculo: magia
 * Esquerda/Direita: explorar mapa
 * START: sair
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspctrl.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

PSP_MODULE_INFO("RANDOM ALPHA", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define BUF_W 512
#define WORLD_W 1600
#define WORLD_H 600
#define GROUND 500

#define MAX_PLATFORMS 40
#define MAX_MOBS 12
#define MAX_SHOTS 16
#define ZONE_COUNT 6

#define PLAYER_W 20
#define PLAYER_H 30

#define GRAVITY 0.42f
#define JUMP_FORCE -8.5f
#define PLAYER_SPEED 2.6f

#define COL_WHITE  0xFFFFFFFF
#define COL_BLACK  0xFF101018
#define COL_RED    0xFF3030E8
#define COL_BLUE   0xFFE08030
#define COL_GREEN  0xFF50C050
#define COL_PURPLE 0xFFB050D0
#define COL_YELLOW 0xFF30D8FF

typedef struct {
    float x, y, w, h;
} Platform;

typedef struct {
    float x, y, vx, vy;
    int alive;
    int magic;
    int life;
} Projectile;

typedef struct {
    float x, y, vx, vy;
    float w, h;
    int hp, maxHp;
    int alive;
    int boss;
    int facing;
    int grounded;
    int cooldown;
} Fighter;

typedef struct {
    unsigned int color;
    float x, y;
} Vertex;

static unsigned int __attribute__((aligned(16)))
displayList[262144];

static Platform platforms[MAX_PLATFORMS];
static int platformCount = 0;

static Projectile shots[MAX_SHOTS];
static Fighter mobs[MAX_MOBS];
static Fighter player;

static int currentZone = 0;
static int magicUnlocked = 0;
static int bossDefeated[ZONE_COUNT] = {0};
static int gameWon = 0;

static float cameraX = 0.0f;
static int facing = 1;
static int attackCooldown = 0;
static int magicCooldown = 0;

static unsigned int zoneSky[ZONE_COUNT] = {
    0xFF24150B, /* Caverna */
    0xFF244020, /* Floresta */
    0xFF303038, /* Cidade */
    0xFF34243C, /* Castelo */
    0xFF282040, /* Torre */
    0xFF181020  /* Arena */
};

static void drawRect(float x, float y, float w, float h,
                     unsigned int color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 2);

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v
    );
}

static void drawTriangle(float x1, float y1,
                         float x2, float y2,
                         float x3, float y3,
                         unsigned int color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 3);

    v[0].color = color;
    v[0].x = x1;
    v[0].y = y1;

    v[1].color = color;
    v[1].x = x2;
    v[1].y = y2;

    v[2].color = color;
    v[2].x = x3;
    v[2].y = y3;

    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        3, 0, v
    );
}

static void drawLine(float x1, float y1,
                     float x2, float y2,
                     unsigned int color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 2);

    v[0].color = color;
    v[0].x = x1;
    v[0].y = y1;

    v[1].color = color;
    v[1].x = x2;
    v[1].y = y2;

    sceGuDrawArray(
        GU_LINES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v
    );
}

static int overlap(float ax, float ay, float aw, float ah,
                   float bx, float by, float bw, float bh)
{
    return ax < bx + bw &&
           ax + aw > bx &&
           ay < by + bh &&
           ay + ah > by;
}

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

    /* Piso principal */
    addPlatform(0, GROUND, WORLD_W, 100);

    switch (currentZone) {
        case 0: /* Caverna do Labirinto */
            addPlatform(100, 430, 100, 20);
            addPlatform(240, 370, 110, 20);
            addPlatform(390, 420, 90, 20);
            addPlatform(520, 340, 120, 20);
            addPlatform(690, 390, 100, 20);
            addPlatform(830, 320, 120, 20);
            addPlatform(1010, 400, 100, 20);
            addPlatform(1170, 350, 120, 20);
            addPlatform(1370, 410, 100, 20);
            break;

        case 1: /* Floresta Esquecida */
            addPlatform(100, 420, 120, 20);
            addPlatform(280, 360, 100, 20);
            addPlatform(450, 400, 100, 20);
            addPlatform(610, 330, 130, 20);
            addPlatform(820, 390, 100, 20);
            addPlatform(1000, 340, 120, 20);
            addPlatform(1220, 400, 120, 20);
            break;

        case 2: /* Cidade Abandonada */
            addPlatform(120, 420, 150, 20);
            addPlatform(330, 350, 120, 20);
            addPlatform(500, 290, 100, 20);
            addPlatform(680, 380, 140, 20);
            addPlatform(900, 320, 120, 20);
            addPlatform(1120, 370, 150, 20);
            addPlatform(1350, 300, 100, 20);
            break;

        case 3: /* Castelo Caído */
            addPlatform(100, 420, 100, 20);
            addPlatform(260, 360, 120, 20);
            addPlatform(450, 310, 120, 20);
            addPlatform(640, 390, 100, 20);
            addPlatform(800, 330, 140, 20);
            addPlatform(1030, 390, 120, 20);
            addPlatform(1250, 330, 120, 20);
            break;

        case 4: /* Torre Antiga */
            addPlatform(100, 420, 120, 20);
            addPlatform(300, 360, 100, 20);
            addPlatform(460, 300, 120, 20);
            addPlatform(650, 350, 100, 20);
            addPlatform(810, 280, 120, 20);
            addPlatform(1010, 350, 100, 20);
            addPlatform(1200, 290, 120, 20);
            break;

        case 5: /* Arena Final */
            addPlatform(180, 420, 120, 20);
            addPlatform(430, 370, 120, 20);
            addPlatform(700, 420, 120, 20);
            addPlatform(960, 370, 120, 20);
            addPlatform(1250, 420, 120, 20);
            break;
    }
}

static void clearMobs(void)
{
    int i;

    for (i = 0; i < MAX_MOBS; i++) {
        memset(&mobs[i], 0, sizeof(Fighter));
    }
}

static void spawnMob(float x, float y, int boss)
{
    int i;

    for (i = 0; i < MAX_MOBS; i++) {
        if (!mobs[i].alive) {
            mobs[i].x = x;
            mobs[i].y = y;
            mobs[i].w = boss ? 44 : 24;
            mobs[i].h = boss ? 48 : 28;
            mobs[i].hp = boss ? 12 : 3;
            mobs[i].maxHp = mobs[i].hp;
            mobs[i].alive = 1;
            mobs[i].boss = boss;
            mobs[i].facing = -1;
            mobs[i].vx = 0;
            mobs[i].vy = 0;
            mobs[i].grounded = 0;
            mobs[i].cooldown = 0;
            return;
        }
    }
}

static void spawnZoneEnemies(void)
{
    clearMobs();

    if (currentZone == 0) {
        spawnMob(350, GROUND - 28, 0);
        spawnMob(720, GROUND - 28, 0);
        spawnMob(1050, GROUND - 28, 0);
    } else if (currentZone == 1) {
        spawnMob(280, GROUND - 28, 0);
        spawnMob(650, GROUND - 28, 0);
        spawnMob(1100, GROUND - 28, 0);
        if (!bossDefeated[1])
            spawnMob(1330, GROUND - 48, 1);
    } else if (currentZone == 2) {
        spawnMob(300, GROUND - 28, 0);
        spawnMob(650, GROUND - 28, 0);
        spawnMob(1050, GROUND - 28, 0);
    } else if (currentZone == 3) {
        spawnMob(250, GROUND - 28, 0);
        spawnMob(600, GROUND - 28, 0);
        if (!bossDefeated[3])
            spawnMob(1250, GROUND - 48, 1);
    } else if (currentZone == 4) {
        spawnMob(350, GROUND - 28, 0);
        spawnMob(800, GROUND - 28, 0);
        if (!bossDefeated[4])
            spawnMob(1300, GROUND - 48, 1);
    } else if (currentZone == 5) {
        spawnMob(400, GROUND - 28, 0);
        if (!bossDefeated[5])
            spawnMob(1250, GROUND - 48, 1);
    }
}

static void enterZone(int zone)
{
    if (zone < 0)
        zone = 0;

    if (zone >= ZONE_COUNT)
        zone = ZONE_COUNT - 1;

    currentZone = zone;

    player.x = 40;
    player.y = GROUND - PLAYER_H;
    player.vx = 0;
    player.vy = 0;
    player.grounded = 1;

    cameraX = 0;

    buildPlatforms();
    spawnZoneEnemies();
}

static void initPlayer(void)
{
    memset(&player, 0, sizeof(player));

    player.x = 40;
    player.y = GROUND - PLAYER_H;
    player.w = PLAYER_W;
    player.h = PLAYER_H;
    player.hp = 10;
    player.maxHp = 10;
    player.alive = 1;
    player.grounded = 1;
    player.facing = 1;

    memset(shots, 0, sizeof(shots));
}

static void drawBackground(void)
{
    int i;

    drawRect(0, 0, SW, SH, zoneSky[currentZone]);

    if (currentZone == 0) {
        /* Formações rochosas */
        for (i = 0; i < 9; i++) {
            float x = (i * 190) - cameraX * 0.25f;
            drawTriangle(x, 0, x + 90, 0, x + 45, 75,
                         0xFF30251B);
        }

        drawRect(0, 190, SW, 82, 0xFF19120D);
    } else if (currentZone == 1) {
        for (i = 0; i < 8; i++) {
            float x = (i * 130) - cameraX * 0.20f;
            drawRect(x, 75, 18, 150, 0xFF18351C);
            drawTriangle(x - 45, 110, x + 60, 110,
                         x + 8, 25, 0xFF24552B);
            drawTriangle(x - 40, 150, x + 55, 150,
                         x + 8, 70, 0xFF204823);
        }
    } else if (currentZone == 2) {
        for (i = 0; i < 7; i++) {
            float x = (i * 100) - cameraX * 0.22f;
            drawRect(x, 70 + (i % 3) * 25,
                     65, 170, 0xFF25252D);
            drawRect(x + 10, 95, 10, 15, 0xFF606050);
            drawRect(x + 35, 125, 10, 15, 0xFF606050);
        }
    } else if (currentZone == 3) {
        for (i = 0; i < 6; i++) {
            float x = (i * 120) - cameraX * 0.22f;
            drawRect(x, 100, 70, 125, 0xFF34263D);
            drawTriangle(x - 10, 100, x + 80, 100,
                         x + 35, 35, 0xFF49334D);
        }
    } else if (currentZone == 4) {
        for (i = 0; i < 6; i++) {
            float x = (i * 100) - cameraX * 0.20f;
            drawRect(x, 50, 18, 180, 0xFF44364D);
            drawTriangle(x - 12, 55, x + 30, 55,
                         x + 9, 20, 0xFF675079);
        }
    } else {
        drawRect(0, 220, SW, 52, 0xFF100A18);

        for (i = 0; i < 8; i++) {
            float x = i * 70;
            drawLine(x, 0, x + 35, SH, 0xFF453050);
            drawLine(x + 35, 0, x, SH, 0xFF302040);
        }
    }
}

static void drawPlatforms(void)
{
    int i;

    for (i = 0; i < platformCount; i++) {
        float x = platforms[i].x - cameraX;

        if (x + platforms[i].w < 0 || x > SW)
            continue;

        drawRect(x, platforms[i].y,
                 platforms[i].w, platforms[i].h,
                 currentZone == 0 ? 0xFF51443A : 0xFF49434D);

        drawRect(x, platforms[i].y,
                 platforms[i].w, 4,
                 0xFF858078);
    }
}

static void drawPlayer(void)
{
    float x = player.x - cameraX;
    float y = player.y;

    /* Corpo */
    drawRect(x + 3, y + 9, 14, 17, 0xFF22222C);

    /* Cabeça */
    drawRect(x + 4, y, 12, 11, 0xFF303038);

    /* Olhos */
    drawRect(x + (facing > 0 ? 12 : 5), y + 4,
             3, 3, COL_RED);

    /* Pernas */
    drawRect(x + 3, y + 25, 5, 5, 0xFF101018);
    drawRect(x + 12, y + 25, 5, 5, 0xFF101018);

    /* Arma */
    if (attackCooldown > 0) {
        drawRect(x + (facing > 0 ? 17 : -10),
                 y + 12, 13, 4, COL_WHITE);
    }
}

static void drawMob(Fighter *m)
{
    float x = m->x - cameraX;
    float y = m->y;

    if (x + m->w < 0 || x > SW)
        return;

    if (m->boss) {
        drawRect(x, y, m->w, m->h, 0xFF302040);
        drawRect(x + 5, y + 8, m->w - 10, 12, COL_PURPLE);
        drawRect(x + 7, y + 10, 6, 5, COL_RED);
        drawRect(x + m->w - 13, y + 10, 6, 5, COL_RED);

        /* Barra de vida do chefe */
        drawRect(x, y - 10, m->w, 5, 0xFF303030);
        drawRect(x, y - 10,
                 m->w * (float)m->hp / m->maxHp,
                 5, COL_RED);
    } else {
        drawRect(x, y, m->w, m->h, 0xFF285040);
        drawRect(x + 3, y + 5, m->w - 6, 8, COL_GREEN);
        drawRect(x + 4, y + 7, 4, 3, COL_RED);
        drawRect(x + m->w - 8, y + 7, 4, 3, COL_RED);
    }
}

static void drawProjectiles(void)
{
    int i;

    for (i = 0; i < MAX_SHOTS; i++) {
        if (!shots[i].alive)
            continue;

        if (shots[i].magic) {
            drawRect(shots[i].x - cameraX,
                     shots[i].y, 12, 12, COL_PURPLE);
            drawTriangle(shots[i].x - cameraX - 5,
                         shots[i].y + 6,
                         shots[i].x - cameraX + 6,
                         shots[i].y - 4,
                         shots[i].x - cameraX + 16,
                         shots[i].y + 6,
                         COL_BLUE);
        } else {
            drawRect(shots[i].x - cameraX,
                     shots[i].y, 8, 5, COL_YELLOW);
        }
    }
}

static void drawHUD(void)
{
    int i;

    /* Vida */
    drawRect(10, 10, 104, 12, 0xFF202028);
    drawRect(12, 12,
             100.0f * player.hp / player.maxHp,
             8, COL_RED);

    /* Indicador de magia */
    drawRect(10, 28, 80, 5, 0xFF202028);

    if (magicUnlocked)
        drawRect(10, 28, 80, 5, COL_PURPLE);

    /* Indicador simples da zona */
    for (i = 0; i < ZONE_COUNT; i++) {
        drawRect(350 + i * 18, 12, 12, 8,
                 i == currentZone ? COL_YELLOW : 0xFF555560);
    }

    if (gameWon) {
        drawRect(120, 100, 240, 45, 0xFF202030);
        drawRect(150, 115, 180, 8, COL_YELLOW);
        drawRect(170, 130, 140, 5, COL_GREEN);
    }
}

static void fireProjectile(int magic)
{
    int i;

    for (i = 0; i < MAX_SHOTS; i++) {
        if (!shots[i].alive) {
            shots[i].alive = 1;
            shots[i].magic = magic;
            shots[i].x = player.x +
                         (facing > 0 ? player.w : -8);
            shots[i].y = player.y + 12;
            shots[i].vx = facing * (magic ? 5.0f : 4.0f);
            shots[i].vy = 0;
            shots[i].life = magic ? 90 : 25;
            return;
        }
    }
}

static void updateProjectiles(void)
{
    int i, j;

    for (i = 0; i < MAX_SHOTS; i++) {
        Projectile *p = &shots[i];

        if (!p->alive)
            continue;

        p->x += p->vx;
        p->y += p->vy;
        p->life--;

        if (p->life <= 0 ||
            p->x < 0 || p->x > WORLD_W) {
            p->alive = 0;
            continue;
        }

        for (j = 0; j < MAX_MOBS; j++) {
            Fighter *m = &mobs[j];

            if (!m->alive)
                continue;

            if (overlap(p->x, p->y, 12, 12,
                        m->x, m->y, m->w, m->h)) {
                m->hp -= p->magic ? 3 : 1;
                p->alive = 0;

                if (m->hp <= 0) {
                    m->alive = 0;

                    if (m->boss) {
                        bossDefeated[currentZone] = 1;

                        if (currentZone == 5)
                            gameWon = 1;
                    }
                }

                break;
            }
        }
    }
}

static void updatePlayerPhysics(void)
{
    int i;
    float oldY = player.y;

    player.vy += GRAVITY;

    if (player.vy > 9.0f)
        player.vy = 9.0f;

    player.x += player.vx;
    player.y += player.vy;

    if (player.x < 0)
        player.x = 0;

    if (player.x > WORLD_W - player.w)
        player.x = WORLD_W - player.w;

    player.grounded = 0;

    if (player.vy >= 0) {
        for (i = 0; i < platformCount; i++) {
            Platform *p = &platforms[i];

            if (overlap(player.x, oldY,
                        player.w, player.h,
                        p->x, p->y, p->w, p->h) &&
                oldY + player.h <= p->y + 8 &&
                player.y + player.h >= p->y) {

                player.y = p->y - player.h;
                player.vy = 0;
                player.grounded = 1;
                break;
            }
        }
    }

    if (player.y + player.h > GROUND) {
        player.y = GROUND - player.h;
        player.vy = 0;
        player.grounded = 1;
    }

    if (player.y > WORLD_H) {
        player.hp--;

        if (player.hp <= 0)
            player.hp = player.maxHp;

        player.x = 40;
        player.y = GROUND - player.h;
        player.vy = 0;
    }

    cameraX = player.x - SW / 2;

    if (cameraX < 0)
        cameraX = 0;

    if (cameraX > WORLD_W - SW)
        cameraX = WORLD_W - SW;
}

static void updateMobs(void)
{
    int i;

    for (i = 0; i < MAX_MOBS; i++) {
        Fighter *m = &mobs[i];

        if (!m->alive)
            continue;

        if (m->cooldown > 0)
            m->cooldown--;

        if (player.x < m->x)
            m->facing = -1;
        else
            m->facing = 1;

        if (fabsf(player.x - m->x) < 260) {
            float speed = m->boss ? 0.8f : 0.55f;

            if (player.x < m->x)
                m->x -= speed;
            else
                m->x += speed;
        }

        if (m->x < 0)
            m->x = 0;

        if (m->x > WORLD_W - m->w)
            m->x = WORLD_W - m->w;

        if (overlap(player.x, player.y,
                    player.w, player.h,
                    m->x, m->y, m->w, m->h)) {

            if (player.hp > 0 && m->cooldown == 0) {
                player.hp--;
                m->cooldown = 50;

                if (player.hp <= 0) {
                    player.hp = player.maxHp;
                    player.x = 40;
                    player.y = GROUND - player.h;
                    player.vy = 0;
                }
            }
        }
    }
}

static void updateGame(SceCtrlData *pad)
{
    player.vx = 0;

    if (pad->Buttons & PSP_CTRL_LEFT) {
        player.vx = -PLAYER_SPEED;
        facing = -1;
    }

    if (pad->Buttons & PSP_CTRL_RIGHT) {
        player.vx = PLAYER_SPEED;
        facing = 1;
    }

    if (pad->Buttons & PSP_CTRL_CROSS) {
        if (player.grounded) {
            player.vy = JUMP_FORCE;
            player.grounded = 0;
        }
    }

    if (attackCooldown > 0)
        attackCooldown--;

    if (magicCooldown > 0)
        magicCooldown--;

    if ((pad->Buttons & PSP_CTRL_SQUARE) &&
        attackCooldown == 0) {
        fireProjectile(0);
        attackCooldown = 18;
    }

    if ((pad->Buttons & PSP_CTRL_CIRCLE) &&
        magicUnlocked &&
        magicCooldown == 0) {
        fireProjectile(1);
        magicCooldown = 35;
    }

    /*
     * A magia é desbloqueada na caverna inicial.
     * Basta alcançar a região profunda da caverna.
     */
    if (currentZone == 0 && player.x > 1350)
        magicUnlocked = 1;

    /*
     * L1/R1 mudam de zona para permitir testar o mapa.
     */
    if (pad->Buttons & PSP_CTRL_LTRIGGER) {
        enterZone(currentZone - 1);
    } else if (pad->Buttons & PSP_CTRL_RTRIGGER) {
        enterZone(currentZone + 1);
    }

    updatePlayerPhysics();
    updateMobs();
    updateProjectiles();
}

int main(void)
{
    SceCtrlData pad;
    unsigned int oldButtons = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();

    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SW, SH, (void *)0x88000, BUF_W);
    sceGuDepthBuffer((void *)0x110000, BUF_W);

    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);

    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SW, SH);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    initPlayer();
    enterZone(0);

    while (1) {
        sceCtrlPeekBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_START)
            break;

        /*
         * Impede a troca contínua de zona enquanto
         * o botão permanece pressionado.
         */
        if ((pad.Buttons & PSP_CTRL_LTRIGGER) &&
            !(oldButtons & PSP_CTRL_LTRIGGER)) {
            enterZone(currentZone - 1);
        } else if ((pad.Buttons & PSP_CTRL_RTRIGGER) &&
                   !(oldButtons & PSP_CTRL_RTRIGGER)) {
            enterZone(currentZone + 1);
        }

        oldButtons = pad.Buttons;

        /*
         * Evita tratar L/R duas vezes no updateGame.
         */
        pad.Buttons &= ~(PSP_CTRL_LTRIGGER |
                         PSP_CTRL_RTRIGGER);

        if (!gameWon)
            updateGame(&pad);

        sceGuStart(GU_DIRECT, displayList);

        sceGuClearColor(COL_BLACK);
        sceGuClear(GU_COLOR_BUFFER_BIT |
                   GU_DEPTH_BUFFER_BIT);

        drawBackground();
        drawPlatforms();

        {
            int i;

            for (i = 0; i < MAX_MOBS; i++) {
                if (mobs[i].alive)
                    drawMob(&mobs[i]);
            }
        }

        drawProjectiles();
        drawPlayer();
        drawHUD();

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuDisplay(GU_FALSE);
    sceGuTerm();

    sceKernelExitGame();
    return 0;
}
