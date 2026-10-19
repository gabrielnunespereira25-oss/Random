/*
 * RANDOM
 * Essencia Escura
 * PSP / PSPSDK
 *
 * Controles:
 * Analógico / Direcional: mover
 * X: pular
 * Quadrado: atacar
 * R: correr
 * START: sair
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspge.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272

#define TILE 24
#define MAP_W 20
#define MAP_H 11
#define ZONE_COUNT 6

#define GRAVITY 900.0f
#define WALK_SPEED 145.0f
#define RUN_SPEED 215.0f
#define JUMP_SPEED 340.0f
#define ATTACK_DURATION 0.50f

#define BLACK 0xFF050508
#define SHADOW 0xFF171720
#define GRAY 0xFF454552
#define EDGE 0xFF737385
#define WHITE 0xFFFFFFFF
#define GLOW 0xFFDDEBFF
#define BACKGROUND 0xFF101016

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef enum {
    ANIM_IDLE,
    ANIM_WALK,
    ANIM_RUN,
    ANIM_JUMP,
    ANIM_FALL,
    ANIM_LAND,
    ANIM_ATTACK
} AnimState;

typedef struct {
    float x, y;
    float vx, vy;
    float animTime;
    float attackTimer;
    float landTimer;

    int facing;
    int grounded;
    int attacking;
    int running;

    AnimState animState;
} Player;

static unsigned int __attribute__((aligned(16))) displayList[262144];
static Player player;

static int currentZone = 0;
static int magicFound = 0;

/* =====================================================
   PROTOTIPOS
   ===================================================== */

static void drawTriangle(
    float x1, float y1,
    float x2, float y2,
    float x3, float y3,
    unsigned int color
);

static void drawCircle(
    float cx, float cy, float radius,
    unsigned int color
);

static void drawLine(
    float x1, float y1,
    float x2, float y2,
    float thickness,
    unsigned int color
);

static void drawPolygon(
    const float *points,
    int count,
    unsigned int color
);

/* =====================================================
   GEOMETRIA
   ===================================================== */

static void drawTriangle(
    float x1, float y1,
    float x2, float y2,
    float x3, float y3,
    unsigned int color
) {
    Vertex *v = (Vertex *)sceGuGetMemory(
        sizeof(Vertex) * 3
    );

    v[0].color = color;
    v[0].x = x1;
    v[0].y = y1;
    v[0].z = 0.0f;

    v[1].color = color;
    v[1].x = x2;
    v[1].y = y2;
    v[1].z = 0.0f;

    v[2].color = color;
    v[2].x = x3;
    v[2].y = y3;
    v[2].z = 0.0f;

    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        3, 0, v
    );
}

static void drawCircle(
    float cx, float cy, float radius,
    unsigned int color
) {
    int i;
    const int segments = 24;
    const float PI = 3.14159265f;

    for (i = 0; i < segments; i++) {
        float a0 = 2.0f * PI * i / segments;
        float a1 = 2.0f * PI * (i + 1) / segments;

        drawTriangle(
            cx, cy,
            cx + cosf(a0) * radius,
            cy + sinf(a0) * radius,
            cx + cosf(a1) * radius,
            cy + sinf(a1) * radius,
            color
        );
    }
}

static void drawLine(
    float x1, float y1,
    float x2, float y2,
    float thickness,
    unsigned int color
) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    float nx, ny;

    if (len < 0.001f)
        return;

    nx = -dy / len * thickness * 0.5f;
    ny =  dx / len * thickness * 0.5f;

    drawTriangle(
        x1 + nx, y1 + ny,
        x1 - nx, y1 - ny,
        x2 + nx, y2 + ny,
        color
    );

    drawTriangle(
        x1 - nx, y1 - ny,
        x2 - nx, y2 - ny,
        x2 + nx, y2 + ny,
        color
    );
}

static void drawPolygon(
    const float *points,
    int count,
    unsigned int color
) {
    int i;

    if (count < 3)
        return;

    for (i = 1; i < count - 1; i++) {
        drawTriangle(
            points[0], points[1],
            points[i * 2], points[i * 2 + 1],
            points[(i + 1) * 2],
            points[(i + 1) * 2 + 1],
            color
        );
    }
}

/* =====================================================
   MAPAS INICIAIS
   # terreno
   . vazio
   S inicio
   M magia
   D saida
   ===================================================== */

