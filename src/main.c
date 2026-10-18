
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define W 480
#define H 272
#define STRIDE 512
#define BUF_SIZE (64 * 1024)

typedef struct {
    unsigned short color;
    short x, y, z;
} Vertex;

static unsigned int __attribute__((aligned(16)))
    displayList[BUF_SIZE / 4];

static int screen = 0;
static int playerX = 60;
static int playerY = 220;
static int velocityY = 0;
static int grounded = 1;
static int checkpointX = 60;
static int checkpointActive = 0;
static unsigned int oldButtons = 0;

static unsigned short color(int r, int g, int b) {
    return ((r >> 3) << 11) |
           ((g >> 2) << 5) |
           (b >> 3);
}

static int justPressed(unsigned int buttons, unsigned int key) {
    return (buttons & key) && !(oldButtons & key);
}

static void rect(int x, int y, int w, int h,
                 unsigned short c) {
    Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));

    v[0].color = c;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].color = c;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_5650 | GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
        2, 0, v
    );
}

static void beginFrame(void) {
    sceGuStart(GU_DIRECT, displayList);
    sceGuClearColor(color(12, 14, 23));
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

static void endFrame(void) {
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void initGraphics(void) {
    sceGuInit();

    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(
        GU_PSM_5650, (void *)0, STRIDE
    );

    sceGuDispBuffer(
        W, H, (void *)(STRIDE * H * 2), STRIDE
    );

    sceGuOffset(2048 - W / 2, 2048 - H / 2);
    sceGuViewport(2048, 2048, W, H);
    sceGuScissor(0, 0, W, H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void drawCharacter(int x, int y) {
    unsigned short white = 0xFFFF;

    /* Cabeça quadrada */
    rect(x - 8, y - 40, 16, 16, white);

    /* Tronco reto */
    rect(x - 3, y - 24, 6, 20, white);

    /* Braços retos */
    rect(x - 15, y - 23, 5, 19, white);
    rect(x + 10, y - 23, 5, 19, white);

    /* Pernas separadas */
    rect(x - 8, y - 4, 6, 15, white);
    rect(x + 2, y - 4, 6, 15, white);
}

static void drawMenu(void) {
    beginFrame();

    rect(0, 0, W, 4, color(255, 255, 255));

    /* Logo geométrico */
    rect(216, 55, 48, 48, color(255, 255, 255));
    rect(226, 65, 28, 28, color(12, 14, 23));

    /* Opções visuais */
    rect(140, 135, 200, 8,
         color(255, 255, 255));
    rect(140, 160, 150, 5,
         color(110, 120, 140));
    rect(140, 180, 120, 5,
         color(110, 120, 140));

    endFrame();
}

static void drawIntro(void) {
    beginFrame();

    rect(0, 235, W, 5, color(80, 90, 110));
    drawCharacter(240, 220);

    endFrame();
}

static void drawGame(void) {
    beginFrame();

    /* Plataforma */
    rect(0, 240, W, 32, color(45, 50, 65));
    rect(0, 238, W, 3, color(160, 170, 190));

    /* Checkpoint */
    rect(checkpointX, 192, 4, 46,
         checkpointActive
            ? color(255, 255, 255)
            : color(90, 100, 120));

    rect(checkpointX - 8, 192, 20, 4,
         checkpointActive
            ? color(255, 255, 255)
            : color(90, 100, 120));

    drawCharacter(playerX, playerY);

    endFrame();
}

int main(void) {
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

        } else if (screen == 1) {
            drawIntro();

            if (justPressed(pad.Buttons, PSP_CTRL_CROSS))
                screen = 2;

        } else {
            if (pad.Buttons & PSP_CTRL_LEFT)
                playerX -= 2;

            if (pad.Buttons & PSP_CTRL_RIGHT)
                playerX += 2;

            if (playerX < 16) playerX = 16;
            if (playerX > W - 16) playerX = W - 16;

            if (justPressed(pad.Buttons, PSP_CTRL_CROSS)
                && grounded) {
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
                checkpointX = playerX;
            }

            if (justPressed(pad.Buttons, PSP_CTRL_START)) {
                screen = 0;
            }

            drawGame();
        }

        oldButtons = pad.Buttons;
    }

    return 0;
}
