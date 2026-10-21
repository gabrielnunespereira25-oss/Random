
#include <pspctrl.h>
#include "character.h"

#define SCREEN_WIDTH 480
#define GROUND_Y 180
#define MOVE_SPEED 2.5f
#define RUN_SPEED 4.0f
#define GRAVITY 0.45f
#define JUMP_SPEED -8.0f

void character_init(Character *c)
{
    c->x = 220.0f;
    c->y = GROUND_Y;
    c->vx = 0.0f;
    c->vy = 0.0f;

    c->grounded = 1;
    c->frame = 0;
    c->timer = 0;
    c->anim = ANIM_IDLE;
}

void character_update(
    Character *c,
    unsigned int buttons
)
{
    c->vx = 0.0f;

    if (buttons & PSP_CTRL_LEFT) {
        c->vx = -MOVE_SPEED;
        c->anim = ANIM_WALK;
    } else if (buttons & PSP_CTRL_RIGHT) {
        c->vx = MOVE_SPEED;
        c->anim = ANIM_WALK;
    } else {
        c->anim = ANIM_IDLE;
    }

    if ((buttons & PSP_CTRL_RTRIGGER) &&
        c->vx != 0.0f) {
        c->vx = c->vx < 0
            ? -RUN_SPEED
            : RUN_SPEED;

        c->anim = ANIM_RUN;
    }

    if ((buttons & PSP_CTRL_CROSS) && c->grounded) {
        c->vy = JUMP_SPEED;
        c->grounded = 0;
        c->anim = ANIM_JUMP;
    }

    if (!c->grounded) {
        c->vy += GRAVITY;
        c->y += c->vy;

        if (c->vy > 0.0f) {
            c->anim = ANIM_FALL;
        }

        if (c->y >= GROUND_Y) {
            c->y = GROUND_Y;
            c->vy = 0.0f;
            c->grounded = 1;
        }
    }

    c->x += c->vx;

    if (c->x < 0.0f) {
        c->x = 0.0f;
    }

    if (c->x > SCREEN_WIDTH - 32) {
        c->x = SCREEN_WIDTH - 32;
    }

    c->timer++;

    if (c->timer >= 8) {
        c->timer = 0;
        c->frame++;
    }
}

const char *character_get_sprite(Character *c)
{
    switch (c->anim) {
        case ANIM_WALK:
            return (c->frame % 2 == 0)
                ? "assets/sprites/walk_1.png"
                : "assets/sprites/walk_2.png";

        case ANIM_RUN:
            return "assets/sprites/run_1.png";

        case ANIM_JUMP:
            return "assets/sprites/jump_1.png";

        case ANIM_FALL:
            return "assets/sprites/fall_1.png";

        case ANIM_ATTACK:
            return "assets/sprites/attack_1.png";

        case ANIM_MAGIC:
            return "assets/sprites/magic_1.png";

        case ANIM_IDLE:
        default:
            return (c->frame % 2 == 0)
                ? "assets/sprites/idle_1.png"
                : "assets/sprites/idle_2.png";
    }
}
