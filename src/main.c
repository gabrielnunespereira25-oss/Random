
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>

#include "character.h"

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define MENU_COUNT 3

typedef enum {
    STATE_MENU,
    STATE_GAME,
    STATE_PAUSE
} GameState;

static const char *menu[MENU_COUNT] = {
    "JOGAR",
    "CONTROLES",
    "SAIR"
};

static int selected = 0;
static int running = 1;
static GameState state = STATE_MENU;
static Character player;

static void draw_menu(void)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(2, 2);

    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("RANDOM PSP\n");
    pspDebugScreenPrintf("====================\n\n");

    for (int i = 0; i < MENU_COUNT; i++) {
        if (i == selected)
            pspDebugScreenSetTextColor(0xFF00FFFF);
        else
            pspDebugScreenSetTextColor(0xFFFFFFFF);

        pspDebugScreenPrintf(
            "%s %s\n",
            i == selected ? ">" : " ",
            menu[i]
        );
    }

    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("\nCIMA/BAIXO: navegar\n");
    pspDebugScreenPrintf("X: selecionar\n");
}

static void draw_controls(void)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(2, 2);
    pspDebugScreenSetTextColor(0xFFFFFFFF);

    pspDebugScreenPrintf("CONTROLES\n\n");
    pspDebugScreenPrintf("ESQUERDA/DIREITA: mover\n");
    pspDebugScreenPrintf("X: pular\n");
    pspDebugScreenPrintf("QUADRADO: atacar\n");
    pspDebugScreenPrintf("CIRCULO: magia\n");
    pspDebugScreenPrintf("START: pausar\n");
    pspDebugScreenPrintf("O: voltar ao menu\n");
}

static void draw_game(void)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(2, 2);
    pspDebugScreenSetTextColor(0xFFFFFFFF);

    pspDebugScreenPrintf("RANDOM PSP - JOGO\n\n");
    pspDebugScreenPrintf("Personagem: ativo\n\n");

    pspDebugScreenPrintf(
        "Posicao X: %.1f\n", player.x
    );
    pspDebugScreenPrintf(
        "Posicao Y: %.1f\n", player.y
    );

    pspDebugScreenPrintf(
        "Velocidade X: %.1f\n", player.vx
    );

    pspDebugScreenPrintf("\n");

    switch (player.anim) {
        case ANIM_IDLE:
            pspDebugScreenPrintf("Animacao: parado\n");
            break;
        case ANIM_WALK:
            pspDebugScreenPrintf("Animacao: andando\n");
            break;
        case ANIM_RUN:
            pspDebugScreenPrintf("Animacao: correndo\n");
            break;
        case ANIM_JUMP:
            pspDebugScreenPrintf("Animacao: pulando\n");
            break;
        case ANIM_FALL:
            pspDebugScreenPrintf("Animacao: caindo\n");
            break;
        case ANIM_ATTACK:
            pspDebugScreenPrintf("Animacao: ataque\n");
            break;
        case ANIM_MAGIC:
            pspDebugScreenPrintf("Animacao: magia\n");
            break;
        default:
            pspDebugScreenPrintf("Animacao: especial\n");
            break;
    }

    pspDebugScreenPrintf("\n");
    pspDebugScreenPrintf("Setas: mover\n");
    pspDebugScreenPrintf("X: pular\n");
    pspDebugScreenPrintf("START: pausar\n");
    pspDebugScreenPrintf("O: voltar ao menu\n");
}

static void draw_pause(void)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(2, 2);
    pspDebugScreenSetTextColor(0xFFFFFFFF);

    pspDebugScreenPrintf("JOGO PAUSADO\n\n");
    pspDebugScreenPrintf("X: continuar\n");
    pspDebugScreenPrintf("O: voltar ao menu\n");
}

int main(void)
{
    SceCtrlData pad;
    unsigned int oldButtons = 0;

    pspDebugScreenInit();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    character_init(&player);
    draw_menu();

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int pressed =
            pad.Buttons & ~oldButtons;

        oldButtons = pad.Buttons;

        if (state == STATE_MENU) {
            if (pressed & PSP_CTRL_UP) {
                selected =
                    (selected + MENU_COUNT - 1) %
                    MENU_COUNT;
                draw_menu();
            }

            if (pressed & PSP_CTRL_DOWN) {
                selected =
                    (selected + 1) % MENU_COUNT;
                draw_menu();
            }

            if (pressed & PSP_CTRL_CROSS) {
                if (selected == 0) {
                    character_init(&player);
                    state = STATE_GAME;
                    draw_game();
                } else if (selected == 1) {
                    draw_controls();
                } else {
                    running = 0;
                }
            }

            if (pressed & PSP_CTRL_CIRCLE) {
                draw_menu();
            }
        }
        else if (state == STATE_GAME) {
            if (pressed & PSP_CTRL_CIRCLE) {
                state = STATE_MENU;
                draw_menu();
            }
            else if (pressed & PSP_CTRL_START) {
                state = STATE_PAUSE;
                draw_pause();
            }
            else {
                character_update(
                    &player, pad.Buttons
                );
                draw_game();
            }
        }
        else if (state == STATE_PAUSE) {
            if (pressed & PSP_CTRL_CROSS) {
                state = STATE_GAME;
                draw_game();
            }

            if (pressed & PSP_CTRL_CIRCLE) {
                state = STATE_MENU;
                draw_menu();
            }
        }

        sceDisplayWaitVblankStart();
        sceKernelDelayThread(16000);
    }

    sceKernelExitGame();
    return 0;
}
