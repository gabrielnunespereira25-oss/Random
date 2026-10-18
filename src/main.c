#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspgum.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512
#define LIST_SIZE (64 * 1024)
#define WORLD_W 1200
#define WORLD_H 700
#define PLAYER_W 16
#define PLAYER_H 30
#define MAX_SHOTS 8

typedef struct {
    unsigned short color;
    short x, y, z;
} Vertex;

typedef struct {
    int x, y, w, h;
} Platform;

typedef struct {
    float x, y, vx;
    int active;
} Shot;

static unsigned int __attribute__((aligned(16))) displayList[LIST_SIZE / 4];

static int screen = 0; /* 0 = menu, 1 = intro, 2 = jogo */
static float playerX = 55.0f, playerY = 210.0f, velocityY = 0.0f;
static int grounded = 0, doubleJumpUsed = 0;
static int magicUnlocked = 0, magicTaken = 0;
static int cameraX = 0, cameraY = 0;
static const int magicX = 310, magicY = 190;
static int shotCooldown = 0;
static Shot shots[MAX_SHOTS];
static unsigned int previousButtons = 0;

static Platform platforms[] = {
    {0,245,180,35}, {205,245,100,35}, {330,220,95,20},
    {455,190,100,20}, {585,220,100,20}, {710,185,105,20},
    {835,150,100,20}, {955,180,110,20}, {1080,220,120,35},
    {120,180,55,16}, {255,145,70,16}, {405,120,65,16},
    {600,125,70,16}, {760,105,60,16}, {920,115,65,16},
    {1020,95,70,16}
};
#define PLATFORM_COUNT (sizeof(platforms) / sizeof(platforms[0]))

