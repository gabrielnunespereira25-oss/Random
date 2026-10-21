
#include <pspctrl.h>
#include "menu.h"
#include "graphics.h"

#define MENU_COUNT 3

static int selected = 0;
static int active = 1;

static const char *items[MENU_COUNT] = {
    "JOGAR",
    "CONTROLES",
    "SAIR"
};

void menu_init(void)
{
    selected = 0;
    active = 1;
}

void menu_show(void)
{
    active = 1;
}

int menu_is_active(void)
{
    return active;
}

void menu_update(unsigned int buttons, int *running)
{
    if (buttons & PSP_CTRL_UP) {
        selected--;

        if (selected < 0) {
            selected = MENU_COUNT - 1;
        }
    }

    if (buttons & PSP_CTRL_DOWN) {
        selected++;

        if (selected >= MENU_COUNT) {
            selected = 0;
        }
    }

    if (buttons & PSP_CTRL_CROSS) {
        if (selected == 0) {
            active = 0;
        } else if (selected == 1) {
            selected = 1;
        } else if (selected == 2) {
            *running = 0;
        }
    }
}

void menu_draw(void)
{
    int i;

    graphics_draw_rect(
        50, 35, 380, 200, 0xFF302040
    );

    graphics_draw_rect(
        60, 45, 360, 180, 0xFF181828
    );

    /*
     * Este módulo usa somente gráficos GU.
     * O texto será desenhado pelo módulo de texto
     * que adicionaremos depois.
     */
    for (i = 0; i < MENU_COUNT; i++) {
        unsigned int color;

        color = (i == selected)
            ? 0xFF00FFFF
            : 0xFFFFFFFF;

        graphics_draw_rect(
            125,
            90 + i * 40,
            230,
            24,
            (i == selected)
                ? 0xFF405878
                : 0xFF282838
        );

        (void)color;
    }
}
