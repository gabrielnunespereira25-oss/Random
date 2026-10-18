
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspgum.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512
#define LIST_BYTES (64 * 1024)

typedef struct {
    unsigned short color;
    short x, y, z;
} Vertex;

static unsigned int __attribute__((aligned(16)))
    displayList[LIST_BYTES / 4];

static int screen = 0;
static int playerX = 60;
static int playerY = 220;
static int velocityY = 0;
static int grounded = 1;

static const int checkpointX = 300;
static int checkpointActive = 0;

static unsigned int previousButtons = 0;

static unsigned short rgb(int r, int g, int b)
{
    return (unsigned short)(
        ((r >> 3) << 11) |
        ((g >> 2) << 5) |
        (b >> 3)
    );
}

static int justPressed(unsigned int buttons, unsigned int key)
{
    return (buttons & key) && !(previousButtons & key);
}

static void rectangle(int x, int y, int w, int h,
                      unsigned short c)
{
    Vertex *v = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex)
    );

    v[0].color = c;
    v[0].x = (short)x;
    v[0].y = (short)y;
    v[0].z = 0;

    v[1].color = c;
    v[1].x = (short)(x + w);
    v[1].y = (short)(y + h);
    v[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_5650 |
        GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
        2,
        NULL,
        v
    );
}

static void beginFrame(void)
{
    sceGuStart(GU_DIRECT, displayList);

    /* Fundo preto puro */
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
        GU_PSM_5650,
        (void *)0,
        BUF_W
    );

    sceGuDispBuffer(
        SCREEN_W,
        SCREEN_H,
        (void *)(BUF_W * SCREEN_H * 2),
        BUF_W
    );

    sceGuOffset(
        2048 - SCREEN_W / 2,
        2048 - SCREEN_H / 2
    );

    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void drawPlayer(int x, int y)
{
    const unsigned short white = 0xFFFF;
    const unsigned short black = 0x0000;

    /* Cabeça branca */
    rectangle(x - 8, y - 38, 16, 14, white);

    /* Dois olhos pretos */
    rectangle(x - 5, y - 33, 3, 3, black);
    rectangle(x + 2, y - 33, 3, 3, black);

    /* Corpo */
    rectangle(x - 4, y - 24, 8, 17, white);

    /* Braços */
    rectangle(x - 12, y - 22, 8, 4, white);
    rectangle(x + 4, y - 22, 8, 4, white);

    /* Pernas */
    rectangle(x - 7, y - 7, 5, 13, white);
    rectangle(x + 2, y - 7, 5, 13, white);
}

static void drawMenu(void)
{
    beginFrame();

    /* Símbolo do RANDOM */
    rectangle(216, 55, 48, 48, 0xFFFF);
    rectangle(226, 65, 28, 28, 0x0000);

    /* Elementos decorativos */
    rectangle(140, 135, 200, 5, 0xFFFF);
    rectangle(165, 155, 150, 3, 0x8410);

    endFrame();
}

static void drawIntro(void)
{
    beginFrame();

    rectangle(0, 238, SCREEN_W, 34, 0x4208);
    rectangle(0, 235, SCREEN_W, 3, 0xBDF7);

    drawPlayer(240, 220);

    endFrame();
}

static void drawGame(void)
{
    beginFrame();

    /* Chão */
    rectangle(0, 238, SCREEN_W, 34, 0x4208);
    rectangle(0, 235, SCREEN_W, 3, 0xBDF7);

    /* Checkpoint parado */
    rectangle(
        checkpointX,
        193,
        4,
        42,
        checkpointActive ? 0xFFFF : 0x8410
    );

    rectangle(
        checkpointX - 5,
        193,
        14,
        3,
        checkpointActive ? 0xFFFF : 0x8410
    );

    drawPlayer(playerX, playerY);

    endFrame();
}

static void updateGame(unsigned int buttons)
{
    if (buttons & PSP_CTRL_LEFT)
        playerX -= 2;

    if (buttons & PSP_CTRL_RIGHT)
        playerX += 2;

    if (playerX < 12)
        playerX = 12;

    if (playerX > SCREEN_W - 12)
        playerX = SCREEN_W - 12;

    if (justPressed(buttons, PSP_CTRL_CROSS) && grounded) {
        velocityY = -9;
        grounded = 0;
    }

    if (!grounded) {
        playerY += velocityY;
        velocityY++;

        if (playerY >= 220) {
            playerY = 220;
            velocityY = 0;
            grounded = 1;
        }
    }

    if (playerX >= checkpointX - 12 &&
        playerX <= checkpointX + 12) {
        checkpointActive = 1;
    }
}

int main(void)
{
    SceCtrlData pad;

    initGraphics();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (screen == 0) {
            drawMenu();

            if (justPressed(pad.Buttons, PSP_CTRL_CROSS))
                screen = 1;
        }
        else if (screen == 1) {
            drawIntro();

            if (justPressed(pad.Buttons, PSP_CTRL_CROSS))
                screen = 2;
        }
        else {
            updateGame(pad.Buttons);

            if (justPressed(pad.Buttons, PSP_CTRL_START)) {
                screen = 0;
            }

            drawGame();
        }

        previousButtons = pad.Buttons;
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
