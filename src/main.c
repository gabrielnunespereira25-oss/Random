
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512
#define PI 3.14159265f

static unsigned int __attribute__((aligned(16))) list[262144];

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, vx, vy;
    int alive;
} Spider;

static float px = 65.0f;
static float py = 190.0f;
static float pvy = 0.0f;
static int grounded = 0;
static int facing = 1;
static int health = 5;
static int swordTimer = 0;
static int magicTimer = 0;
static float cameraX = 0.0f;
static Spider spider = {310.0f, 210.0f, -0.8f, 0.0f, 1};

static unsigned int color(unsigned char r,
                          unsigned char g,
                          unsigned char b) {
    return GU_RGBA(r, g, b, 255);
}

static void triangle(float x1, float y1,
                     float x2, float y2,
                     float x3, float y3,
                     unsigned int c) {
    Vertex v[3] = {
        {c, x1, y1, 0},
        {c, x2, y2, 0},
        {c, x3, y3, 0}
    };

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(0xFFFFFFFF);
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        3, NULL, v);
}

static void rect(float x, float y, float w, float h,
                 unsigned int c) {
    Vertex v[2] = {
        {c, x, y, 0},
        {c, x + w, y + h, 0}
    };

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(0xFFFFFFFF);
    sceGuDrawArray(GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, NULL, v);
}

static void circle(float cx, float cy, float r,
                   unsigned int c) {
    int i;

    for (i = 0; i < 16; i++) {
        float a1 = (float)i * 2.0f * PI / 16.0f;
        float a2 = (float)(i + 1) * 2.0f * PI / 16.0f;

        triangle(cx, cy,
                 cx + cosf(a1) * r,
                 cy + sinf(a1) * r,
                 cx + cosf(a2) * r,
                 cy + sinf(a2) * r, c);
    }
}

static void line(float x1, float y1, float x2, float y2,
                 unsigned int c) {
    Vertex v[2] = {
        {c, x1, y1, 0},
        {c, x2, y2, 0}
    };

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(0xFFFFFFFF);
    sceGuDrawArray(GU_LINES,
        GU_COLOR_8888 | GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, NULL, v);
}

static void initGraphics(void) {
    sceGuInit();

    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(GU_PSM_8888,
                    (void *)0, STRIDE);
    sceGuDispBuffer(SW, SH,
                    (void *)0x88000, STRIDE);
    sceGuDepthBuffer((void *)0x110000, STRIDE);

    sceGuOffset(2048 - (SW / 2),
                2048 - (SH / 2));
    sceGuViewport(2048, 2048, SW, SH);
    sceGuDepthRange(65535, 0);

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
}

static void drawPlayer(float x, float y) {
    /* Perna única */
    rect(x + 9, y + 27, 10, 13,
         color(35, 35, 45));

    /* Corpo triangular */
    triangle(x + 3, y + 9,
             x + 25, y + 9,
             x + 14, y + 31,
             color(30, 30, 40));

    /* Cabeça */
    circle(x + 14, y + 7, 8,
           color(225, 180, 140));

    /* Cabelo de fogo */
    circle(x + 7, y + 1, 5,
           color(255, 65, 20));
    circle(x + 13, y - 3, 6,
           color(255, 120, 15));
    circle(x + 19, y + 1, 5,
           color(255, 40, 15));
    circle(x + 12, y + 3, 4,
           color(255, 200, 25));

    /* Olho */
    rect(x + (facing > 0 ? 17 : 7),
         y + 6, 2, 2, color(220, 20, 30));

    /* Espada */
    if (swordTimer > 0) {
        float sx = x + (facing > 0 ? 22 : -10);
        line(sx, y + 19, sx + facing * 20,
             y + 3, color(220, 230, 245));
        line(sx - 2, y + 20, sx + 3,
             y + 15, color(180, 130, 65));
    }
}

static void drawSpider(float x, float y) {
    int i;

    /* Pernas */
    for (i = 0; i < 4; i++) {
        float lx = x - 7 + i * 5;

        line(lx, y + 2, lx - 8, y + 10,
             color(30, 100, 30));
        line(lx, y + 2, lx - 7, y - 5,
             color(30, 100, 30));

        line(lx + 4, y + 2, lx + 12, y + 10,
             color(30, 100, 30));
        line(lx + 4, y + 2, lx + 10, y - 5,
             color(30, 100, 30));
    }

    circle(x, y, 10, color(55, 155, 45));
    circle(x - 3, y - 2, 3, color(80, 200, 55));
    circle(x + 3, y - 2, 3, color(80, 200, 55));

    circle(x - 3, y, 1.5f, color(255, 30, 30));
    circle(x + 3, y, 1.5f, color(255, 30, 30));
}

