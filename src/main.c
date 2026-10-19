
/*
 * RANDOM
 * PSP / PSPSDK
 * Essencia Escura
 *
 * Controles:
 * Analógico / direcional: mover
 * X: pular
 * Quadrado: ataque
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

#define SW 480
#define SH 272
#define PI 3.14159265f

#define BLACK  0xFF08080C
#define DARK   0xFF191923
#define GRAY   0xFF626273
#define WHITE  0xFFF4F4FF
#define PURPLE 0xFF713B91
#define BLUE   0xFF382B20
#define RED    0xFF3535D5

#define MAP_W 100
#define TILE  24
#define FLOOR_Y 224.0f

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, vx, vy;
    float anim;
    float attack;
    int facing;
    int grounded;
} Player;

static Player p = {
    90.0f, FLOOR_Y - 40.0f,
    0, 0, 0, 0, 1, 1
};

static float cameraX = 0;
static float worldTime = 0;
static int running = 1;

static unsigned int __attribute__((aligned(16)))
list[262144];

static unsigned int __attribute__((aligned(16)))
fb0[512 * 272];

static unsigned int __attribute__((aligned(16)))
fb1[512 * 272];

static unsigned int __attribute__((aligned(16)))
zbuffer[512 * 272];

static void drawTriangle(
    float x1, float y1,
    float x2, float y2,
    float x3, float y3,
    unsigned int color
) {
    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 3);

    v[0].color = color;
    v[0].x = x1; v[0].y = y1; v[0].z = 0;

    v[1].color = color;
    v[1].x = x2; v[1].y = y2; v[1].z = 0;

    v[2].color = color;
    v[2].x = x3; v[2].y = y3; v[2].z = 0;

    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        3, 0, v
    );
}

static void drawRect(
    float x, float y,
    float w, float h,
    unsigned int color
) {
    drawTriangle(x, y, x + w, y, x, y + h, color);
    drawTriangle(
        x + w, y,
        x + w, y + h,
        x, y + h,
        color
    );
}

static void drawCircle(
    float cx, float cy,
    float r,
    unsigned int color
) {
    int i;
    const int n = 20;

    for (i = 0; i < n; i++) {
        float a = 2.0f * PI * i / n;
        float b = 2.0f * PI * (i + 1) / n;

        drawTriangle(
            cx, cy,
            cx + cosf(a) * r,
            cy + sinf(a) * r,
            cx + cosf(b) * r,
            cy + sinf(b) * r,
            color
        );
    }
}

static void drawLine(
    float x1, float y1,
    float x2, float y2,
    float width,
    unsigned int color
) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    float nx, ny;

    if (len < 0.001f)
        return;

    nx = -dy / len * width * 0.5f;
    ny =  dx / len * width * 0.5f;

    drawTriangle(
        x1 + nx, y1 + ny,
        x1 - nx, y1 - ny,
        x2 + nx, y2 + ny,
        color
    );

    drawTriangle(
        x1 - nx, y1 - ny,
        x2 - nx, y2 - ny,
        x2 - nx, y2 - ny,
        color
    );
}

static void drawBackground(void) {
    int i;

    drawRect(0, 0, SW, SH, BLACK);

    /* Lua */
    drawCircle(390, 48, 19, WHITE);
    drawCircle(398, 41, 17, BLACK);

    /* Montanhas distantes */
    for (i = 0; i < 7; i++) {
        float x = i * 100.0f - fmodf(cameraX * 0.15f, 100.0f);

        drawTriangle(
            x - 30, 185,
            x + 45, 70 + (i % 3) * 12,
            x + 125, 185,
            DARK
        );
    }

    /* Cristais no fundo */
    for (i = 0; i < 8; i++) {
        float x = i * 83.0f - fmodf(cameraX * 0.35f, 83.0f);

        drawTriangle(
            x, 190,
            x + 8, 166 + (i % 3) * 4,
            x + 16, 190,
            PURPLE
        );

        drawTriangle(
            x + 8, 166 + (i % 3) * 4,
            x + 8, 190,
            x + 16, 190,
            BLUE
        );
    }
}

static int isPlatform(float x, float y) {
    int tileX = (int)(x / TILE);
    int tileY = (int)(y / TILE);

    if (tileX < 0 || tileX >= MAP_W)
        return 1;

    /* Chao principal */
    if (tileY >= 9)
        return 1;

    /* Plataformas elevadas */
    if (tileY == 6 && tileX >= 8 && tileX <= 14)
        return 1;

    if (tileY == 7 && tileX >= 20 && tileX <= 26)
        return 1;

    if (tileY == 5 && tileX >= 33 && tileX <= 40)
        return 1;

    if (tileY == 7 && tileX >= 48 && tileX <= 55)
        return 1;

    if (tileY == 6 && tileX >= 66 && tileX <= 74)
        return 1;

    if (tileY == 5 && tileX >= 83 && tileX <= 90)
        return 1;

    return 0;
}

