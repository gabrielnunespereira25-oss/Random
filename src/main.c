
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define MENU_COUNT 4

static const char *menu[MENU_COUNT] = {
    "JOGAR",
    "PERSONAGEM",
    "OPCOES",
    "SAIR"
};

static int selected = 0;
static int running = 1;

static void draw_menu(void)
{
    pspDebugScreenClear();

    pspDebugScreenSetXY(2, 2);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("RANDOM PSP\n");
    pspDebugScreenPrintf("--------------------------\n\n");

    for (int i = 0; i < MENU_COUNT; i++) {
        if (i == selected) {
            pspDebugScreenSetTextColor(0xFF00FFFF);
            pspDebugScreenPrintf(" > %s\n", menu[i]);
        } else {
            pspDebugScreenSetTextColor(0xFFFFFFFF);
            pspDebugScreenPrintf("   %s\n", menu[i]);
        }
    }

    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("\n");
    pspDebugScreenPrintf("CIMA / BAIXO: navegar\n");
    pspDebugScreenPrintf("X: selecionar\n");
    pspDebugScreenPrintf("O: sair\n");
}

int main(void)
{
    SceCtrlData pad;
    unsigned int oldButtons = 0;

    pspDebugScreenInit();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    draw_menu();

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int pressed =
            pad.Buttons & ~oldButtons;

        oldButtons = pad.Buttons;

        if (pressed & PSP_CTRL_UP) {
            selected--;

            if (selected < 0)
                selected = MENU_COUNT - 1;

            draw_menu();
        }

        if (pressed & PSP_CTRL_DOWN) {
            selected++;

            if (selected >= MENU_COUNT)
                selected = 0;

            draw_menu();
        }

        if (pressed & PSP_CTRL_CROSS) {
            if (selected == 3) {
                running = 0;
            } else {
                pspDebugScreenClear();
                pspDebugScreenSetXY(2, 2);
                pspDebugScreenSetTextColor(0xFFFFFFFF);

                pspDebugScreenPrintf("RANDOM PSP\n\n");

                if (selected == 0)
                    pspDebugScreenPrintf("JOGO\n");
                else if (selected == 1)
                    pspDebugScreenPrintf("PERSONAGEM\n");
                else
                    pspDebugScreenPrintf("OPCOES\n");

                pspDebugScreenPrintf("\n");
                pspDebugScreenPrintf(
                    "Pressione O para voltar.\n"
                );
            }
        }

        if (pressed & PSP_CTRL_CIRCLE) {
            if (selected != 3) {
                draw_menu();
            } else {
                running = 0;
            }
        }

        sceDisplayWaitVblankStart();
        sceKernelDelayThread(16000);
    }

    sceKernelExitGame();
    return 0;
}