static const char *maps[ZONE_COUNT][MAP_H] = {
    {
        "####################",
        "##....##...........#",
        "#....##....##......#",
        "####.....###...##..#",
        "#....###......##...#",
        "#.M..#...##........#",
        "########.####..#####",
        "#.............##...#",
        "###..####..###.....#",
        "#S....D............#",
        "####################"
    },
    {
        "####################",
        "#..................#",
        "#...###......###...#",
        "#........##........#",
        "###.............####",
        "#......###.........#",
        "#..##........##....#",
        "#.................D#",
        "#S....###..........#",
        "#..................#",
        "####################"
    },
    {
        "####################",
        "#..................#",
        "#..####....####....#",
        "#..................#",
        "####.....###....####",
        "#..................#",
        "#....####..........#",
        "#..............##..#",
        "#S.................#",
        "#.................D#",
        "####################"
    },
    {
        "####################",
        "#........##........#",
        "#..####......####..#",
        "#..................#",
        "####....####....####",
        "#..................#",
        "#....####..........#",
        "#...........####...#",
        "#S.................#",
        "#.................D#",
        "####################"
    },
    {
        "####################",
        "#..............#...#",
        "#..####........#...#",
        "#..............#...#",
        "#........####..#...#",
        "#..............#...#",
        "#..####........#...#",
        "#..............#...#",
        "#S.................#",
        "#.................D#",
        "####################"
    },
    {
        "####################",
        "#..................#",
        "#..................#",
        "#...##........##...#",
        "#..................#",
        "#..................#",
        "#..................#",
        "#..................#",
        "#S.................#",
        "#..................#",
        "####################"
    }
};

static int tileSolid(int tx, int ty) {
    if (tx < 0 || tx >= MAP_W ||
        ty < 0 || ty >= MAP_H)
        return 1;

    return maps[currentZone][ty][tx] == '#';
}

/* =====================================================
   DESENHO DO MAPA
   ===================================================== */

static void drawMap(float cameraX, float cameraY) {
    int tx, ty;

    for (ty = 0; ty < MAP_H; ty++) {
        for (tx = 0; tx < MAP_W; tx++) {
            char tile = maps[currentZone][ty][tx];

            float x = tx * TILE - cameraX;
            float y = ty * TILE - cameraY;

            if (x < -TILE || x > SCREEN_W ||
                y < -TILE || y > SCREEN_H)
                continue;

            if (tile == '#') {
                /*
                 * Superficie rochosa irregular,
                 * desenhada com triangulos.
                 */
                float rock[] = {
                    x, y + 5,
                    x + 5, y,
                    x + 13, y + 2,
                    x + 19, y,
                    x + TILE, y + 5,
                    x + TILE, y + TILE,
                    x, y + TILE
                };

                drawPolygon(rock, 7, GRAY);

                drawLine(
                    x, y + 5,
                    x + TILE, y + 5,
                    1.0f, EDGE
                );
            }

            if (tile == 'M' && !magicFound) {
                drawCircle(
                    x + 12, y + 12,
                    8.0f, SHADOW
                );

                drawTriangle(
                    x + 12, y + 1,
                    x + 5, y + 13,
                    x + 12, y + 21,
                    GLOW
                );

                drawTriangle(
                    x + 12, y + 1,
                    x + 19, y + 13,
                    x + 12, y + 21,
                    WHITE
                );
            }

            if (tile == 'D') {
                drawLine(
                    x + 5, y + 23,
                    x + 5, y + 5,
                    2.0f, EDGE
                );

                drawLine(
                    x + 19, y + 23,
                    x + 19, y + 5,
                    2.0f, EDGE
                );

                drawLine(
                    x + 5, y + 5,
                    x + 19, y + 5,
                    2.0f, WHITE
                );
            }
        }
    }
}

/* =====================================================
   ESSENCIA ESCURA
   ===================================================== */

