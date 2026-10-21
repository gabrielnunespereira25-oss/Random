#ifndef CHARACTER_H
#define CHARACTER_H

typedef enum {
    ANIM_IDLE,
    ANIM_WALK,
    ANIM_RUN,
    ANIM_JUMP,
    ANIM_FALL,
    ANIM_ATTACK,
    ANIM_MAGIC
} Animation;

typedef struct {
    float x, y;
    float vx, vy;
    int grounded;
    int frame;
    int timer;
    Animation anim;
} Character;

void character_init(Character *c);
void character_update(Character *c, unsigned int buttons);
const char *character_get_sprite(Character *c);

#endif
