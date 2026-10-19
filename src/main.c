
/*
 * RANDOM
 * Jogo homebrew para PSP - PSPSDK
 *
 * ZONAS:
 * 0 - Caverna do Labirinto
 * 1 - Floresta Esquecida
 *
 * RECURSOS:
 * - Duas zonas grandes
 * - Camera horizontal
 * - Plataformas com colisao
 * - Gravidade e pulo
 * - Espada com animacao de corte
 * - Magia com projeteis
 * - Inimigos e combate
 * - Golem com muita vida
 * - Barra de vida do jogador
 * - Barra de vida do chefe
 * - Efeitos desenhados em C
 *
 * CONTROLES:
 * Direcional esquerdo/direito: mover
 * X: pular
 * QUADRADO: espada
 * TRIANGULO: magia
 * START: sair
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

/* ---------------------------------------------------------
   CONFIGURACAO
   --------------------------------------------------------- */

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272

#define TILE 24
#define FLOOR_Y 224

#define MAP_W 240
#define WORLD_W (MAP_W * TILE)

#define MAX_ENEMIES 10
#define MAX_MAGIC 8
#define MAX_PARTICLES 50

#define PLAYER_SPEED 150.0f
#define JUMP_POWER 310.0f
#define GRAVITY 680.0f

#define ZONE_SPLIT 100

/* ---------------------------------------------------------
   GRAFICOS PSP
   --------------------------------------------------------- */

static unsigned int __attribute__((aligned(16))) list[262144];

static unsigned int *fb0;
static unsigned int *fb1;
static unsigned int *zbuffer;

/* ---------------------------------------------------------
   ESTRUTURAS
   --------------------------------------------------------- */

typedef struct {
    float x;
    float y;

    float vx;
    float vy;

    float w;
    float h;

    int hp;
    int maxhp;

    int facing;
    int grounded;

    int attacking;
    float attackTime;

    int magicCooldown;
    float magicTimer;

    int invincible;
    float invincibleTimer;
} Player;

typedef struct {
    int x1;
    int x2;
    int row;
} Platform;

typedef struct {
    float x;
    float y;

    float vx;
    float vy;

    float life;
    int active;
} Magic;

typedef struct {
    float x;
    float y;

    float vx;

    float w;
    float h;

    int hp;
    int maxhp;

    int active;
    int type;

    int facing;

    float attackTimer;
    float hurtTimer;
} Enemy;

typedef struct {
    float x;
    float y;

    float vx;
    float vy;

    float life;
    int active;
} Particle;

/* ---------------------------------------------------------
   VARIAVEIS GLOBAIS
   --------------------------------------------------------- */

static Player p;

static Magic magic[MAX_MAGIC];
static Enemy enemies[MAX_ENEMIES];
static Particle particles[MAX_PARTICLES];

static float cameraX = 0.0f;

static int gameRunning = 1;

static int currentZone = 0;

/* ---------------------------------------------------------
   PLATAFORMAS
   x1 e x2 sao colunas do mapa.
   row define a altura: row * TILE.
   --------------------------------------------------------- */

static const Platform platforms[] = {

    /* ZONA 1 - CAVERNA */

    {  8,  14, 6 },
    { 20,  26, 7 },
    { 33,  40, 5 },
    { 48,  55, 7 },
    { 66,  74, 6 },
    { 83,  90, 5 },

    /* ZONA 2 - FLORESTA */

    {106, 115, 6 },
    {121, 130, 7 },
    {137, 145, 5 },
    {153, 162, 6 },
    {171, 180, 5 },
    {189, 198, 7 },
    {207, 216, 6 },
    {225, 234, 5 }
};

#define PLATFORM_COUNT \
    ((int)(sizeof(platforms) / sizeof(platforms[0])))

/* ---------------------------------------------------------
   CORES
   Formato GU: RGBA
   --------------------------------------------------------- */

#define COLOR(r,g,b,a) \
    ((unsigned int)(r) | \
    ((unsigned int)(g) << 8) | \
    ((unsigned int)(b) << 16) | \
    ((unsigned int)(a) << 24))