static void playerDraw(
    Player *p,
    float cameraX,
    float cameraY
) {
    float x = p->x - cameraX;
    float y = p->y - cameraY;

    float t = p->animTime;
    float face = (float)p->facing;

    float bob = 0.0f;
    float lean = 0.0f;
    float leg = 0.0f;
    float arm = 0.0f;

    if (p->animState == ANIM_IDLE)
        bob = sinf(t * 3.0f);

    if (p->animState == ANIM_WALK) {
        bob = fabsf(sinf(t * 9.0f)) * 2.0f;
        leg = sinf(t * 9.0f) * 4.0f;
        arm = sinf(t * 9.0f) * 3.0f;
    }

    if (p->animState == ANIM_RUN) {
        bob = fabsf(sinf(t * 14.0f)) * 2.5f;
        leg = sinf(t * 14.0f) * 6.0f;
        arm = sinf(t * 14.0f) * 5.0f;
        lean = face * 3.0f;
    }

    if (p->animState == ANIM_JUMP) {
        leg = 4.0f;
        arm = -4.0f;
        lean = face * 2.0f;
    }

    if (p->animState == ANIM_FALL) {
        leg = -3.0f;
        arm = 4.0f;
    }

    float ox = x + lean;
    float oy = y + bob;

    /* Sombra circular */
    if (p->grounded)
        drawCircle(x + 14, y + 39, 8, BLACK);

    /* Manto */
    {
        float cape[] = {
            ox + 8, oy + 12,
            ox + 2, oy + 18,
            ox + 4, oy + 25,
            ox - 3, oy + 34,
            ox + 7, oy + 30,
            ox + 14, oy + 19
        };

        drawPolygon(cape, 6, BLACK);

        drawLine(
            ox + 7, oy + 16,
            ox + 2, oy + 29,
            1.0f, EDGE
        );
    }

    /* Pernas */
    {
        float leftLeg[] = {
            ox + 8, oy + 23,
            ox + 13, oy + 24,
            ox + 12 + leg * 0.35f, oy + 33,
            ox + 7 + leg * 0.25f, oy + 38,
            ox + 3 + leg * 0.25f, oy + 38,
            ox + 7, oy + 31
        };

        float rightLeg[] = {
            ox + 15, oy + 23,
            ox + 20, oy + 23,
            ox + 22 - leg * 0.35f, oy + 32,
            ox + 26 - leg * 0.25f, oy + 37,
            ox + 22 - leg * 0.25f, oy + 38,
            ox + 17, oy + 31
        };

        drawPolygon(leftLeg, 6, BLACK);
        drawPolygon(rightLeg, 6, BLACK);

        drawLine(
            ox + 4 + leg * 0.25f, oy + 38,
            ox + 9 + leg * 0.25f, oy + 38,
            1.0f, EDGE
        );

        drawLine(
            ox + 21 - leg * 0.25f, oy + 38,
            ox + 26 - leg * 0.25f, oy + 37,
            1.0f, EDGE
        );
    }

    /* Braco traseiro */
    drawLine(
        ox + 8, oy + 14,
        ox + 4 + arm * 0.3f, oy + 23,
        3.5f, BLACK
    );

    drawCircle(
        ox + 4 + arm * 0.3f,
        oy + 23, 2.0f, SHADOW
    );

    /* Tronco angular */
    {
        float torso[] = {
            ox + 8, oy + 10,
            ox + 16, oy + 9,
            ox + 22, oy + 15,
            ox + 18, oy + 23,
            ox + 12, oy + 27,
            ox + 6, oy + 19
        };

        drawPolygon(torso, 6, BLACK);

        drawTriangle(
            ox + 8, oy + 12,
            ox + 15, oy + 15,
            ox + 10, oy + 20,
            SHADOW
        );

        drawLine(
            ox + 10, oy + 13,
            ox + 17, oy + 21,
            0.8f, EDGE
        );
    }

    /* Braco dianteiro */
    drawLine(
        ox + 19, oy + 13,
        ox + 24 + arm * 0.3f, oy + 21,
        3.5f, BLACK
    );

    drawCircle(
        ox + 24 + arm * 0.3f,
        oy + 21, 2.0f, SHADOW
    );

    /* Cabeca */
    drawTriangle(
        ox + 5, oy + 10,
        ox + 8, oy + 2,
        ox + 13, oy + 9,
        BLACK
    );

    drawTriangle(
        ox + 10, oy + 8,
        ox + 18, oy + 1,
        ox + 21, oy + 11,
        BLACK
    );

    drawCircle(
        ox + 13, oy + 10, 6.5f, BLACK
    );

    /* Chama branca */
    drawTriangle(
        ox + 7, oy + 7,
        ox + 9, oy - 2,
        ox + 13, oy + 5,
        WHITE
    );

    drawTriangle(
        ox + 11, oy + 5,
        ox + 18, oy - 5,
        ox + 18, oy + 7,
        GLOW
    );

    drawTriangle(
        ox + 16, oy + 6,
        ox + 22, oy + 2,
        ox + 20, oy + 11,
        WHITE
    );

    /* Olho luminoso */
    if (p->facing > 0) {
        drawLine(
            ox + 15, oy + 10,
            ox + 20, oy + 9,
            1.4f, WHITE
        );
    } else {
        drawLine(
            ox + 6, oy + 9,
            ox + 11, oy + 10,
            1.4f, WHITE
        );
    }

    /* Espada quando nao ataca */
    if (!p->attacking) {
        float sx = ox + 14 + face * 7;
        float sy = oy + 19;

        drawLine(
            sx, sy,
            sx + face * 13,
            sy - 12,
            1.5f, GLOW
        );

        drawTriangle(
            sx + face * 13, sy - 12,
            sx + face * 9, sy - 10,
            sx + face * 11, sy - 7,
            WHITE
        );

        drawLine(
            sx - face * 2, sy + 2,
            sx + face * 3, sy - 3,
            2.0f, EDGE
        );
    }

    /* Ataque de meia-lua: duracao total 0,5 s */
    if (p->attacking) {
        float progress =
            1.0f - p->attackTimer / ATTACK_DURATION;

        float cx = ox + 14 + face * 12;
        float cy = oy + 18;
        float radius = 17.0f + progress * 10.0f;

        int i;
        const int segments = 20;
        const float PI = 3.14159265f;

        if (progress < 0.72f) {
            for (i = 0; i < segments; i++) {
                float a0 = -1.45f +
                    2.90f * i / segments;

                float a1 = -1.45f +
                    2.90f * (i + 1) / segments;

                float x0 = cx + face * cosf(a0) * radius;
                float y0 = cy + sinf(a0) * radius;

                float x1 = cx + face * cosf(a1) * radius;
                float y1 = cy + sinf(a1) * radius;

                float inner = radius - 2.5f;

                float ix0 = cx + face * cosf(a0) * inner;
                float iy0 = cy + sinf(a0) * inner;

                float ix1 = cx + face * cosf(a1) * inner;
                float iy1 = cy + sinf(a1) * inner;

                drawTriangle(
                    x0, y0, x1, y1, ix0, iy0, WHITE
                );

                drawTriangle(
                    ix0, iy0, x1, y1, ix1, iy1, GLOW
                );
            }
        }
    }
}

