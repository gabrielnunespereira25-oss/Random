
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

typedef enum {
    MENU,
    INTRO,
    GAME,
    OPTIONS
} Screen;

static Screen screen = MENU;
static int selected = 0;
static int playerX = 10;
static int playerY = 20;
static int velocityY = 0;
static int grounded = 1;
static int introStep = 0;
static unsigned int previousButtons = 0;

static int pressed(unsigned int buttons, unsigned int key)
{
    return (buttons & key) && !(previousButtons & key);
}

static void drawMenu(void)
{
    pspDebugScreenClear();

    pspDebugScreenSetXY(15, 3);
    pspDebugScreenPrintf("RANDOM");

    pspDebugScreenSetXY(9, 5);
    pspDebugScreenPrintf("BETA 0.1 - PSP");

    pspDebugScreenSetXY(10, 9);
    pspDebugScreenPrintf("%s NOVO JOGO",
        selected == 0 ? ">" : " ");

    pspDebugScreenSetXY(10, 10);
    pspDebugScreenPrintf("%s OPCOES",
        selected == 1 ? ">" : " ");

    pspDebugScreenSetXY(10, 11);
    pspDebugScreenPrintf("%s SAIR",
        selected == 2 ? ">" : " ");

    pspDebugScreenSetXY(4, 26);
    pspDebugScreenPrintf("D-PAD: mover | X: confirmar");
}

static void drawIntro(void)
{
    pspDebugScreenClear();

    pspDebugScreenSetXY(2, 5);
    pspDebugScreenPrintf("RANDOM");

    pspDebugScreenSetXY(2, 10);

    if (introStep == 0)
        pspDebugScreenPrintf("Onde estou?");
    else if (introStep == 1)
        pspDebugScreenPrintf("Tudo esta em silencio...");
    else
        pspDebugScreenPrintf("Preciso continuar.");

    pspDebugScreenSetXY(2, 26);
    pspDebugScreenPrintf("Pressione X para continuar");
}

static void drawGame(void)
{
    pspDebugScreenClear();

    pspDebugScreenSetXY(2, 2);
    pspDebugScreenPrintf("RANDOM - BETA");

    pspDebugScreenSetXY(playerX, playerY);
    pspDebugScreenPrintf("o");

    pspDebugScreenSetXY(0, 22);
    pspDebugScreenPrintf(
        "================================================"
    );

    pspDebugScreenSetXY(2, 26);
    pspDebugScreenPrintf("D-PAD: mover | X: pular | START: menu");
}

static void drawOptions(void)
{
    pspDebugScreenClear();

    pspDebugScreenSetXY(5, 5);
    pspDebugScreenPrintf("OPCOES");

    pspDebugScreenSetXY(2, 9);
    pspDebugScreenPrintf("RANDOM - Beta 0.1");

    pspDebugScreenSetXY(2, 26);
    pspDebugScreenPrintf("TRIANGULO: voltar");
}

int main(void)
{
    SceCtrlData pad;

    pspDebugScreenInit();
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
                    playerX = 10;
                    playerY = 20;
                    velocityY = 0;
                    grounded = 1;
                    screen = GAME;
                }
            }

            drawIntro();
            break;

        case GAME:
            if (pad.Buttons & PSP_CTRL_LEFT)
                playerX--;

            if (pad.Buttons & PSP_CTRL_RIGHT)
                playerX++;

            if (playerX < 0)
                playerX = 0;

            if (playerX > 47)
                playerX = 47;

            if (pressed(pad.Buttons, PSP_CTRL_CROSS) && grounded) {
                velocityY = -3;
                grounded = 0;
            }

            if (!grounded) {
                playerY += velocityY;
                velocityY++;

                if (playerY >= 20) {
                    playerY = 20;
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
        sceDisplayWaitVblankStart();
    }

    return 0;
}