static unsigned short rgb(int r, int g, int b)
{
    return (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static int justPressed(unsigned int buttons, unsigned int mask)
{
    return (buttons & mask) && !(previousButtons & mask);
}

static void rectangle(int x, int y, int w, int h, unsigned short color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
    v[0].color = color;
    v[0].x = x; v[0].y = y; v[0].z = 0;
    v[1].color = color;
    v[1].x = x + w; v[1].y = y + h; v[1].z = 0;
    sceGuDrawArray(GU_SPRITES,
        GU_COLOR_5650 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        2, 0, v);
}

static void beginFrame(void)
{
    sceGuStart(GU_DIRECT, displayList);
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

static void endFrame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void initGraphics(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);
    sceGuDrawBuffer(GU_PSM_5650, (void *)0, STRIDE);
    sceGuDispBuffer(SW, SH, (void *)(STRIDE * SH * 2), STRIDE);
    sceGuOffset(2048 - (SW / 2), 2048 - (SH / 2));
    sceGuViewport(2048, 2048, SW, SH);
    sceGuDepthRange(0, 65535);
    sceGuScissor(0, 0, SW, SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* Fonte bitmap 5x7 desenhada inteiramente com retângulos em C.
   Letras A-Z, números 0-9, dois-pontos e hífen. */
static const unsigned char font5x7[36][5] = {
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
    {0x7F,0x20,0x18,0x20,0x7F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E}
};

static void drawText(int x, int y, const char *text, int scale,
                     unsigned short color)
{
    while (*text) {
        char c = *text++;
        int index = -1;
        int col, row;

        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c >= 'A' && c <= 'Z') index = c - 'A';
        else if (c >= '0' && c <= '9') index = 26 + c - '0';

        if (index >= 0) {
            for (col = 0; col < 5; ++col) {
                unsigned char bits = font5x7[index][col];
                for (row = 0; row < 7; ++row) {
                    if (bits & (1 << row))
                        rectangle(x + col * scale, y + row * scale,
                                  scale, scale, color);
                }
            }
        } else if (c == ':') {
            rectangle(x + 2 * scale, y + 1 * scale, scale, scale, color);
            rectangle(x + 2 * scale, y + 5 * scale, scale, scale, color);
        } else if (c == '-') {
            rectangle(x + scale, y + 3 * scale, 3 * scale, scale, color);
        }

        x += 6 * scale;
    }
}

static void worldRect(int x, int y, int w, int h, unsigned short color)
{
    rectangle(x - cameraX, y - cameraY, w, h, color);
}

static void drawPlayer(void)
{
    worldRect((int)playerX, (int)playerY, PLAYER_W, PLAYER_H,
              rgb(245,245,245));
    worldRect((int)playerX + 3, (int)playerY + 7, 3, 3, rgb(0,0,0));
    worldRect((int)playerX + 10, (int)playerY + 7, 3, 3, rgb(0,0,0));
}

static void drawCave(void)
{
    unsigned short stone = rgb(75,75,82);
    unsigned short edge = rgb(125,125,132);
    unsigned int i;

    for (i = 0; i < PLATFORM_COUNT; ++i) {
        worldRect(platforms[i].x, platforms[i].y,
                  platforms[i].w, platforms[i].h, stone);
        worldRect(platforms[i].x, platforms[i].y,
                  platforms[i].w, 3, edge);
    }

    /* Paredes e passagens que dão forma ao sistema de cavernas. */
    worldRect(0, 0, 12, 245, stone);
    worldRect(1188, 220, 12, 480, stone);
    worldRect(180, 245, 25, 12, edge);
    worldRect(425, 220, 30, 12, edge);
    worldRect(555, 190, 30, 12, edge);
}

static void drawMagicSymbol(void)
{
    unsigned short violet = rgb(180,100,255);
    worldRect(magicX, magicY, 4, 22, violet);
    worldRect(magicX - 9, magicY + 9, 22, 4, violet);
    worldRect(magicX - 5, magicY + 4, 14, 14, rgb(235,220,255));
    worldRect(magicX - 2, magicY + 7, 8, 8, violet);
}

static void drawShots(void)
{
    int i;
    for (i = 0; i < MAX_SHOTS; ++i)
        if (shots[i].active)
            worldRect((int)shots[i].x, (int)shots[i].y, 10, 5,
                      rgb(190,100,255));
}

static void fireMagic(void)
{
    int i;
    if (!magicUnlocked || shotCooldown > 0) return;

    for (i = 0; i < MAX_SHOTS; ++i) {
        if (!shots[i].active) {
            shots[i].x = playerX + PLAYER_W;
            shots[i].y = playerY + 12;
            shots[i].vx = 5.0f;
            shots[i].active = 1;
            shotCooldown = 15;
            break;
        }
    }
}

static void updateShots(void)
{
    int i;
    for (i = 0; i < MAX_SHOTS; ++i) {
        if (!shots[i].active) continue;
        shots[i].x += shots[i].vx;
        if (shots[i].x > WORLD_W) shots[i].active = 0;
    }
    if (shotCooldown > 0) --shotCooldown;
}

static void drawMenu(void)
{
    unsigned short white = rgb(245,245,245);
    unsigned short gray = rgb(125,125,135);
    unsigned short red = rgb(220,55,65);

    beginFrame();

    /* Moldura e detalhes do menu. */
    rectangle(24, 22, 3, 226, gray);
    rectangle(24, 22, 432, 3, gray);
    rectangle(429, 22, 3, 226, gray);
    rectangle(24, 245, 408, 3, gray);
    rectangle(62, 96, 356, 2, rgb(70,70,78));

    drawText(168, 42, "RANDOM", 3, white);
    drawText(204, 72, "BETA 02", 1, gray);

    rectangle(108, 121, 264, 28, rgb(35,35,42));
    rectangle(108, 121, 3, 28, red);
    drawText(130, 131, "NEW GAME", 2, white);
    drawText(130, 169, "CONTINUE", 2, gray);
    drawText(130, 207, "OPTIONS", 2, gray);

    drawText(286, 232, "X START", 1, white);

    endFrame();
}

static void drawIntro(void)
{
    beginFrame();
    drawText(178, 45, "RANDOM", 3, rgb(245,245,245));
    drawText(142, 100, "FIND THE MAGIC", 1, rgb(170,170,180));
    rectangle(150, 151, 180, 2, rgb(100,100,110));
    drawText(162, 180, "X TO BEGIN", 1, rgb(245,245,245));
    endFrame();
}

static void drawGame(void)
{
    beginFrame();
    drawCave();
    if (!magicTaken) drawMagicSymbol();
    drawShots();
    drawPlayer();

    /* HUD minimalista, sem depender de arquivos externos. */
    rectangle(8, 8, 120, 18, rgb(25,25,30));
    drawText(14, 13, magicUnlocked ? "MAGIC: ON" : "MAGIC: OFF",
             1, magicUnlocked ? rgb(200,140,255) : rgb(150,150,155));
    endFrame();
}

static void updateGame(unsigned int buttons)
{
    float oldY = playerY;
    int i;

    if (buttons & PSP_CTRL_LEFT) playerX -= 2.2f;
    if (buttons & PSP_CTRL_RIGHT) playerX += 2.2f;
    if (playerX < 0) playerX = 0;
    if (playerX > WORLD_W - PLAYER_W) playerX = WORLD_W - PLAYER_W;

    if (justPressed(buttons, PSP_CTRL_CROSS)) {
        if (grounded) {
            velocityY = -7.0f;
            grounded = 0;
            doubleJumpUsed = 0;
        } else if (!doubleJumpUsed) {
            velocityY = -6.5f;
            doubleJumpUsed = 1;
        }
    }

    if (justPressed(buttons, PSP_CTRL_CIRCLE)) fireMagic();

    velocityY += 0.32f;
    if (velocityY > 6.0f) velocityY = 6.0f;
    playerY += velocityY;
    grounded = 0;

    for (i = 0; i < (int)PLATFORM_COUNT; ++i) {
        Platform p = platforms[i];
        int overlapsX = playerX + PLAYER_W > p.x &&
                        playerX < p.x + p.w;
        if (overlapsX && velocityY >= 0 &&
            oldY + PLAYER_H <= p.y &&
            playerY + PLAYER_H >= p.y) {
            playerY = (float)(p.y - PLAYER_H);
            velocityY = 0;
            grounded = 1;
            doubleJumpUsed = 0;
        }
    }

    if (playerY > WORLD_H) {
        playerX = 55;
        playerY = 210;
        velocityY = 0;
    }

    if (!magicTaken &&
        playerX + PLAYER_W > magicX - 10 &&
        playerX < magicX + 12 &&
        playerY + PLAYER_H > magicY - 8 &&
        playerY < magicY + 24) {
        magicTaken = 1;
        magicUnlocked = 1;
    }

    updateShots();

    cameraX = (int)playerX - SW / 2;
    cameraY = (int)playerY - SH / 2;
    if (cameraX < 0) cameraX = 0;
    if (cameraY < 0) cameraY = 0;
    if (cameraX > WORLD_W - SW) cameraX = WORLD_W - SW;
    if (cameraY > WORLD_H - SH) cameraY = WORLD_H - SH;
}

int main(void)
{
    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    initGraphics();

    for (;;) {
        unsigned int buttons;
        sceCtrlReadBufferPositive(&pad, 1);
        buttons = pad.Buttons;

        if (buttons & PSP_CTRL_START) {
            screen = 0;
        } else if (screen == 0) {
            drawMenu();
            if (justPressed(buttons, PSP_CTRL_CROSS)) screen = 1;
        } else if (screen == 1) {
            drawIntro();
            if (justPressed(buttons, PSP_CTRL_CROSS)) screen = 2;
        } else {
            updateGame(buttons);
            drawGame();
        }

        previousButtons = buttons;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