#define BLACK       COLOR(8, 8, 15, 255)
#define WHITE       COLOR(255, 255, 255, 255)
#define RED         COLOR(220, 45, 45, 255)
#define DARK_RED    COLOR(100, 15, 20, 255)
#define GREEN       COLOR(60, 220, 90, 255)
#define BLUE        COLOR(65, 140, 255, 255)
#define YELLOW      COLOR(255, 220, 70, 255)
#define ORANGE      COLOR(255, 120, 30, 255)
#define PURPLE      COLOR(180, 80, 255, 255)
#define CYAN        COLOR(80, 240, 255, 255)
#define GRAY        COLOR(95, 100, 115, 255)
#define DARK_GRAY   COLOR(35, 40, 55, 255)
#define BROWN       COLOR(95, 55, 35, 255)
#define DARK_BROWN  COLOR(45, 28, 25, 255)

/* ---------------------------------------------------------
   DESENHO 2D
   --------------------------------------------------------- */

static void drawRect(
    int x,
    int y,
    int w,
    int h,
    unsigned int color
) {
    struct RectVertex {
        unsigned int color;
        float x;
        float y;
        float z;
    };

    struct RectVertex vertices[2];

    if (w <= 0 || h <= 0)
        return;

    vertices[0].color = color;
    vertices[0].x = (float)x;
    vertices[0].y = (float)y;
    vertices[0].z = 0.0f;

    vertices[1].color = color;
    vertices[1].x = (float)(x + w);
    vertices[1].y = (float)(y + h);
    vertices[1].z = 0.0f;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2,
        0,
        vertices
    );
}

static void drawTriangle(

    float x1,
    float y1,
    float x2,
    float y2,
    float x3,
    float y3,
    unsigned int color
) {
    struct Vertex {
        unsigned int color;
        float x, y, z;
    };

    struct Vertex vertices[3] = {
        { color, x1, y1, 0 },
        { color, x2, y2, 0 },
        { color, x3, y3, 0 }
    };

    sceGuColor(color);

    sceGuDisable(GU_TEXTURE_2D);

    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        3,
        0,
        vertices
    );
}

static void drawCircle(
    int cx,
    int cy,
    int radius,
    unsigned int color
) {
    int i;

    if (radius <= 0)
        return;

    for (i = 0; i < 12; i++) {
        float a1 =
            (float)i * 6.2831853f / 12.0f;

        float a2 =
            (float)(i + 1) * 6.2831853f / 12.0f;

        drawTriangle(
            (float)cx,
            (float)cy,
            cx + cosf(a1) * radius,
            cy + sinf(a1) * radius,
            cx + cosf(a2) * radius,
            cy + sinf(a2) * radius,
            color
        );
    }
}

/* ---------------------------------------------------------
   TEXTO DEBUG
   --------------------------------------------------------- */

static void drawDebugText(void) {
    /* A interface visual principal e desenhada em C.
       O pspDebugScreen fica disponivel para diagnostico. */
}

/* ---------------------------------------------------------
   PLATAFORMAS E COLISAO
   --------------------------------------------------------- */

static float platformTop(int row) {
    return (float)(row * TILE);
}

static int overlaps(
    float ax,
    float aw,
    float bx,
    float bw
) {
    return ax < bx + bw &&
           ax + aw > bx;
}

static int platformOverlaps(
    float x,
    float w,
    int index
) {
    float left =
        (float)(platforms[index].x1 * TILE);

    float right =
        (float)((platforms[index].x2 + 1) * TILE);

    return overlaps(
        x,
        w,
        left,
        right - left
    );
}

static int isPlatformAt(
    float x,
    float y,
    float w
) {
    int i;

    if (y >= FLOOR_Y - 0.5f)
        return 1;

    for (i = 0; i < PLATFORM_COUNT; i++) {
        float top = platformTop(platforms[i].row);

        if (fabsf(y - top) < 1.0f &&
            platformOverlaps(x, w, i)) {
            return 1;
        }
    }

    return 0;
}

/* ---------------------------------------------------------
   INICIALIZACAO DO JOGADOR
   --------------------------------------------------------- */

static void initPlayer(void) {
    memset(&p, 0, sizeof(Player));

    p.x = 100.0f;
    p.y = (float)FLOOR_Y;

    p.w = 20.0f;
    p.h = 40.0f;

    p.hp = 100;
    p.maxhp = 100;

    p.facing = 1;
    p.grounded = 1;

    p.magicCooldown = 0;
}

/* ---------------------------------------------------------
   INICIALIZACAO DOS INIMIGOS
   --------------------------------------------------------- */