/* =====================================================
   COLISOES SIMPLES
   ===================================================== */

static void resolveMapCollision(Player *p) {
    int left, right, top, bottom;

    /* Colisao horizontal */
    if (p->vx > 0.0f) {
        right = (int)((p->x + 26.0f) / TILE);
        top = (int)((p->y + 4.0f) / TILE);
        bottom = (int)((p->y + 37.0f) / TILE);

        if (tileSolid(right, top) ||
            tileSolid(right, bottom)) {
            p->x = right * TILE - 27.0f;
            p->vx = 0.0f;
        }
    } else if (p->vx < 0.0f) {
        left = (int)(p->x / TILE);
        top = (int)((p->y + 4.0f) / TILE);
        bottom = (int)((p->y + 37.0f) / TILE);

        if (tileSolid(left, top) ||
            tileSolid(left, bottom)) {
            p->x = (left + 1) * TILE;
            p->vx = 0.0f;
        }
    }

    /* Colisao vertical */
    left = (int)((p->x + 3.0f) / TILE);
    right = (int)((p->x + 23.0f) / TILE);

    if (p->vy >= 0.0f) {
        bottom = (int)((p->y + 38.0f) / TILE);

        if (tileSolid(left, bottom) ||
            tileSolid(right, bottom)) {
            p->y = bottom * TILE - 39.0f;
            p->vy = 0.0f;
            p->grounded = 1;
        } else {
            p->grounded = 0;
        }
    } else {
        top = (int)(p->y / TILE);

        if (tileSolid(left, top) ||
            tileSolid(right, top)) {
            p->y = (top + 1) * TILE;
            p->vy = 0.0f;
        }
    }
}

/* =====================================================
   ATUALIZACAO DO JOGADOR
   ===================================================== */

