
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>

#include "graphics.h"
#include "menu.h"
#include "game.h"

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(void)
{
    SceCtrlData pad;
    unsigned int previous_buttons = 0;
    int running = 1;

    graphics_init();
    menu_init();
    game_init();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    while (running) {
        unsigned int buttons;
        unsigned int pressed;

        sceCtrlReadBufferPositive(&pad, 1);

        buttons = pad.Buttons;
        pressed = buttons & ~previous_buttons;
        previous_buttons = buttons;

        if (menu_is_active()) {
            menu_update(pressed, &running);
        } else {
            game_update(pressed);

            if (game_return_to_menu()) {
                menu_show();
            }
        }

        graphics_begin_frame();

        if (menu_is_active()) {
            menu_draw();
        } else {
            game_draw();
        }

        graphics_end_frame();
    }

    graphics_shutdown();
    sceKernelExitGame();

    return 0;
}