static void createEnemy(
    int index,
    float x,
    int type
) {
    Enemy *e = &enemies[index];

    memset(e, 0, sizeof(Enemy));

    e->x = x;
    e->y = (float)FLOOR_Y;

    e->type = type;
    e->active = 1;

    e->facing = -1;

    if (type == 1) {
        /* Golem */
        e->w = 42;
        e->h = 54;

        e->hp = 300;
        e->maxhp = 300;
    } else {
        /* Zumbi */
        e->w = 22;
        e->h = 36;

        e->hp = 45;
        e->maxhp = 45;
    }
}

static void initEnemies(void) {
    int i;

    memset(enemies, 0, sizeof(enemies));

    createEnemy(0, 650, 0);
    createEnemy(1, 1450, 1);
    createEnemy(2, 1900, 0);
    createEnemy(3, 2200, 0);

    createEnemy(4, 2800, 0);
    createEnemy(5, 3550, 1);
    createEnemy(6, 4050, 0);
    createEnemy(7, 4650, 0);
    createEnemy(8, 5200, 0);
    createEnemy(9, 5550, 0);

    for (i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].attackTimer = 0.0f;
        enemies[i].hurtTimer = 0.0f;
    }
}

/* ---------------------------------------------------------
   MAGIA
   --------------------------------------------------------- */

static void castMagic(void) {
    int i;

    if (p.magicTimer > 0.0f)
        return;

    for (i = 0; i < MAX_MAGIC; i++) {
        if (!magic[i].active) {
            magic[i].x =
                p.x + (p.facing > 0 ? 18.0f : -12.0f);

            magic[i].y = p.y - 24.0f;

            magic[i].vx =
                (float)p.facing * 280.0f;

            magic[i].vy = -15.0f;

            magic[i].life = 1.8f;
            magic[i].active = 1;

            p.magicTimer = 0.35f;

            break;
        }
    }
}

static void updateMagic(float dt) {
    int i, j;

    for (i = 0; i < MAX_MAGIC; i++) {
        Magic *m = &magic[i];

        if (!m->active)
            continue;

        m->x += m->vx * dt;
        m->y += m->vy * dt;

        m->life -= dt;

        if (m->life <= 0.0f ||
            m->x < 0 ||
            m->x > WORLD_W) {
            m->active = 0;
            continue;
        }

        for (j = 0; j < MAX_ENEMIES; j++) {
            Enemy *e = &enemies[j];

            if (!e->active)
                continue;

            if (fabsf(m->x - e->x) < e->w * 0.5f + 8 &&
                fabsf(m->y - (e->y - e->h * 0.5f))
                    < e->h * 0.5f + 8) {

                e->hp -= 35;
                e->hurtTimer = 0.15f;

                m->active = 0;

                if (e->hp <= 0) {
                    e->active = 0;
                }

                break;
            }
        }
    }
}

static void drawMagic(void) {
    int i;

    for (i = 0; i < MAX_MAGIC; i++) {
        Magic *m = &magic[i];

        if (!m->active)
            continue;

        int sx = (int)(m->x - cameraX);
        int sy = (int)m->y;

        drawCircle(sx, sy, 9, PURPLE);
        drawCircle(sx, sy, 5, CYAN);

        drawTriangle(
            sx - 13, sy,
            sx - 4, sy - 5,
            sx - 4, sy + 5,
            WHITE
        );
    }
}

/* ---------------------------------------------------------
   PARTICULAS
   --------------------------------------------------------- */

static void spawnParticles(
    float x,
    float y,
    unsigned int color
) {
    int i;

    for (i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].x = x;
            particles[i].y = y;

            particles[i].vx =
                (float)((rand() % 180) - 90);

            particles[i].vy =
                (float)(-40 - rand() % 140);

            particles[i].life =
                0.25f + (float)(rand() % 50) / 100.0f;

            particles[i].active = 1;

            (void)color;
            return;
        }
    }
}

static void updateParticles(float dt) {
    int i;

    for (i = 0; i < MAX_PARTICLES; i++) {
        Particle *a = &particles[i];

        if (!a->active)
            continue;

        a->x += a->vx * dt;
        a->y += a->vy * dt;

        a->vy += 200.0f * dt;

        a->life -= dt;

        if (a->life <= 0.0f)
            a->active = 0;
    }
}

static void drawParticles(void) {
    int i;

    for (i = 0; i < MAX_PARTICLES; i++) {
        Particle *a = &particles[i];

        if (!a->active)
            continue;

        drawRect(
            (int)(a->x - cameraX),
            (int)a->y,
            3,
            3,
            ORANGE
        );
    }
}

