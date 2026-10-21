
#include "character.h"
#include <pspctrl.h>

#define SCREEN_W 480
#define SCREEN_H 272
#define GROUND_Y 220.0f

#define SPEED_WALK 2.0f
#define SPEED_RUN 4.0f
#define JUMP_POWER -7.5f
#define GRAVITY 0.30f

static int pressed(Character *c, unsigned int buttons,
                   unsigned int key)
{
    return (buttons & key) && !(c->previous_buttons & key);
}

void character_init(Character *c)
{
    c->x = 40.0f;
    c->y = GROUND_Y;
    c->vx = 0;
    c->vy = 0;
    c->grounded = 1;
    c->jumps = 0;
    c->frame = 0;
    c->timer = 0;
    c->action_timer = 0;
    c->paused = 0;
    c->anim = ANIM_IDLE;
    c->previous_buttons = 0;
}

void character_update(Character *c, unsigned int buttons)
{
    if (pressed(c, buttons, PSP_CTRL_START))
        c->paused = !c->paused;

    if (c->paused) {
        c->previous_buttons = buttons;
        return;
    }

    if (c->action_timer > 0)
        c->action_timer--;

    /* Ações especiais */
    if (pressed(c, buttons, PSP_CTRL_SQUARE)) {
        c->anim = ANIM_ATTACK;
        c->action_timer = 20;
        c->frame = 0;
    }
    else if (pressed(c, buttons, PSP_CTRL_CIRCLE)) {
        c->anim = ANIM_MAGIC;
        c->action_timer = 24;
        c->frame = 0;
    }
    else if (pressed(c, buttons, PSP_CTRL_CROSS)
             && c->jumps < 2) {
        c->vy = JUMP_POWER;
        c->grounded = 0;
        c->jumps++;
        c->anim = ANIM_JUMP;
        c->frame = 0;
    }

    /* Movimento horizontal */
    c->vx = 0;

    if (buttons & PSP_CTRL_LEFT)
        c->vx = -((buttons & PSP_CTRL_RTRIGGER)
                  ? SPEED_RUN : SPEED_WALK);

    if (buttons & PSP_CTRL_RIGHT)
        c->vx = ((buttons & PSP_CTRL_RTRIGGER)
                 ? SPEED_RUN : SPEED_WALK);

    c->x += c->vx;

    if (c->x < 0) c->x = 0;
    if (c->x > SCREEN_W - 32)
        c->x = SCREEN_W - 32;

    /* Gravidade e aterrissagem */
    if (!c->grounded) {
        c->vy += GRAVITY;
        c->y += c->vy;

        if (c->y >= GROUND_Y) {
            c->y = GROUND_Y;
            c->vy = 0;
            c->grounded = 1;
            c->jumps = 0;
            c->anim = ANIM_LANDING;
            c->action_timer = 6;
            c->frame = 0;
        } else if (c->vy > 0) {
            c->anim = ANIM_FALL;
        }
    }

    /* Prioridade das animações */
    if (c->action_timer > 0 &&
        (c->anim == ANIM_ATTACK ||
         c->anim == ANIM_MAGIC ||
         c->anim == ANIM_LANDING)) {
        /* Mantém a animação da ação */
    }
    else if (!c->grounded) {
        if (c->vy <= 0)
            c->anim = ANIM_JUMP;
        else
            c->anim = ANIM_FALL;
    }
    else if (c->vx != 0) {
        c->anim = (buttons & PSP_CTRL_RTRIGGER)
                  ? ANIM_RUN : ANIM_WALK;
    }
    else {
        c->anim = ANIM_IDLE;
    }

    /* Velocidade da animação */
    c->timer++;
    if (c->timer >= 8) {
        c->timer = 0;
        c->frame++;
    }

    c->previous_buttons = buttons;
}

const char *character_get_sprite(Character *c)
{
    static const char *idle[] = {
        "assets/sprites/idle_1.png",
        "assets/sprites/idle_2.png"
    };

    static const char *walk[] = {
        "assets/sprites/walk_1.png",
        "assets/sprites/walk_2.png"
    };

    static const char *run[] = {
        "assets/sprites/run_1.png",
        "assets/sprites/run_2.png",
        "assets/sprites/run_3.png",
        "assets/sprites/run_4.png"
    };

    static const char *jump[] = {
        "assets/sprites/jump_1.png",
        "assets/sprites/jump_2.png"
    };

    static const char *attack[] = {
        "assets/sprites/attack_1.png",
        "assets/sprites/attack_2.png"
    };

    switch (c->anim) {
        case ANIM_IDLE:
            return idle[c->frame % 2];

        case ANIM_WALK:
            return walk[c->frame % 2];

        case ANIM_RUN:
            return run[c->frame % 4];

        case ANIM_JUMP:
            return jump[c->frame % 2];

        case ANIM_FALL:
            return "assets/sprites/fall.png";

        case ANIM_LANDING:
            return "assets/sprites/landing.png";

        case ANIM_ATTACK:
            if (c->action_timer > 0)
                return attack[(c->frame / 2) % 2];
            break;

        case ANIM_MAGIC:
            if (c->action_timer > 0)
                return "assets/sprites/magic.png";
            break;

        case ANIM_HURT:
            return "assets/sprites/hurt.png";
    }

    return idle[c->frame % 2];
}
