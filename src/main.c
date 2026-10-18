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
#define WORLD_W 1800
#define WORLD_H 560
#define PLAYER_W 25
#define PLAYER_H 38
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

static unsigned int __attribute__((aligned(16)))
displayList[LIST_SIZE / 4];

static int screen = 0;
static float playerX = 60.0f;
static float playerY = 372.0f;
static float velocityY = 0.0f;

static int grounded = 0;
static int doubleJumpUsed = 0;
static int magicUnlocked = 0;
static int magicTaken = 0;
static int cameraX = 0;
static int cameraY = 95;
static int shotCooldown = 0;

static const int magicX = 370;
static const int magicY = 370;

static Shot shots[MAX_SHOTS];
static unsigned int previousButtons = 0;

static Platform platforms[] = {
    {0,410,360,90},
    {390,410,360,90},
    {780,410,330,90},
    {1140,410,300,90},
    {1470,410,330,90},
    {540,370,70,40},
    {1030,380,65,30},
    {1370,370,65,40}
};

#define PLATFORM_COUNT \
    (sizeof(platforms) / sizeof(platforms[0]))

static unsigned short rgb(int r, int g, int b)
{
    return (unsigned short)(
        ((r >> 3) << 11) |
        ((g >> 2) << 5) |
        (b >> 3)
    );
}

#define BLACK rgb(0,0,0)
#define DARK  rgb(24,24,24)
#define GRAY  rgb(100,100,100)
#define LIGHT rgb(190,190,190)
#define WHITE rgb(250,250,250)

static int justPressed(
    unsigned int buttons,
    unsigned int mask)
{
    return (buttons & mask) &&
           !(previousButtons & mask);
}

static void rectangle(
    int x, int y, int w, int h,
    unsigned short color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex));

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_5650 |
        GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
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

    sceGuDrawBuffer(
        GU_PSM_5650, (void *)0, STRIDE);

    sceGuDispBuffer(
        SW, SH,
        (void *)(STRIDE * SH * 2),
        STRIDE);

    sceGuOffset(
        2048 - (SW / 2),
        2048 - (SH / 2));

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
/* Fonte bitmap 5x7. */
static const unsigned char font5x7[36][5] = {
    {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},
    {0x7F,0x20,0x18,0x20,0x7F},
    {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43},
    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}
};

static void drawText(
    int x, int y,
    const char *text,
    int scale,
    unsigned short color)
{
    while (*text) {
        char c = *text++;
        int index = -1;
        int col, row;

        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');

        if (c >= 'A' && c <= 'Z')
            index = c - 'A';
        else if (c >= '0' && c <= '9')
            index = 26 + c - '0';

        if (index >= 0) {
            for (col = 0; col < 5; ++col) {
                unsigned char bits =
                    font5x7[index][col];

                for (row = 0; row < 7; ++row) {
                    if (bits & (1 << row)) {
                        rectangle(
                            x + col * scale,
                            y + row * scale,
                            scale, scale, color);
                    }
                }
            }
        } else if (c == ':') {
            rectangle(x + 2*scale, y + scale,
                      scale, scale, color);
            rectangle(x + 2*scale, y + 5*scale,
                      scale, scale, color);
        } else if (c == '-') {
            rectangle(x + scale, y + 3*scale,
                      3*scale, scale, color);
        }

        x += 6 * scale;
    }
}

static void worldRect(
    int x, int y, int w, int h,
    unsigned short color)
{
    rectangle(
        x - cameraX, y - cameraY,
        w, h, color);
}

/* Humanoide geométrico original. */
static void drawAngularHumanoid(void)
{
    int x = (int)playerX;
    int y = (int)playerY;

    /* Cabeça angular */
    worldRect(x + 6, y, 12, 10, WHITE);
    worldRect(x + 4, y + 2, 2, 6, LIGHT);
    worldRect(x + 18, y + 2, 2, 6, LIGHT);

    /* Tronco */
    worldRect(x + 4, y + 10, 16, 14, WHITE);
    worldRect(x + 8, y + 13, 8, 2, BLACK);

    /* Braços */
    worldRect(x, y + 11, 5, 13, LIGHT);
    worldRect(x + 20, y + 11, 5, 13, LIGHT);

    /* Pernas */
    worldRect(x + 5, y + 24, 6, 13, WHITE);
    worldRect(x + 14, y + 24, 6, 13, WHITE);

    /* Rosto */
    worldRect(x + 9, y + 3, 2, 2, BLACK);
    worldRect(x + 14, y + 3, 2, 2, BLACK);
}