/* ---------------------------------------------------------
   ATAQUE DE ESPADA
   O dano e aplicado uma vez no inicio do golpe.
   --------------------------------------------------------- */

static void swordAttack(void) {
    int i;

    if (p.attacking)
        return;

    p.attacking = 1;
    p.attackTime = 0.28f;

    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];

        float dx;
        float dy;

        if (!e->active)
            continue;

        dx = e->x - p.x;
        dy = (e->y - e->h * 0.5f) -
             (p.y - p.h * 0.5f);

        if (p.facing > 0 &&
            dx > 0 &&
            dx < 64 &&
            fabsf(dy) < 43) {

            e->hp -= 25;
            e->hurtTimer = 0.18f;

            spawnParticles(
                e->x,
                e->y - e->h * 0.5f,
                YELLOW
            );
        }

        if (p.facing < 0 &&
            dx < 0 &&
            dx > -64 &&
            fabsf(dy) < 43) {

            e->hp -= 25;
            e->hurtTimer = 0.18f;

            spawnParticles(
                e->x,
                e->y - e->h * 0.5f,
                YELLOW
            );
        }

        if (e->hp <= 0)
            e->active = 0;
    }
}

/* ---------------------------------------------------------
   JOGADOR
   p.y representa a posicao dos pes.
   --------------------------------------------------------- */

static void updatePlayer(
    float dt,
    SceCtrlData *pad,
    SceCtrlData *oldPad
) {
    float previousY;
    float nextY;

    int left =
        (pad->Buttons & PSP_CTRL_LEFT) != 0;

    int right =
        (pad->Buttons & PSP_CTRL_RIGHT) != 0;

    int jumpPressed =
        (pad->Buttons & PSP_CTRL_CROSS) &&
        !(oldPad->Buttons & PSP_CTRL_CROSS);

    int swordPressed =
        (pad->Buttons & PSP_CTRL_SQUARE) &&
        !(oldPad->Buttons & PSP_CTRL_SQUARE);

    int magicPressed =
        (pad->Buttons & PSP_CTRL_TRIANGLE) &&
        !(oldPad->Buttons & PSP_CTRL_TRIANGLE);

    if (left) {
        p.vx = -PLAYER_SPEED;
        p.facing = -1;
    } else if (right) {
        p.vx = PLAYER_SPEED;
        p.facing = 1;
    } else {
        p.vx = 0.0f;
    }

    p.x += p.vx * dt;

    if (p.x < 0)
        p.x = 0;

    if (p.x > WORLD_W - p.w)
        p.x = WORLD_W - p.w;

    if (jumpPressed && p.grounded) {
        p.vy = -JUMP_POWER;
        p.grounded = 0;
    }

    previousY = p.y;

    p.vy += GRAVITY * dt;
    nextY = p.y + p.vy * dt;

    p.grounded = 0;

    /*
     * Colisao com o chao.
     */

    if (p.vy >= 0 &&
        previousY <= FLOOR_Y &&
        nextY >= FLOOR_Y) {

        nextY = FLOOR_Y;
        p.vy = 0;
        p.grounded = 1;
    }

    /*
     * Colisao com plataformas.
     * So aterrissa ao cair de cima.
     */

    if (p.vy >= 0) {
        int i;

        for (i = 0; i < PLATFORM_COUNT; i++) {
            float top =
                platformTop(platforms[i].row);

            if (previousY <= top &&
                nextY >= top &&
                platformOverlaps(
                    p.x,
                    p.w,
                    i
                )) {

                if (top < nextY ||
                    fabsf(top - nextY) < 1.0f) {

                    nextY = top;
                    p.vy = 0;
                    p.grounded = 1;
                }
            }
        }
    }

    p.y = nextY;

    if (p.y > FLOOR_Y) {
        p.y = FLOOR_Y;
        p.vy = 0;
        p.grounded = 1;
    }

    if (swordPressed)
        swordAttack();

    if (magicPressed)
        castMagic();

    if (p.attacking) {
        p.attackTime -= dt;

        if (p.attackTime <= 0.0f) {
            p.attackTime = 0.0f;
            p.attacking = 0;
        }
    }

    if (p.magicTimer > 0.0f)
        p.magicTimer -= dt;

    if (p.invincibleTimer > 0.0f)
        p.invincibleTimer -= dt;
    else
        p.invincible = 0;
}

/* ---------------------------------------------------------
   INIMIGOS
   --------------------------------------------------------- */

