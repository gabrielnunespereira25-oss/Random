
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_WIDTH 512
#define LIST_SIZE (64 * 1024)

typedef enum {
    MENU,
    INTRO,
    GAME,
    OPTIONS
} Screen;

static Screen screen = MENU;
static int selected = 0;
static int playerX = 220;
static int playerY = 190;
static int velocityY = 0;
static int grounded = 1;
static int introStep = 0;
static unsigned int previousButtons = 0;

static unsigned int __attribute__((aligned(16))) list[LIST_SIZE / 4];

typedef struct {
    unsigned short color;
    short x, y, z;
} Vertex;

static int pressed(unsigned int buttons, unsigned int key)
{
    return (buttons & key) && !(previousButtons & key);
}

static unsigned short rgb(unsigned char r, unsigned char g,
                          unsigned char b)
{
    return (unsigned short)(((b >> 3) << 11) |
                            ((g >> 2) << 5) |
                            (r >> 3));
}

static void drawRect(int x, int y, int w, int h,
                     unsigned short color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDrawArray(GU_SPRITES,
                   GU_COLOR_5650 | GU_VERTEX_16BIT |
                   GU_TRANSFORM_2D,
                   2, 0, v);
}

static void beginFrame(void)
{
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(rgb(15, 18, 28));
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

    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_5650,
                    (void *)0, BUF_WIDTH);
    sceGuDispBuffer(SCREEN_W, SCREEN_H,
                    (void *)(BUF_WIDTH * SCREEN_H * 2),
                    BUF_WIDTH);
    sceGuOffset(2048 - SCREEN_W / 2,
                2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuDepthRange(65535, 0);
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

static void drawCharacter(int x, int y)
{
    unsigned short white = 0xFFFF;

    /* Cabeca quadrada, sem olhos */
    drawRect(x - 10, y - 52, 20, 20, white);

    /* Tronco reto */
    drawRect(x - 4, y - 32, 8, 29, white);

    /* Bracos retos e simetricos */
    drawRect(x - 19, y - 30, 8, 30, white);
    drawRect(x + 11, y - 30, 8, 30, white);

    /* Pernas retas e ligeiramente separadas */
    drawRect(x - 10, y - 4, 8, 25, white);
    drawRect(x + 2, y - 4, 8, 25, white);
}

static void drawMenu(void)
{
    pspDebugScreenInit();
    pspDebugScreenSetXY(18, 4);
    pspDebugScreenPrintf("RANDOM");

    pspDebugScreenSetXY(14, 6);
    pspDebugScreenPrintf("BETA 0.1");

    pspDebugScreenSetXY(14, 11);
    pspDebugScreenPrintf("%s NOVO JOGO",
        selected == 0 ? ">" : " ");

    pspDebugScreenSetXY(14, 12);
    pspDebugScreenPrintf("%s OPCOES",
        selected == 1 ? ">" : " ");

    pspDebugScreenSetXY(14, 13);
    pspDebugScreenPrintf("%s SAIR",
        selected == 2 ? ">" : " ");

    pspDebugScreenSetXY(8, 27);
    pspDebugScreenPrintf("D-PAD: mover | X: selecionar");
}

static void drawIntro(void)
{
    pspDebugScreenInit();

    pspDebugScreenSetXY(5, 6);
    pspDebugScreenPrintf("RANDOM");

    pspDebugScreenSetXY(5, 12);

    if (introStep == 0)
        pspDebugScreenPrintf("Onde estou?");
    else if (introStep == 1)
        pspDebugScreenPrintf("Tudo esta em silencio...");
    else
        pspDebugScreenPrintf("Preciso continuar.");

    pspDebugScreenSetXY(5, 26);
    pspDebugScreenPrintf("X: continuar");
}

static void drawGame(void)
{
    beginFrame();

    /* Chao */
    drawRect(0, 230, SCREEN_W, 5, rgb(100, 110, 130));

    /* Personagem RANDOM */
    drawCharacter(playerX, playerY);

    endFrame();

    pspDebugScreenInit();
    pspDebugScreenSetXY(2, 1);
    pspDebugScreenPrintf("RANDOM");

    pspDebugScreenSetXY(2, 32);
    pspDebugScreenPrintf("D-PAD: mover | X: pular | START: menu");
}

static void drawOptions(void)
{
    pspDebugScreenInit();

    pspDebugScreenSetXY(5, 6);
    pspDebugScreenPrintf("OPCOES");

    pspDebugScreenSetXY(5, 10);
    pspDebugScreenPrintf("RANDOM - Beta 0.1");

    pspDebugScreenSetXY(5, 26);
    pspDebugScreenPrintf("TRIANGULO: voltar");
}

int main(void)
{
    SceCtrlData pad;

    initGraphics();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        switch (screen) {
        case MENU:
            if (pressed(pad.Buttons, PSP_CTRL_UP))
                selected = (selected + 2) % 3;

            if (pressed(pad.Buttons, PSP_CTRL_DOWN))
                selected = (selected + 1) % 3;

            if (pressed(pad.Buttons, PSP_CTRL_CROSS)) {
                if (selected == 0) {
                    introStep = 0;
                    screen = INTRO;
                } else if (selected == 1) {
                    screen = OPTIONS;
                } else {
                    sceKernelExitGame();
                }
            }

            drawMenu();
            break;

        case INTRO:
            if (pressed(pad.Buttons, PSP_CTRL_CROSS)) {
                introStep++;

                if (introStep >= 3) {
                    playerX = 220;
                    playerY = 225;
                    velocityY = 0;
                    grounded = 1;
                    screen = GAME;
                }
            }

            drawIntro();
            break;

        case GAME:
            if (pad.Buttons & PSP_CTRL_LEFT)
                playerX -= 2;

            if (pad.Buttons & PSP_CTRL_RIGHT)
                playerX += 2;

            if (playerX < 20)
                playerX = 20;

            if (playerX > SCREEN_W - 20)
                playerX = SCREEN_W - 20;

            if (pressed(pad.Buttons, PSP_CTRL_CROSS) && grounded) {
                velocityY = -9;
                grounded = 0;
            }

            if (!grounded) {
                playerY += velocityY;
                velocityY++;

                if (playerY >= 225) {
                    playerY = 225;
                    velocityY = 0;
                    grounded = 1;
                }
            }

            if (pressed(pad.Buttons, PSP_CTRL_START))
                screen = MENU;

            drawGame();
            break;

        case OPTIONS:
            if (pressed(pad.Buttons, PSP_CTRL_TRIANGLE))
                screen = MENU;

            drawOptions();
            break;
        }

        previousButtons = pad.Buttons;
    }

    return 0;
}
