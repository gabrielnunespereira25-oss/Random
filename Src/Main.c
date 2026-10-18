
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define W 480
#define H 272
#define BUF_WIDTH 512
#define PIXEL_SIZE 4
#define FRAME_SIZE (BUF_WIDTH * H * PIXEL_SIZE)

static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned int __attribute__((aligned(16))) framebuffer[BUF_WIDTH * H];

typedef struct {
    float x, y, w, h;
    unsigned int color;
} Rect;

static int screen = 0;
static int selected = 0;
static float playerX = 70;
static float playerY = 210;
static float velocityY = 0;
static int onGround = 1;
static int introTimer = 0;

static unsigned int rgba(
    unsigned char r, unsigned char g,
    unsigned char b, unsigned char a
) {
    return (a << 24) | (b << 16) | (g << 8) | r;
}

static void rect(float x, float y, float w, float h,
                 unsigned int color) {
    sceGuColor(color);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, BUF_WIDTH);
    sceGuFinish();
    sceGuSync(0, 0);

    typedef struct {
        short x, y, z;
    } Vertex;

    Vertex *v = (Vertex*)sceGuGetMemory(2 * sizeof(Vertex));
    v[0].x = (short)x;
    v[0].y = (short)y;
    v[0].z = 0;
    v[1].x = (short)(x + w);
    v[1].y = (short)(y + h);
    v[1].z = 0;

    sceGuDrawArray(GU_SPRITES,
        GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        2, 0, v);
}

static void text(int x, int y, const char *str,
                 unsigned int color) {
    /* Texto temporariamente representado por blocos.
       Na próxima etapa, podemos integrar uma fonte bitmap. */
    int cx = x;
    while (*str) {
        if (*str != ' ')
            rect(cx, y, 5, 8, color);
        cx += 8;
        str++;
    }
}

static void beginFrame(void) {
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(rgba(12, 12, 12, 255));
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

static void endFrame(void) {
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void drawPlayer(void) {
    unsigned int white = rgba(255, 255, 255, 255);

    rect(playerX, playerY, 12, 18, white);
    rect(playerX + 2, playerY + 4, 3, 3,
         rgba(12, 12, 12, 255));
    rect(playerX + 7, playerY + 4, 3, 3,
         rgba(12, 12, 12, 255));
}

static void drawTitle(void) {
    text(188, 65, "RANDOM", rgba(255, 255, 255, 255));

    const char *items[] = {
        "NOVO JOGO",
        "CONTINUAR",
        "OPCOES"
    };

    for (int i = 0; i < 3; i++) {
        unsigned int c = (i == selected)
            ? rgba(255, 255, 255, 255)
            : rgba(100, 100, 100, 255);

        if (i == selected)
            rect(145, 111 + i * 28, 5, 10, c);

        text(160, 112 + i * 28, items[i], c);
    }

    text(145, 235, "X: CONFIRMAR",
         rgba(130, 130, 130, 255));
}

static void drawIntro(void) {
    rect(0, 230, W, 42, rgba(255, 255, 255, 255));
    drawPlayer();

    text(20, 20, "DIA 0",
         rgba(255, 255, 255, 255));

    if (introTimer < 180) {
        text(130, 80, "ONDE ESTOU?",
             rgba(255, 255, 255, 255));
    } else {
        text(100, 80, "EXPLORE O CAMINHO",
             rgba(255, 255, 255, 255));
    }
}

static void updateMenu(SceCtrlData pad) {
    if ((pad.Buttons & PSP_CTRL_DOWN) &&
        !(pad.Buttons & PSP_CTRL_DOWN)) {
        selected = (selected + 1) % 3;
    }

    if (pad.Buttons & PSP_CTRL_CROSS) {
        if (selected == 0) {
            screen = 1;
            introTimer = 0;
        } else if (selected == 1) {
            screen = 2;
        } else {
            screen = 3;
        }
    }
}

static void updateGame(SceCtrlData pad) {
    if (pad.Buttons & PSP_CTRL_LEFT)
        playerX -= 2.0f;

    if (pad.Buttons & PSP_CTRL_RIGHT)
        playerX += 2.0f;

    if ((pad.Buttons & PSP_CTRL_CROSS) && onGround) {
        velocityY = -6.5f;
        onGround = 0;
    }

    velocityY += 0.25f;
    playerY += velocityY;

    if (playerY > 212) {
        playerY = 212;
        velocityY = 0;
        onGround = 1;
    }

    if (playerX < 0) playerX = 0;
    if (playerX > W - 12) playerX = W - 12;
}

int main(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();

    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888,
                    (void*)0, BUF_WIDTH);
    sceGuDispBuffer(W, H,
                    (void*)0x88000, BUF_WIDTH);
    sceGuOffset(2048 - W / 2, 2048 - H / 2);
    sceGuViewport(2048, 2048, W, H);
    sceGuDepthRange(65535, 0);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    while (1) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_START) {
            if (screen == 1)
                screen = 2;
        }

        beginFrame();

        if (screen == 0) {
            if (pad.Buttons & PSP_CTRL_UP)
                selected = (selected + 2) % 3;
            if (pad.Buttons & PSP_CTRL_DOWN)
                selected = (selected + 1) % 3;

            if (pad.Buttons & PSP_CTRL_CROSS) {
                screen = (selected == 2) ? 3 : 1;
            }

            drawTitle();
        } else if (screen == 1) {
            drawIntro();
            introTimer++;

            if (introTimer > 240)
                screen = 2;
        } else if (screen == 2) {
            updateGame(pad);
            rect(0, 230, W, 42,
                 rgba(255, 255, 255, 255));
            drawPlayer();
            text(10, 10, "RANDOM",
                 rgba(255, 255, 255, 255));
        } else {
            text(160, 80, "OPCOES",
                 rgba(255, 255, 255, 255));
            text(100, 120, "TRIANGULO: VOLTAR",
                 rgba(200, 200, 200, 255));

            if (pad.Buttons & PSP_CTRL_TRIANGLE)
                screen = 0;
        }

        endFrame();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