static void playerUpdate(
    Player *p,
    float dt,
    unsigned int buttons,
    SceCtrlData *pad
) {
    float axis = 0.0f;

    if (pad->Lx < 110)
        axis = -1.0f;
    else if (pad->Lx > 145)
        axis = 1.0f;

    if (buttons & PSP_CTRL_LEFT)
        axis = -1.0f;

    if (buttons & PSP_CTRL_RIGHT)
        axis = 1.0f;

    p->running =
        (buttons & PSP_CTRL_RTRIGGER) != 0;

    if (!p->attacking) {
        p->vx = axis *
            (p->running ? RUN_SPEED : WALK_SPEED);

        if (axis < 0.0f)
            p->facing = -1;

        if (axis > 0.0f)
            p->facing = 1;
    }

    if ((buttons & PSP_CTRL_CROSS) && p->grounded) {
        p->vy = -JUMP_SPEED;
        p->grounded = 0;
    }

    p->vy += GRAVITY * dt;

    p->x += p->vx * dt;
    resolveMapCollision(p);

    p->y += p->vy * dt;
    resolveMapCollision(p);

    if (p->attacking) {
        p->attackTimer -= dt;

        if (p->attackTimer <= 0.0f) {
            p->attackTimer = 0.0f;
            p->attacking = 0;
        }
    }

    if (p->landTimer > 0.0f)
        p->landTimer -= dt;

    if (p->attacking) {
        p->animState = ANIM_ATTACK;
    } else if (!p->grounded && p->vy < 0.0f) {
        p->animState = ANIM_JUMP;
    } else if (!p->grounded) {
        p->animState = ANIM_FALL;
    } else if (p->landTimer > 0.0f) {
        p->animState = ANIM_LAND;
    } else if (fabsf(p->vx) > 1.0f) {
        p->animState = p->running ? ANIM_RUN : ANIM_WALK;
    } else {
        p->animState = ANIM_IDLE;
    }

    p->animTime += dt;
}

/* =====================================================
   INICIALIZACAO GRAFICA
   ===================================================== */

static void graphicsInit(void) {
    sceGuInit();

    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        512
    );

    sceGuDispBuffer(
        SCREEN_W,
        SCREEN_H,
        (void *)0x88000,
        512
    );

    sceGuDepthBuffer(
        (void *)0x110000,
        512
    );

    sceGuOffset(
        2048 - SCREEN_W / 2,
        2048 - SCREEN_H / 2
    );

    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuShadeModel(GU_SMOOTH);
    sceGuClearColor(BACKGROUND);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceGuDisplay(GU_TRUE);
}

/* =====================================================
   LOOP PRINCIPAL
   ===================================================== */

int main(void) {
    SceCtrlData pad;

    unsigned int oldButtons = 0;

    float cameraX = 0.0f;
    float cameraY = 0.0f;

    memset(&player, 0, sizeof(player));

    player.x = 48.0f;
    player.y = 190.0f;
    player.facing = 1;
    player.grounded = 0;
    player.animState = ANIM_IDLE;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    graphicsInit();

    while (1) {
        unsigned int buttons;
        unsigned int pressed;
        float dt = 1.0f / 60.0f;

        sceCtrlReadBufferPositive(&pad, 1);

        buttons = pad.Buttons;
        pressed = buttons & ~oldButtons;

        if (pressed & PSP_CTRL_START)
            break;

        if ((pressed & PSP_CTRL_SQUARE) &&
            !player.attacking) {
            player.attacking = 1;
            player.attackTimer = ATTACK_DURATION;
        }

        playerUpdate(&player, dt, buttons, &pad);

        /* Magia coletavel */
        {
            int tx = (int)((player.x + 14.0f) / TILE);
            int ty = (int)((player.y + 18.0f) / TILE);

            if (currentZone == 0 &&
                tx >= 0 && tx < MAP_W &&
                ty >= 0 && ty < MAP_H &&
                maps[0][ty][tx] == 'M') {
                magicFound = 1;
            }
        }

        /* Transicao de zona provisoria pela saida */
        {
            int tx = (int)((player.x + 14.0f) / TILE);
            int ty = (int)((player.y + 18.0f) / TILE);

            if (tx >= 0 && tx < MAP_W &&
                ty >= 0 && ty < MAP_H &&
                maps[currentZone][ty][tx] == 'D') {
                if (currentZone < ZONE_COUNT - 1) {
                    currentZone++;
                    player.x = 48.0f;
                    player.y = 190.0f;
                    player.vx = 0.0f;
                    player.vy = 0.0f;
                }
            }
        }

        cameraX = player.x - 200.0f;
        if (cameraX < 0.0f)
            cameraX = 0.0f;

        if (cameraX > MAP_W * TILE - SCREEN_W)
            cameraX = MAP_W * TILE - SCREEN_W;

        cameraY = 0.0f;

        sceGuStart(GU_DIRECT, displayList);

        sceGuClearColor(BACKGROUND);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        drawMap(cameraX, cameraY);
        playerDraw(&player, cameraX, cameraY);

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        oldButtons = buttons;
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