static void updateEnemies(float dt) {
    int i;

    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];

        if (!e->active)
            continue;

        if (e->hurtTimer > 0.0f)
            e->hurtTimer -= dt;

        if (e->attackTimer > 0.0f)
            e->attackTimer -= dt;

        float dx = p.x - e->x;

        if (fabsf(dx) < 240.0f) {
            e->facing = dx >= 0 ? 1 : -1;

            if (fabsf(dx) > 38.0f) {
                float speed =
                    e->type == 1 ? 38.0f : 55.0f;

                e->x +=
                    (dx > 0 ? speed : -speed) * dt;
            } else if (e->attackTimer <= 0.0f) {
                if (p.invincibleTimer <= 0.0f) {
                    p.hp -=
                        e->type == 1 ? 15 : 7;

                    if (p.hp < 0)
                        p.hp = 0;

                    p.invincible = 1;
                    p.invincibleTimer = 0.7f;
                }

                e->attackTimer = 1.0f;
            }
        }

        if (e->x < 0)
            e->x = 0;

        if (e->x > WORLD_W - e->w)
            e->x = WORLD_W - e->w;
    }
}

/* ---------------------------------------------------------
   CAMERA
   --------------------------------------------------------- */

static void updateCamera(void) {
    cameraX = p.x - SW * 0.5f;

    if (cameraX < 0)
        cameraX = 0;

    if (cameraX > WORLD_W - SW)
        cameraX = WORLD_W - SW;

    if (p.x < ZONE_SPLIT * TILE)
        currentZone = 0;
    else
        currentZone = 1;
}

/* ---------------------------------------------------------
   CENARIO
   --------------------------------------------------------- */

static void drawBackground(void) {
    int x;

    if (currentZone == 0) {
        /* Caverna */

        drawRect(0, 0, SW, SH,
                 COLOR(12, 12, 32, 255));

        /* Formacoes rochosas */

        for (x = 0; x < SW; x += 80) {
            drawTriangle(
                x, 0,
                x + 40, 48,
                x + 80, 0,
                COLOR(28, 30, 55, 255)
            );

            drawTriangle(
                x + 10, SH,
                x + 40, 218,
                x + 70, SH,
                COLOR(22, 24, 43, 255)
            );
        }

        /* Cristais */

        drawTriangle(
            70, 120,
            80, 95,
            90, 120,
            PURPLE
        );

        drawTriangle(
            340, 145,
            350, 115,
            360, 145,
            CYAN
        );
    } else {
        /* Floresta */

        drawRect(0, 0, SW, SH,
                 COLOR(10, 38, 35, 255));

        /* Lua / luz */

        drawCircle(380, 48, 24,
                   COLOR(90, 140, 130, 255));

        drawCircle(388, 43, 20,
                   COLOR(10, 38, 35, 255));

        /* Silhuetas de arvores */

        for (x = 0; x < SW; x += 95) {
            drawTriangle(
                x + 5, 190,
                x + 42, 48,
                x + 78, 190,
                COLOR(12, 65, 45, 255)
            );

            drawRect(
                x + 34,
                160,
                15,
                60,
                COLOR(48, 35, 28, 255)
            );
        }
    }
}

static void drawPlatforms(void) {
    int i;

    /* Chao principal */

    drawRect(
        0,
        FLOOR_Y,
        SW,
        SH - FLOOR_Y,
        DARK_BROWN
    );

    drawRect(
        0,
        FLOOR_Y,
        SW,
        5,
        COLOR(100, 85, 65, 255)
    );

    for (i = 0; i < PLATFORM_COUNT; i++) {
        float wx1 =
            (float)(platforms[i].x1 * TILE);

        float wx2 =
            (float)((platforms[i].x2 + 1) * TILE);

        float sy =
            platformTop(platforms[i].row);

        int sx = (int)(wx1 - cameraX);
        int ex = (int)(wx2 - cameraX);

        int width = ex - sx;

        if (ex < 0 || sx > SW)
            continue;

        if (currentZone == 0) {
            drawRect(
                sx,
                (int)sy,
                width,
                12,
                COLOR(60, 60, 82, 255)
            );

            drawRect(
                sx,
                (int)sy,
                width,
                4,
                COLOR(115, 120, 155, 255)
            );
        } else {
            drawRect(
                sx,
                (int)sy,
                width,
                12,
                COLOR(60, 90, 52, 255)
            );

            drawRect(
                sx,
                (int)sy,
                width,
                4,
                COLOR(100, 170, 75, 255)
            );
        }

        /* Detalhes da plataforma */

        drawRect(
            sx + 5,
            (int)sy + 7,
            6,
            2,
            DARK_GRAY
        );

        drawRect(
            sx + width / 2,
            (int)sy + 8,
            8,
            2,
            DARK_GRAY
        );
    }
}