static void drawWorld(void) {
    int i;
    int start = (int)(cameraX / TILE) - 1;
    int end = start + SW / TILE + 3;

    for (i = start; i < end; i++) {
        float x = i * TILE - cameraX;

        if (i < 0 || i >= MAP_W)
            continue;

        /* Chao */
        drawRect(x, FLOOR_Y, TILE, SH - FLOOR_Y, DARK);
        drawRect(x, FLOOR_Y, TILE, 4, GRAY);

        /* Textura angular da pedra */
        if (i % 3 == 0) {
            drawTriangle(
                x + 3, FLOOR_Y + 9,
                x + 12, FLOOR_Y + 5,
                x + 20, FLOOR_Y + 13,
                BLACK
            );
        }

        /* Cristais decorativos */
        if (i % 11 == 4) {
            drawTriangle(
                x + 4, FLOOR_Y,
                x + 10, FLOOR_Y - 17,
                x + 16, FLOOR_Y,
                PURPLE
            );
        }
    }

    /* Plataformas */
    for (i = 0; i < MAP_W; i++) {
        int row;

        for (row = 4; row <= 7; row++) {
            if (!isPlatform(i * TILE + 2, row * TILE + 2))
                continue;

            if (row >= 9)
                continue;

            {
                float x = i * TILE - cameraX;
                float y = row * TILE;

                if (x < -TILE || x > SW)
                    continue;

                drawRect(x, y, TILE, TILE, DARK);
                drawRect(x, y, TILE, 3, GRAY);

                drawTriangle(
                    x + 3, y + 7,
                    x + 11, y + 4,
                    x + 18, y + 12,
                    BLACK
                );
            }
        }
    }

    /* Portais ao longo da fase */
    for (i = 1; i < 5; i++) {
        float wx = i * 430.0f;
        float x = wx - cameraX;

        if (x < -40 || x > SW + 40)
            continue;

        drawRect(x - 12, FLOOR_Y - 52, 24, 52, BLACK);
        drawRect(x - 8, FLOOR_Y - 48, 16, 48, PURPLE);
        drawRect(x - 4, FLOOR_Y - 43, 8, 43, DARK);
    }
}

static void drawFlame(float x, float y) {
    float sway = sinf(worldTime * 9.0f) * 3.0f;
    float h = 14.0f + sinf(worldTime * 12.0f) * 3.0f;

    drawTriangle(
        x - 7, y,
        x + 7, y,
        x + sway, y - h,
        WHITE
    );

    drawTriangle(
        x - 3, y - 2,
        x + 3, y - 2,
        x + sway * 0.5f, y - h * 0.6f,
        GRAY
    );
}

static void drawPlayer(void) {
    float x = p.x - cameraX;
    float y = p.y;
    float walk = sinf(p.anim * 10.0f) * 5.0f;
    float bob = 0.0f;
    float leg1 = 0.0f;
    float leg2 = 0.0f;

    if (p.grounded && fabsf(p.vx) > 5.0f) {
        bob = fabsf(sinf(p.anim * 10.0f)) * 2.0f;
        leg1 = walk;
        leg2 = -walk;
    } else if (!p.grounded) {
        leg1 = -4.0f;
        leg2 = 5.0f;
    }

    y += bob;

    /* Sombra */
    drawCircle(x, y + 2, 12, BLACK);

    /* Manto animado */
    drawTriangle(
        x - 7, y - 29,
        x + 7, y - 29,
        x - 14 + sinf(p.anim * 5.0f) * 4.0f, y - 4,
        DARK
    );

    drawTriangle(
        x + 7, y - 29,
        x + 12 + sinf(p.anim * 4.0f) * 3.0f, y - 5,
        x - 14 + sinf(p.anim * 5.0f) * 4.0f, y - 4,
        GRAY
    );

    /* Pernas */
    drawLine(
        x - 4, y - 13,
        x - 6 + leg1, y - 2,
        6, BLACK
    );

    drawLine(
        x + 4, y - 13,
        x + 6 + leg2, y - 2,
        6, BLACK
    );

    /* Pes */
    drawTriangle(
        x - 12 + leg1, y - 3,
        x - 3 + leg1, y - 3,
        x - 7 + leg1, y + 1,
        GRAY
    );

    drawTriangle(
        x + 3 + leg2, y - 3,
        x + 12 + leg2, y - 3,
        x + 7 + leg2, y + 1,
        GRAY
    );

    /* Corpo angular */
    drawTriangle(
        x - 9, y - 31,
        x + 9, y - 31,
        x - 6, y - 12,
        BLACK
    );

    drawTriangle(
        x + 9, y - 31,
        x + 7, y - 12,
        x - 6, y - 12,
        GRAY
    );

    /* Simbolo no peito */
    drawTriangle(
        x - 2, y - 27,
        x + 4, y - 23,
        x - 1, y - 18,
        WHITE
    );

    /* Bracos */
    drawLine(
        x - 7, y - 28,
        x - 12, y - 17 + walk * 0.3f,
        5, BLACK
    );

    drawLine(
        x + 7, y - 28,
        x + 12, y - 18 - walk * 0.3f,
        5, GRAY
    );

    /* Cabeca */
    drawCircle(x, y - 38, 8, BLACK);

    /* Chama */
    drawFlame(x, y - 44);

    /* Olho fino */
    if (p.facing > 0)
        drawRect(x + 2, y - 39, 5, 1.5f, WHITE);
    else
        drawRect(x - 7, y - 39, 5, 1.5f, WHITE);

    /* Ataque em meia-lua durante meio segundo */
    if (p.attack > 0.0f) {
        float progress = 1.0f - p.attack / 0.5f;
        float ax = x + p.facing * (15.0f + progress * 18.0f);
        float ay = y - 25.0f;

        drawTriangle(
            ax - 5, ay - 17,
            ax + p.facing * 18, ay,
            ax - 5, ay + 17,
            WHITE
        );

        drawTriangle(
            ax - 1, ay - 12,
            ax + p.facing * 12, ay,
            ax - 1, ay + 12,
            PURPLE
        );
    }
}

