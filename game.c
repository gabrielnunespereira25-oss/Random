
#include <pspctrl.h>

#include "game.h"
#include "graphics.h"
#include "sprite.h"
#include "character.h"

#define SCREEN_WIDTH 480
#define GROUND_Y 220

static Character player;
static int return_to_menu = 0;

void game_init(void)
{
    character_init(&player);
    sprite_init();
    return_to_menu = 0;
}

void game_update(unsigned int buttons)
{
    return_to_menu = 0;

    if (buttons & PSP_CTRL_CIRCLE) {
        return_to_menu = 1;
        return;
    }

    character_update(&player, buttons);
}

int game_return_to_menu(void)
{
    if (return_to_menu) {
        return_to_menu = 0;
        return 1;
    }

    return 0;
}

void game_draw(void)
{
    graphics_draw_rect(
        0, 0,
        SCREEN_WIDTH, GROUND_Y,
        0xFF76B9E8
    );

    graphics_draw_rect(
        0, GROUND_Y,
        SCREEN_WIDTH, 52,
        0xFF399447
    );

    sprite_draw(
        character_get_sprite(&player),
        (int)player.x,
        (int)player.y
    );
}