/* ---------------------------------------------------------
   JOGADOR VISUAL
   --------------------------------------------------------- */

static void drawPlayer(void) {
    int sx = (int)(p.x - cameraX);
    int sy = (int)p.y;

    int bodyColor =
        (p.invincible &&
         ((int)(p.invincibleTimer * 20) % 2))
        ? WHITE
        : COLOR(25, 25, 38, 255);

    /*
     * Capa.
     */

    drawTriangle(
        sx - 5,
        sy - 34,
        sx + 5,
        sy - 35,
        sx - 10,
        sy - 8,
        COLOR(130, 20, 35, 255)
    );

    /*
     * Pernas.
     */

    drawRect(
        sx - 7,
        sy - 13,
        6,
        13,
        DARK_GRAY
    );

    drawRect(
        sx + 2,
        sy - 13,
        6,
        13,
        DARK_GRAY
    );

    /*
     * Botas.
     */

    drawRect(
        sx - 9,
        sy - 3,
        9,
        4,
        BLACK
    );

    drawRect(
        sx + 1,
        sy - 3,
        9,
        4,
        BLACK
    );

    /*
     * Corpo.
     */

    drawRect(
        sx - 9,
        sy - 33,
        18,
        21,
        bodyColor
    );

    drawRect(
        sx - 8,
        sy - 31,
        16,
        3,
        COLOR(130, 25, 40, 255)
    );

    /*
     * Cabeca.
     */

    drawRect(
        sx - 7,
        sy - 45,
        14,
        13,
        COLOR(220, 180, 145, 255)
    );

    /*
     * Cabelo.
     */

    drawRect(
        sx - 8,
        sy - 46,
        16,
        5,
        BLACK
    );

    drawTriangle(
        sx - 7,
        sy - 42,
        sx - 2,
        sy - 48,
        sx + 1,
        sy - 41,
        BLACK
    );

    /*
     * Olhos.
     */

    drawRect(
        sx + (p.facing > 0 ? 2 : -5),
        sy - 39,
        3,
        2,
        RED
    );

    /*
     * Braco.
     */

    drawRect(
        sx + (p.facing > 0 ? 5 : -9),
        sy - 30,
        5,
        13,
        COLOR(220, 180, 145, 255)
    );

    /*
     * Espada e animacao do golpe.
     */

    if (p.attacking) {
        float t = p.attackTime / 0.28f;

        int swordX =
            sx + p.facing * (12 + (int)(20 * (1.0f - t)));

        int swordY =
            sy - 28 - (int)(10 * sinf(t * 3.14159f));

        /*
         * Rastro do golpe.
         */

        drawTriangle(
            sx + p.facing * 5,
            sy - 40,
            sx + p.facing * 48,
            sy - 36,
            sx + p.facing * 42,
            sy - 7,
            COLOR(90, 180, 255, 130)
        );

        drawTriangle(
            sx + p.facing * 10,
            sy - 35,
            sx + p.facing * 44,
            sy - 30,
            sx + p.facing * 35,
            sy - 12,
            COLOR(190, 235, 255, 220)
        );

        /*
         * Lamina.
         */

        drawTriangle(
            swordX,
            swordY,
            swordX + p.facing * 30,
            swordY - 12,
            swordX + p.facing * 27,
            swordY - 5,
            WHITE
        );

        drawRect(
            sx + p.facing * 8,
            sy - 28,
            5,
            4,
            YELLOW
        );
    } else {
        /*
         * Espada em repouso.
         */

        drawTriangle(
            sx + p.facing * 8,
            sy - 24,
            sx + p.facing * 23,
            sy - 18,
            sx + p.facing * 10,
            sy - 14,
            COLOR(185, 195, 210, 255)
        );

        drawRect(
            sx + p.facing * 7,
            sy - 26,
            5,
            4,
            BROWN
        );
    }
}

/* ---------------------------------------------------------
   INIMIGOS VISUAIS
   --------------------------------------------------------- */

