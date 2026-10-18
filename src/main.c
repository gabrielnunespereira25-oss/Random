
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <psputils.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define STRIDE 512

static unsigned int __attribute__((aligned(16))) displayList[262144];

static void initGraphics(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        STRIDE
    );

    sceGuDispBuffer(
        SCREEN_W,
        SCREEN_H,
        (void *)0x88000,
        STRIDE
    );

    sceGuOffset(
        2048 - SCREEN_W / 2,
        2048 - SCREEN_H / 2
    );

    sceGuViewport(
        2048,
        2048,
        SCREEN_W,
        SCREEN_H
    );

    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuClearColor(0xFF17121D);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void rect(
    float x,
    float y,
    float w,
    float h,
    unsigned int color
)
{
    typedef struct {
        unsigned int color;
        float x, y, z;
    } Vertex;

    if (w <= 0.0f || h <= 0.0f)
        return;

    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 2);

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF,
        2,
        0,
        v
    );
}

int main(void)
{
    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initGraphics();

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        sceGuStart(GU_DIRECT, displayList);

        sceGuClearColor(0xFF17121D);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        /* Fundo de teste. */
        rect(0, 0, SCREEN_W, SCREEN_H, 0xFF17121D);

        /* Chão. */
        rect(0, 232, SCREEN_W, 40, 0xFF403443);
        rect(0, 232, SCREEN_W, 4, 0xFF62516A);

        /* Personagem de teste. */
        rect(100, 190, 18, 30, 0xFF25213D);
        rect(104, 181, 11, 11, 0xFFB7A7C6);
        rect(112, 185, 3, 2, 0xFFFFD080);

        /* Indicador colorido para confirmar a renderização. */
        rect(20, 20, 40, 12, 0xFFFF8040);
        rect(70, 20, 40, 12, 0xFFFFD080);
        rect(120, 20, 40, 12, 0xFFFFFFFF);

        /* D-pad movimenta o quadrado de teste. */
        static float px = 220.0f;
        static float py = 190.0f;

        if (pad.Buttons & PSP_CTRL_LEFT)
            px -= 2.0f;

        if (pad.Buttons & PSP_CTRL_RIGHT)
            px += 2.0f;

        if (pad.Buttons & PSP_CTRL_UP)
            py -= 2.0f;

        if (pad.Buttons & PSP_CTRL_DOWN)
            py += 2.0f;

        if (px < 0) px = 0;
        if (px > SCREEN_W - 18) px = SCREEN_W - 18;
        if (py < 0) py = 0;
        if (py > SCREEN_H - 30) py = SCREEN_H - 30;

        rect(px, py, 18, 30, 0xFF50C060);

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