/* Caverna geométrica fechada, com teto e piso espessos. */
static void drawCave(void)
{
    int x;
    unsigned short rock = rgb(35,35,35);

    /* Teto contínuo */
    worldRect(0, 0, WORLD_W, 105, rock);
    worldRect(0, 105, WORLD_W, 5, WHITE);

    for (x = 0; x < WORLD_W; x += 96) {
        worldRect(x + 12, 110, 35, 3, GRAY);
        worldRect(x + 47, 110, 3, 18, GRAY);
    }

    /* Parede de entrada e parede final */
    worldRect(0, 100, 34, 410, rock);
    worldRect(0, 100, 34, 5, WHITE);

    worldRect(WORLD_W - 34, 100, 34, 410, rock);
    worldRect(WORLD_W - 34, 100, 34, 5, WHITE);

    /* Chão grosso */
    worldRect(0, 410, WORLD_W, 150, rock);
    worldRect(0, 410, WORLD_W, 5, WHITE);

    /* Textura angular no piso */
    for (x = 50; x < WORLD_W; x += 90) {
        worldRect(x, 430, 2, 30, GRAY);
        worldRect(x + 2, 460, 28, 2, GRAY);
        worldRect(x + 30, 462, 2, 22, GRAY);
    }

    /* Pilares geométricos no fundo */
    for (x = 170; x < WORLD_W; x += 360) {
        worldRect(x, 160, 8, 130, DARK);
        worldRect(x, 160, 42, 4, GRAY);
        worldRect(x + 38, 164, 4, 38, GRAY);
    }

    /* Plataformas opcionais e baixas */
    for (x = 0; x < (int)PLATFORM_COUNT; ++x) {
        Platform p = platforms
/* Símbolo mágico geométrico em preto e branco. */
static void drawMagicSymbol(void)
{
    worldRect(magicX, magicY - 10, 4, 24, WHITE);
    worldRect(magicX - 10, magicY, 24, 4, WHITE);
    worldRect(magicX - 5, magicY - 5, 14, 14, GRAY);
    worldRect(magicX - 2, magicY - 2, 8, 8, BLACK);
    worldRect(magicX - 1, magicY - 1, 6, 6, WHITE);
}

static void drawShots(void)
{
    int i;

    for (i = 0; i < MAX_SHOTS; ++i) {
        if (shots[i].active) {
            worldRect((int)shots[i].x,
                      (int)shots[i].y,
                      12, 4, WHITE);

            worldRect((int)shots[i].x + 3,
                      (int)shots[i].y - 3,
                      6, 10, GRAY);
        }
    }
}

static void fireMagic(void)
{
    int i;

    if (!magicUnlocked || shotCooldown > 0)
        return;

    for (i = 0; i < MAX_SHOTS; ++i) {
        if (!shots[i].active) {
            shots[i].x = playerX + PLAYER_W;
            shots[i].y = playerY + 16;
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
        if (!shots[i].active)
            continue;

        shots[i].x += shots[i].vx;

        if (shots[i].x > WORLD_W)
            shots[i].active = 0;
    }

    if (shotCooldown > 0)
        --shotCooldown;
}

static void drawMenu(void)
{
    beginFrame();

    /* Moldura geométrica */
    rectangle(24, 22, 3, 226, GRAY);
    rectangle(24, 22, 432, 3, GRAY);
    rectangle(429, 22, 3, 226, GRAY);
    rectangle(24, 245, 408, 3, GRAY);
    rectangle(62, 96, 356, 2, GRAY);

    drawText(168, 42, "RANDOM", 3, WHITE);
    drawText(204, 72, "BETA 03", 1, LIGHT);

    rectangle(108, 121, 264, 28, DARK);
    rectangle(108, 121, 3, 28, WHITE);

    drawText(130, 131, "NEW GAME", 2, WHITE);
    drawText(130, 169, "CONTINUE", 2, LIGHT);
    drawText(130, 207, "OPTIONS", 2, LIGHT);

    drawText(286, 232, "X START", 1, WHITE);

    endFrame();
}

static void drawIntro(void)
{
    beginFrame();

    drawText(178, 45, "RANDOM", 3, WHITE);
    drawText(142, 100, "FIND THE MAGIC", 1, LIGHT);

    rectangle(150, 151, 180, 2, GRAY);

    drawText(162, 180, "X TO BEGIN", 1, WHITE);

    endFrame();
}

static void drawGame(void)
{
    beginFrame();

    drawCave();

    if (!magicTaken)
        drawMagicSymbol();

    drawShots();
    drawAngularHumanoid();

    /* HUD monocromático */
    rectangle(8, 8, 120, 18, DARK);
    rectangle(8, 8, 120, 2, LIGHT);

    drawText(
        14, 13,
        magicUnlocked ? "MAGIC: ON" : "MAGIC: OFF",
        1,
        magicUnlocked ? WHITE : LIGHT
    );

    endFrame();
}

static void updateGame(unsigned int buttons)
{
    float oldY = playerY;
    int i;

    /* Movimento horizontal */
    if (buttons & PSP_CTRL_LEFT)
        playerX -= 2.2f;

    if (buttons & PSP_CTRL_RIGHT)
        playerX += 2.2f;

    if (playerX < 34)
        playerX = 34;

    if (playerX > WORLD_W - 34 - PLAYER_W)
        playerX = WORLD_W - 34 - PLAYER_W;

    /* Pulo e pulo duplo */
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

    /* Magia no botão CIRCLE */
    if (justPressed(buttons, PSP_CTRL_CIRCLE))
        fireMagic();

    /* Gravidade */
    velocityY += 0.32f;

    if (velocityY > 6.0f)
        velocityY = 6.0f;

    playerY += velocityY;
    grounded = 0;

    /* Colisão com plataformas */
    for (i = 0; i < (int)PLATFORM_COUNT; ++i) {
        Platform p = platforms[i];

        int overlapsX =
            playerX + PLAYER_W > p.x &&
            playerX < p.x + p.w;

        if (overlapsX &&
            velocityY >= 0 &&
            oldY + PLAYER_H <= p.y &&
            playerY + PLAYER_H >= p.y) {

            playerY = (float)(p.y - PLAYER_H);
            velocityY = 0;
            grounded = 1;
            doubleJumpUsed = 0;
        }
    }

    /* Colisão com o teto */
    if (playerY < 110) {
        playerY = 110;

        if (velocityY < 0)
            velocityY = 0;
    }

    /* Chão contínuo */
    if (playerY + PLAYER_H > 410) {
        playerY = 410 - PLAYER_H;
        velocityY = 0;
        grounded = 1;
        doubleJumpUsed = 0;
    }

    /* Coleta da magia */
    if (!magicTaken &&
        playerX + PLAYER_W > magicX - 10 &&
        playerX < magicX + 12 &&
        playerY + PLAYER_H > magicY - 12 &&
        playerY < magicY + 18) {

        magicTaken = 1;
        magicUnlocked = 1;
    }

    updateShots();

    /* Câmera horizontal mais distante e vertical estável */
    cameraX = (int)(playerX + PLAYER_W / 2) - SW / 2;

    if (cameraX < 0)
        cameraX = 0;

    if (cameraX > WORLD_W - SW)
        cameraX = WORLD_W - SW;

    cameraY = 95;
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

            if (justPressed(buttons, PSP_CTRL_CROSS))
                screen = 1;

        } else if (screen == 1) {
            drawIntro();

            if (justPressed(buttons, PSP_CTRL_CROSS)) {
                screen = 2;

                playerX = 60.0f;
                playerY = 372.0f;
                velocityY = 0.0f;
                grounded = 0;
                doubleJumpUsed = 0;
                cameraX = 0;
                cameraY = 95;
            }

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