static void drawEnemy(Enemy *e) {
    int sx;
    int sy;

    if (!e->active)
        return;

    sx = (int)(e->x - cameraX);
    sy = (int)e->y;

    if (sx < -70 || sx > SW + 70)
        return;

    if (e->type == 1) {
        /* GOLEM */

        unsigned int stone =
            e->hurtTimer > 0
            ? WHITE
            : COLOR(100, 105, 120, 255);

        /* Pernas */

        drawRect(
            sx - 16,
            sy - 20,
            13,
            20,
            DARK_GRAY
        );

        drawRect(
            sx + 3,
            sy - 20,
            13,
            20,
            DARK_GRAY
        );

        /* Corpo */

        drawRect(
            sx - 20,
            sy - 48,
            40,
            29,
            stone
        );

        drawRect(
            sx - 24,
            sy - 45,
            8,
            20,
            stone
        );

        drawRect(
            sx + 16,
            sy - 45,
            8,
            20,
            stone
        );

        /* Cabeca */

        drawRect(
            sx - 16,
            sy - 62,
            32,
            16,
            stone
        );

        /* Olhos */

        drawRect(
            sx - 10,
            sy - 56,
            6,
            4,
            RED
        );

        drawRect(
            sx + 4,
            sy - 56,
            6,
            4,
            RED
        );

        /* Fendas da pedra */

        drawRect(
            sx - 5,
            sy - 42,
            3,
            10,
            DARK_GRAY
        );

        drawRect(
            sx + 7,
            sy - 36,
            3,
            8,
            DARK_GRAY
        );
    } else {
        /* ZUMBI */

        unsigned int skin =
            e->hurtTimer > 0
            ? WHITE
            : COLOR(75, 150, 85, 255);

        drawRect(
            sx - 8,
            sy - 15,
            6,
            15,
            DARK_GRAY
        );

        drawRect(
            sx + 2,
            sy - 15,
            6,
            15,
            DARK_GRAY
        );

        drawRect(
            sx - 9,
            sy - 30,
            18,
            17,
            COLOR(60, 80, 65, 255)
        );

        drawRect(
            sx - 7,
            sy - 40,
            14,
            11,
            skin
        );

        drawRect(
            sx - 6,
            sy - 39,
            4,
            3,
            RED
        );

        drawRect(
            sx + 2,
            sy - 39,
            4,
            3,
            RED
        );

        drawRect(
            sx + (e->facing > 0 ? 5 : -11),
            sy - 29,
            6,
            14,
            skin
        );
    }
}

/* ---------------------------------------------------------
   INTERFACE
   --------------------------------------------------------- */

static void drawHealthBar(
    int x,
    int y,
    int width,
    int height,
    int hp,
    int maxhp,
    unsigned int color
) {
    int filled;

    if (maxhp <= 0)
        return;

    if (hp < 0)
        hp = 0;

    if (hp > maxhp)
        hp = maxhp;

    filled = width * hp / maxhp;

    drawRect(
        x - 1,
        y - 1,
        width + 2,
        height + 2,
        BLACK
    );

    drawRect(
        x,
        y,
        width,
        height,
        DARK_RED
    );

    drawRect(
        x,
        y,
        filled,
        height,
        color
    );
}

static void drawHUD(void) {
    int i;
    Enemy *boss = 0;

    drawHealthBar(
        10,
        10,
        120,
        10,
        p.hp,
        p.maxhp,
        GREEN
    );

    /*
     * Indicador da zona.
     */

    if (currentZone == 0) {
        drawRect(
            365,
            10,
            105,
            18,
            COLOR(35, 35, 65, 255)
        );

        drawRect(
            371,
            16,
            8,
            6,
            PURPLE
        );

        drawRect(
            383,
            16,
            70,
            3,
            WHITE
        );
    } else {
        drawRect(
            365,
            10,
            105,
            18,
            COLOR(20, 65, 40, 255)
        );

        drawRect(
            371,
            16,
            8,
            6,
            GREEN
        );

        drawRect(
            383,
            16,
            70,
            3,
            WHITE
        );
    }

    /*
     * Barra do chefe quando estiver perto.
     */

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active &&
            enemies[i].type == 1 &&
            fabsf(enemies[i].x - p.x) < 500) {

            boss = &enemies[i];
            break;
        }
    }

    if (boss) {
        drawHealthBar(
            115,
            34,
            250,
            10,
            boss->hp,
            boss->maxhp,
            RED
        );
    }
}

/* ---------------------------------------------------------
   RENDERIZACAO COMPLETA
   --------------------------------------------------------- */