static void drawWorld(void) {
    int i;

    /* Céu escuro */
    rect(0, 0, SW, SH, color(18, 20, 38));

    /* Fundo da caverna */
    for (i = 0; i < 8; i++) {
        float x = (float)(i * 85) -
                  fmodf(cameraX * 0.25f, 85.0f);

        triangle(x, 0, x + 50, 0,
                 x + 25, 80 + (i % 3) * 15,
                 color(38, 40, 65));
    }

    /* Chão */
    rect(0, 235, SW, 37, color(50, 42, 45));
    rect(0, 235, SW, 5, color(80, 125, 55));

    /* Pedras */
    for (i = 0; i < 10; i++) {
        float x = (float)(i * 60) -
                  fmodf(cameraX, 60.0f);

        rect(x, 246 + (i % 2) * 7, 22, 5,
             color(78, 67, 68));
    }

    /* Plataformas */
    rect(150 - cameraX, 195, 55, 8,
         color(90, 100, 75));
    rect(360 - cameraX, 175, 60, 8,
         color(90, 100, 75));
}

static void drawHUD(void) {
    int i;

    rect(8, 8, 100, 12, color(55, 25, 35));
    rect(10, 10, health * 18, 8,
         color(210, 40, 50));

    for (i = 0; i < 3; i++) {
        circle(125 + i * 15, 15, 4,
               color(255, 100 + i * 30, 25));
    }

    /* Título */
    rect(350, 8, 120, 20, color(20, 20, 32));
    rect(356, 14, 8, 8, color(255, 80, 20));
    rect(368, 14, 8, 8, color(255, 150, 20));
    rect(380, 14, 8, 8, color(255, 50, 20));
}

static void updateGame(void) {
    SceCtrlData pad;
    float oldY = py;

    sceCtrlReadBufferPositive(&pad, 1);

    if (pad.Buttons & PSP_CTRL_LEFT) {
        px -= 2.0f;
        facing = -1;
    }

    if (pad.Buttons & PSP_CTRL_RIGHT) {
        px += 2.0f;
        facing = 1;
    }

    if (pad.Buttons & PSP_CTRL_CROSS) {
        if (grounded) {
            pvy = -7.0f;
            grounded = 0;
        }
    }

    if (pad.Buttons & PSP_CTRL_SQUARE) {
        swordTimer = 10;
    }

    if (pad.Buttons & PSP_CTRL_TRIANGLE) {
        magicTimer = 12;
    }

    if (pad.Buttons & PSP_CTRL_RTRIGGER) {
        if (pad.Buttons & PSP_CTRL_RIGHT) px += 1.5f;
        if (pad.Buttons & PSP_CTRL_LEFT) px -= 1.5f;
    }

    pvy += 0.30f;
    py += pvy;

    if (py + 40 >= 235) {
        py = 195;
        pvy = 0;
        grounded = 1;
    }

    if (py < 0) {
        py = 0;
        pvy = 0;
    }

    if (px < 0) px = 0;
    if (px > 1000) px = 1000;

    cameraX = px - 100;
    if (cameraX < 0) cameraX = 0;

    /* Aranha persegue o jogador */
    if (spider.alive) {
        float worldSpiderX = spider.x;
        float dx = px - worldSpiderX;

        if (dx > 2) spider.x += 0.65f;
        if (dx < -2) spider.x -= 0.65f;

        spider.y = 220;

        /* Colisão com o jogador */
        if (fabsf(px - spider.x) < 23 &&
            fabsf(py - 195) < 35) {
            if (health > 0 && (pad.Buttons & PSP_CTRL_LTRIGGER)) {
                /* L não dá dano; serve como proteção simples */
            } else if (health > 0 && oldY == py) {
                health--;
                px -= facing * 8;
            }
        }

        /* Ataque de espada */
        if (swordTimer > 0 &&
            fabsf(px - spider.x) < 55 &&
            fabsf(py - 195) < 45) {
            spider.alive = 0;
        }
    }

    if (swordTimer > 0) swordTimer--;
    if (magicTimer > 0) magicTimer--;
}

static void drawGame(void) {
    float screenPlayerX = px - cameraX;
    float screenSpiderX = spider.x - cameraX;

    sceGuStart(GU_DIRECT, list);

    sceGuClearColor(color(18, 20, 38));
    sceGuClear(GU_COLOR_BUFFER_BIT |
               GU_DEPTH_BUFFER_BIT);

    drawWorld();

    if (spider.alive) {
        drawSpider(screenSpiderX, 220);
    }

    drawPlayer(screenPlayerX, py);

    if (magicTimer > 0) {
        circle(screenPlayerX + facing * 30,
               py + 15, 8, color(100, 80, 255));
        circle(screenPlayerX + facing * 30,
               py + 15, 4, color(220, 220, 255));
    }

    drawHUD();

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

int main(void) {
    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initGraphics();

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_START) {
            break;
        }

        updateGame();
        drawGame();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
