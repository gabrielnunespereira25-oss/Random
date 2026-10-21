#include "character.h"
#include <pspctrl.h>

#define GROUND_Y 220.0f
#define SPEED 2.0f
#define GRAVITY 0.35f
#define JUMP_POWER -6.0f

static const char *sprites[][4] = {
    {"assets/sprites/idle_1.png", "assets/sprites/idle_2.png", 0, 0},
    {"assets/sprites/walk_1.png", "assets/sprites/walk_2.png", 0, 0},
    {"assets/sprites/run_1.png", "assets/sprites/run_2.png",
     "assets/sprites/run_3.png", "assets/sprites/run_4.png"},
    {"assets/sprites/jump_1.png", "assets/sprites/jump_2.png", 0, 0},
    {"assets/sprites/fall.png", 0, 0, 0},
    {"assets/sprites/attack_1.png", "assets/sprites/attack_2.png", 0, 0},
    {"assets/sprites/magic.png", 0, 0, 0}
};

void character_init(Character *c) {
    c->x = 100.0f;
    c->y = GROUND_Y;
    c->vx = 0;
    c->vy = 0;
    c->grounded = 1;
    c->frame = 0;
    c->timer = 0;
    c->anim = ANIM_IDLE;
}

void character_update(Character *c, unsigned int buttons) {
    c->vx = 0;

    if (buttons & PSP_CTRL_LEFT)
        c->vx = -SPEED;

    if (buttons & PSP_CTRL_RIGHT)
        c->vx = SPEED;

    if ((buttons & PSP_CTRL_CROSS) && c->grounded) {
        c->vy = JUMP_POWER;
        c->grounded = 0;
    }

    if (buttons & PSP_CTRL_SQUARE)
        c->anim = ANIM_ATTACK;
    else if (buttons & PSP_CTRL_CIRCLE)
        c->anim = ANIM_MAGIC;
    else if (!c->grounded)
        c->anim = c->vy < 0 ? ANIM_JUMP : ANIM_FALL;
    else if (c->vx != 0)
        c->anim = ANIM_WALK;
    else
        c->anim = ANIM_IDLE;

    c->x += c->vx;
    c->vy += GRAVITY;
    c->y += c->vy;

    if (c->y >= GROUND_Y) {
        c->y = GROUND_Y;
        c->vy = 0;
        c->grounded = 1;
    }

    if (c->x < 0) c->x = 0;
    if (c->x > 448) c->x = 448;

    c->timer++;
    if (c->timer >= 10) {
        c->timer = 0;
        c->frame = (c->frame + 1) % 4;
    }
}

const char *character_get_sprite(Character *c) {
    int count = 0;
    const char **list = sprites[c->anim];

    while (count < 4 && list[count])
        count++;

    if (count == 0)
        return "assets/sprites/idle_1.png";

    return list[c->frame % count];
}