static void drawWorld(void) {
    int i;

    drawBackground();

    drawPlatforms();

    for (i = 0; i < MAX_ENEMIES; i++)
        drawEnemy(&enemies[i]);

    drawMagic();

    drawParticles();

    drawPlayer();

    drawHUD();
}

/* ---------------------------------------------------------
   INICIALIZACAO GRAFICA
   --------------------------------------------------------- */

static void graphicsInit(void) {
    sceGuInit();

    sceGuStart(
        GU_DIRECT,
        list
    );

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        512
    );

    sceGuDispBuffer(
        SW,
        SH,
        (void *)(512 * 272 * 4),
        512
    );

    sceGuDepthBuffer(
        (void *)(512 * 272 * 8),
        512
    );

    sceGuOffset(
        2048 - (SW / 2),
        2048 - (SH / 2)
    );

    sceGuViewport(
        2048,
        2048,
        SW,
        SH
    );

    sceGuDepthRange(
        0,
        65535
    );

    sceGuScissor(
        0,
        0,
        SW,
        SH
    );

    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);

    sceGuClearColor(BLACK);

    sceGuClear(
        GU_COLOR_BUFFER_BIT |
        GU_DEPTH_BUFFER_BIT
    );

    sceGuFinish();

    sceGuSync(
        0,
        0
    );

    sceDisplayWaitVblankStart();

    sceGuDisplay(GU_TRUE);
}

/* ---------------------------------------------------------
   ENCERRAMENTO
   --------------------------------------------------------- */

static int exitCallback(
    int arg1,
    int arg2,
    void *common
) {
    (void)arg1;
    (void)arg2;
    (void)common;

    gameRunning = 0;

    return 0;
}

static int callbackThread(
    SceSize args,
    void *argp
) {
    int cbid;

    (void)args;
    (void)argp;

    cbid = sceKernelCreateCallback(
        "Exit Callback",
        exitCallback,
        0
    );

    sceKernelRegisterExitCallback(cbid);

    sceKernelSleepThreadCB();

    return 0;
}

static int setupCallbacks(void) {
    int thid;

    thid = sceKernelCreateThread(
        "Callback Thread",
        callbackThread,
        0x11,
        0xFA0,
        0,
        0
    );

    if (thid >= 0)
        sceKernelStartThread(thid, 0, 0);

    return thid;
}

/* ---------------------------------------------------------
   LOOP PRINCIPAL
   --------------------------------------------------------- */

int main(int argc, char *argv[]) {
    SceCtrlData pad;
    SceCtrlData oldPad;

    unsigned long long lastTime;
    unsigned long long currentTime;

    float dt;

    (void)argc;
    (void)argv;

    setupCallbacks();

    sceCtrlSetSamplingCycle(0);

    sceCtrlSetSamplingMode(
        PSP_CTRL_MODE_ANALOG
    );

    memset(&oldPad, 0, sizeof(oldPad));

    memset(magic, 0, sizeof(magic));
    memset(particles, 0, sizeof(particles));

    initPlayer();
    initEnemies();

    graphicsInit();

    lastTime = sceKernelGetSystemTimeWide();

    while (gameRunning) {
        sceCtrlReadBufferPositive(
            &pad,
            1
        );

        if (pad.Buttons & PSP_CTRL_START) {
            gameRunning = 0;
            break;
        }

        currentTime =
            sceKernelGetSystemTimeWide();

        dt =
            (float)(currentTime - lastTime) /
            1000000.0f;

        lastTime = currentTime;

        /*
         * Evita saltos gigantes de fisica
         * se o jogo sofrer uma queda de FPS.
         */

        if (dt > 0.04f)
            dt = 0.04f;

        if (dt < 0.001f)
            dt = 0.001f;

        updatePlayer(
            dt,
            &pad,
            &oldPad
        );

        updateEnemies(dt);

        updateMagic(dt);

        updateParticles(dt);

        updateCamera();

        /*
         * Renderizacao.
         */

        sceGuStart(
            GU_DIRECT,
            list
        );

        sceGuClearColor(BLACK);

        sceGuClear(
            GU_COLOR_BUFFER_BIT |
            GU_DEPTH_BUFFER_BIT
        );

        drawWorld();

        sceGuFinish();

        sceGuSync(
            0,
            0
        );

        sceDisplayWaitVblankStart();

        sceGuSwapBuffers();

        oldPad = pad;

        /*
         * Derrota.
         */

        if (p.hp <= 0) {
            gameRunning = 0;
        }
    }

    sceGuTerm();

    sceKernelExitGame();

    return 0;
}