static void updatePlayer(float dt, SceCtrlData pad) {
    float speed = 145.0f;
    float move = 0.0f;

    if (pad.Buttons & PSP_CTRL_RTRIGGER)
        speed = 215.0f;

    if (pad.Buttons & PSP_CTRL_LEFT)
        move -= 1.0f;

    if (pad.Buttons & PSP_CTRL_RIGHT)
        move += 1.0f;

    if (pad.Lx < 110)
        move -= (110 - pad.Lx) / 110.0f;

    if (pad.Lx > 145)
        move += (pad.Lx - 145) / 110.0f;

    if (move < -1.0f) move = -1.0f;
    if (move >  1.0f) move =  1.0f;

    p.vx = move * speed;

    if (move > 0.1f) p.facing = 1;
    if (move < -0.1f) p.facing = -1;

    if ((pad.Buttons & PSP_CTRL_CROSS) && p.grounded) {
        p.vy = -340.0f;
        p.grounded = 0;
    }

    if ((pad.Buttons & PSP_CTRL_SQUARE) && p.attack <= 0.0f)
        p.attack = 0.5f;

    p.attack -= dt;
    if (p.attack < 0.0f)
        p.attack = 0.0f;

    p.vy += 900.0f * dt;

    p.x += p.vx * dt;
    p.y += p.vy * dt;

    if (p.y >= FLOOR_Y - 40.0f) {
        p.y = FLOOR_Y - 40.0f;
        p.vy = 0.0f;
        p.grounded = 1;
    } else {
        p.grounded = 0;
    }

    if (p.x < 12.0f)
        p.x = 12.0f;

    if (p.x > MAP_W * TILE - 12.0f)
        p.x = MAP_W * TILE - 12.0f;

    if (p.x - cameraX > SW * 0.65f)
        cameraX = p.x - SW * 0.65f;

    if (cameraX < 0)
        cameraX = 0;

    if (cameraX > MAP_W * TILE - SW)
        cameraX = MAP_W * TILE - SW;

    p.anim += dt;
}

static void graphicsInit(void) {
    sceGuInit();

    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        512
    );

    sceGuDispBuffer(
        SW, SH,
        (void *)0x88000,
        512
    );

    sceGuDepthBuffer(
        (void *)0x110000,
        512
    );

    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);

    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SW, SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuClearColor(BLACK);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

int main(void) {
    SceCtrlData pad;
    unsigned int lastButtons = 0;
    unsigned int now;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    graphicsInit();

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);

        now = pad.Buttons;

        if ((now & PSP_CTRL_START) &&
            !(lastButtons & PSP_CTRL_START)) {
            running = 0;
        }

        updatePlayer(1.0f / 60.0f, pad);
        worldTime += 1.0f / 60.0f;

        sceGuStart(GU_DIRECT, list);

        sceGuClearColor(BLACK);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        drawBackground();
        drawWorld();
        drawPlayer();

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        lastButtons = now;
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
