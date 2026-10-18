
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

enum {
    MENU,
    INTRO,
    GAME,
    OPTIONS
};

static int screen = MENU;
static int selected = 0;
static int introStep = 0;
static int introFrames = 0;
static int playerX = 8;
static int playerY = 10;
static int jumping = 0;
static int jumpVelocity = 0;
static int previousButtons = 0;

static int pressed(int buttons, int key) {
    return (buttons & key) && !(previousButtons & key);
}

static void waitFrame(void) {
    sceDisplayWaitVblankStart();
}

static void drawMenu(void) {
    pspDebugScreenClear();

    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenSetXY(15, 3);
    pspDebugScreenPrintf("R A N D O M");

    pspDebugScreenSetTextColor(0xFFAAAAAA);
    pspDebugScreenSetXY(10, 5);
    pspDebugScreenPrintf("BETA 0.1");

    const char *items[] = {
        "NOVO JOGO",
        "CONTINUAR",
        "OPCOES"
    };

    for (int i = 0; i < 3; i++) {
        pspDebugScreenSetXY(12, 10 + i * 2);

        if (i == selected) {
            pspDebugScreenSetTextColor(0xFFFFFFFF);
            pspDebugScreenPrintf("> %s", items[i]);
        } else {
            pspDebugScreenSetTextColor(0xFF888888);
            pspDebugScreenPrintf("  %s", items[i]);
        }
    }

    pspDebugScreenSetTextColor(0xFFAAAAAA);
    pspDebugScreenSetXY(5, 25);
    pspDebugScreenPrintf("CIMA/BAIXO: NAVEGAR");
    pspDebugScreenSetXY(5, 26);
    pspDebugScreenPrintf("X: CONFIRMAR");
}

static void drawIntro(void) {
    pspDebugScreenClear();

    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenSetXY(2, 3);
    pspDebugScreenPrintf("RANDOM");

    pspDebugScreenSetXY(2, 8);

    if (introStep == 0) {
        pspDebugScreenPrintf("...");
    } else if (introStep == 1) {
        pspDebugScreenPrintf("Onde estou?");
    } else {
        pspDebugScreenPrintf("Preciso continuar.");
    }

    pspDebugScreenSetTextColor(0xFFAAAAAA);
    pspDebugScreenSetXY(2, 25);
    pspDebugScreenPrintf("X: CONTINUAR");
}

static void drawGame(void) {
    pspDebugScreenClear();

    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenSetXY(2, 2);
    pspDebugScreenPrintf("RANDOM | EXPLORACAO");

    pspDebugScreenSetXY(2, 6);
    pspDebugScreenPrintf("________________________________");

    pspDebugScreenSetXY(playerX, playerY);
    pspDebugScreenPrintf("[]");

    pspDebugScreenSetXY(2, 20);
    pspDebugScreenPrintf("________________________________");

    pspDebugScreenSetTextColor(0xFFAAAAAA);
    pspDebugScreenSetXY(2, 25);
    pspDebugScreenPrintf("ANALOG/D-PAD: MOVER");
    pspDebugScreenSetXY(2, 26);
    pspDebugScreenPrintf("X: SALTAR | START: MENU");
}

static void updateGame(SceCtrlData pad) {
    if (pad.Buttons & PSP_CTRL_LEFT) {
        if (playerX > 2) playerX--;
    }

    if (pad.Buttons & PSP_CTRL_RIGHT) {
        if (playerX < 28) playerX++;
    }

    if (pressed(pad.Buttons, PSP_CTRL_CROSS) && !jumping) {
        jumping = 1;
        jumpVelocity = -4;
    }

    if (jumping) {
        playerY += jumpVelocity;
        jumpVelocity++;

        if (playerY >= 18) {
            playerY = 18;
            jumping = 0;
            jumpVelocity = 0;
        }

        if (playerY < 6) {
            playerY = 6;
            jumpVelocity = 1;
        }
    }

    if (pressed(pad.Buttons, PSP_CTRL_START)) {
        screen = MENU;
    }
}

int main(void) {
    SceCtrlData pad;

    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        switch (screen) {
        case MENU:
            if (pressed(pad.Buttons, PSP_CTRL_UP)) {
                selected = (selected + 2) % 3;
            }

            if (pressed(pad.Buttons, PSP_CTRL_DOWN)) {
                selected = (selected + 1) % 3;
            }

            if (pressed(pad.Buttons, PSP_CTRL_CROSS)) {
                if (selected == 0) {
                    screen = INTRO;
                    introStep = 0;
                    introFrames = 0;
                    playerX = 8;
                    playerY = 18;
                } else if (selected == 1) {
                    screen = GAME;
                } else {
                    screen = OPTIONS;
                }
            }

            drawMenu();
            break;

        case INTRO:
            drawIntro();

            if (pressed(pad.Buttons, PSP_CTRL_CROSS)) {
                introStep++;

                if (introStep >= 3) {
                    screen = GAME;
                }
            }

            introFrames++;
            break;

        case GAME:
            updateGame(pad);
            drawGame();
            break;

        case OPTIONS:
            pspDebugScreenClear();
            pspDebugScreenSetTextColor(0xFFFFFFFF);
            pspDebugScreenSetXY(5, 5);
            pspDebugScreenPrintf("OPCOES");
            pspDebugScreenSetXY(5, 8);
            pspDebugScreenPrintf("Beta 0.1");
            pspDebugScreenSetXY(5, 12);
            pspDebugScreenPrintf("TRIANGULO: VOLTAR");

            if (pressed(pad.Buttons, PSP_CTRL_TRIANGLE)) {
                screen = MENU;
            }
            break;
        }

        previousButtons = pad.Buttons;
        waitFrame();
    }

    sceKernelExitGame();
    return 0;
}
