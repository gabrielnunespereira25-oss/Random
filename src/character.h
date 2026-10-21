
#ifndef CHARACTER_H
#define CHARACTER_H

#include <pspctrl.h>

typedef enum {
    ANIM_IDLE,
    ANIM_WALK,
    ANIM_RUN,
    ANIM_JUMP,
    ANIM_FALL,
    ANIM_LANDING,
    ANIM_ATTACK,
    ANIM_HURT,
    ANIM_MAGIC
} Animation;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;

    int grounded;
    int jumps;
    int frame;
    int timer;
    int action_timer;
    int paused;

    Animation anim;
    unsigned int previous_buttons;
} Character;

void character_init(Character *c);

void character_update(
    Character *c,
    unsigned int buttons
);

const char *character_get_sprite(
    Character *c
);

#endif
