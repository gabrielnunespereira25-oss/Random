
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspdisplay.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512

static unsigned int __attribute__((aligned(16))) list[262144];

static int selected = 0;
static int running = 1;

static const char *menu[] = {
    "JOGAR",
    "PERSONAGEM",
    "OPCOES",
    "SAIR"
};

static void setup_gu(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H,
                    (void *)0x88000, BUF_W);
    sceGuDepthBuffer((void *)0x110000, BUF_W);

    sceGuOffset(2048 - SCREEN_W / 2,
                2048 - SCREEN_H / 2);
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

int main(void)
{
    SceCtrlData pad;

    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    setup_gu();

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_UP) {
            selected = (selected + 3) % 4;
            sceKernelDelayThread(180000);
        }

        if (pad.Buttons & PSP_CTRL_DOWN) {
            selected = (selected + 1) % 4;
            sceKernelDelayThread(180000);
        }

        if (pad.Buttons & PSP_CTRL_CROSS) {
            if (selected == 3) {
                running = 0;
            } else {
                sceKernelDelayThread(180000);
            }
        }

        if (pad.Buttons & PSP_CTRL_CIRCLE) {
            running = 0;
        }

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF101018);
        sceGuClear(GU_COLOR_BUFFER_BIT |
                   GU_DEPTH_BUFFER_BIT);
        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        pspDebugScreenSetXY(2, 2);
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        pspDebugScreenPrintf("RANDOM PSP\n\n");

        for (int i = 0; i < 4; i++) {
            if (i == selected)
                pspDebugScreenSetTextColor(0xFF00FFFF);
            else
                pspDebugScreenSetTextColor(0xFFFFFFFF);

            pspDebugScreenPrintf("%s %s\n",
                i == selected ? ">" : " ",
                menu[i]);
        }

        pspDebugScreenSetTextColor(0xFFFFFFFF);
        pspDebugScreenPrintf(
            "\nX: selecionar\nO: sair\n");
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
