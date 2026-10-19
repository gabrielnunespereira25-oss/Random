/*
 * RANDOM
 * Reconstrucao da base do jogo
 * Plataforma: PSP / PPSSPP
 * Linguagem: C
 *
 * Personagem: Essencia Escura
 *
 * Controles:
 * Analógico / direcional : movimentar
 * X                      : pular
 * Quadrado               : atacar
 * START                  : sair
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272

#define BLACK   0xFF000000
#define WHITE   0xFFFFFFFF
#define DARK_BG 0xFF171717
#define GRAY    0xFF444444

#define GRAVITY       900.0f
#define MOVE_SPEED    145.0f
#define RUN_SPEED     215.0f
#define JUMP_SPEED    340.0f
#define ATTACK_TIME   0.50f

typedef enum {
    ANIM_IDLE,
    ANIM_WALK,
    ANIM_RUN,
    ANIM_JUMP,
    ANIM_FALL,
    ANIM_LAND,
    ANIM_ATTACK
} AnimState;

typedef struct {
    float x, y;
    float vx, vy;

    float animTime;
    float attackTimer;
    float landTimer;

    int facing;
    int grounded;
    int attacking;
    int running;

    AnimState animState;
} Player;

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned int __attribute__((aligned(16))) guList[262144];

static Player player;

/* -------------------------------------------------
   DESENHO 2D
   ------------------------------------------------- */

static void drawQuad(
    float x, float y,
    float w, float h,
    unsigned int color
) {
    Vertex *v = (Vertex *)sceGuGetMemory(
        sizeof(Vertex) * 2
    );

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.0f;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v
    );
}

static void drawTriangle(
    float x1, float y1,
    float x2, float y2,
    float x3, float y3,
    unsigned int color
) {
    Vertex *v = (Vertex *)sceGuGetMemory(
        sizeof(Vertex) * 3
    );

    v[0].color = color;
    v[0].x = x1;
    v[0].y = y1;
    v[0].z = 0.0f;

    v[1].color = color;
    v[1].x = x2;
    v[1].y = y2;
    v[1].z = 0.0f;

    v[2].color = color;
    v[2].x = x3;
    v[2].y = y3;
    v[2].z = 0.0f;

    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        3, 0, v
    );
}

/* -------------------------------------------------
   ATAQUE EM MEIA-LUA
   Desenhado matematicamente, sem imagem externa.
   ------------------------------------------------- */

static void drawCrescent(
    float cx, float cy,
    float radius,
    float progress,
    int facing
) {
    int i;
    const int segments = 18;

    float startAngle;
    float endAngle;

    if (facing > 0) {
        startAngle = -1.25f;
        endAngle   =  1.25f;
    } else {
        startAngle = 3.1415926f + 1.25f;
        endAngle   = 3.1415926f - 1.25f;
    }

    /*
     * A animação avança durante os primeiros 70%
     * do ataque. O restante serve para desaparecer.
     */
    float sweepProgress = progress / 0.70f;

    if (sweepProgress > 1.0f)
        sweepProgress = 1.0f;

    int visibleSegments =
        (int)(segments * sweepProgress);

    if (visibleSegments < 1)
        visibleSegments = 1;

    for (i = 0; i < visibleSegments; i++) {
        float a0 = startAngle +
            (endAngle - startAngle) *
            ((float)i / segments);

        float a1 = startAngle +
            (endAngle - startAngle) *
            ((float)(i + 1) / segments);

        float x0 = cx + cosf(a0) * radius;
        float y0 = cy + sinf(a0) * radius;

        float x1 = cx + cosf(a1) * radius;
        float y1 = cy + sinf(a1) * radius;

        float innerRadius = radius - 4.0f;

        float ix0 = cx + cosf(a0) * innerRadius;
        float iy0 = cy + sinf(a0) * innerRadius;

        float ix1 = cx + cosf(a1) * innerRadius;
        float iy1 = cy + sinf(a1) * innerRadius;

        drawTriangle(
            x0, y0, x1, y1, ix0, iy0, WHITE
        );

        drawTriangle(
            ix0, iy0, x1, y1, ix1, iy1, WHITE
        );
    }
}

/* -------------------------------------------------
   PERSONAGEM
   ------------------------------------------------- */

static void playerDraw(
    Player *p,
    float cameraX,
    float cameraY
) {
    float x = p->x - cameraX;
    float y = p->y - cameraY;

    float legSwing = 0.0f;
    float armSwing = 0.0f;
    float lean = 0.0f;

    if (p->animState == ANIM_WALK) {
        legSwing = sinf(p->animTime * 9.0f) * 3.0f;
        armSwing = sinf(p->animTime * 9.0f) * 2.0f;
    }

    if (p->animState == ANIM_RUN) {
        legSwing = sinf(p->animTime * 14.0f) * 5.0f;
        armSwing = sinf(p->animTime * 14.0f) * 4.0f;
        lean = (float)p->facing * 2.0f;
    }

    if (p->animState == ANIM_JUMP)
        legSwing = 3.0f;

    if (p->animState == ANIM_FALL)
        legSwing = -2.0f;

    /* Sombra simples no chão */
    if (p->grounded) {
        drawQuad(
            x + 3.0f, y + 39.0f,
            22.0f, 2.0f,
            GRAY
        );
    }

    /* Pernas articuladas */
    drawQuad(
        x + 6.0f + lean,
        y + 25.0f,
        5.0f,
        12.0f + legSwing,
        BLACK
    );

    drawQuad(
        x + 16.0f + lean,
        y + 25.0f,
        5.0f,
        12.0f - legSwing,
        BLACK
    );

    /* Pés */
    drawQuad(
        x + 4.0f + lean,
        y + 36.0f + legSwing,
        8.0f, 3.0f,
        BLACK
    );

    drawQuad(
        x + 15.0f + lean,
        y + 36.0f - legSwing,
        8.0f, 3.0f,
        BLACK
    );

    /* Tronco */
    drawQuad(
        x + 5.0f + lean,
        y + 12.0f,
        18.0f, 15.0f,
        BLACK
    );

    /* Braços */
    drawQuad(
        x + 1.0f + lean,
        y + 14.0f + armSwing,
        5.0f, 13.0f,
        BLACK
    );

    drawQuad(
        x + 22.0f + lean,
        y + 14.0f - armSwing,
        5.0f, 13.0f,
        BLACK
    );

    /* Cabeça / chama geométrica */
    drawTriangle(
        x + 5.0f,  y + 12.0f,
        x + 8.0f,  y + 1.0f,
        x + 13.0f, y + 9.0f,
        BLACK
    );

    drawTriangle(
        x + 12.0f, y + 9.0f,
        x + 19.0f, y + 0.0f,
        x + 23.0f, y + 12.0f,
        BLACK
    );

    drawQuad(
        x + 6.0f, y + 7.0f,
        17.0f, 10.0f,
        BLACK
    );

    /* Um olho branco fino */
    if (p->facing > 0) {
        drawQuad(
            x + 17.0f, y + 10.0f,
            5.0f, 1.5f,
            WHITE
        );
    } else {
        drawQuad(
            x + 7.0f, y + 10.0f,
            5.0f, 1.5f,
            WHITE
        );
    }

    /* Golpe de meia-lua */
    if (p->attacking) {
        float progress =
            1.0f - p->attackTimer / ATTACK_TIME;

        float attackX = x + 14.0f +
            (float)p->facing * 15.0f;

        float attackY = y + 19.0f;

        /*
         * Fade simples: o arco fica visível durante
         * a fase de varredura e some no final.
         */
        if (progress < 0.85f) {
            drawCrescent(
                attackX,
                attackY,
                25.0f,
                progress,
                p->facing
            );
        }
    }
}

/* -------------------------------------------------
   ATUALIZACAO DO JOGADOR
   ------------------------------------------------- */

static void playerUpdate(
    Player *p,
    float dt,
    unsigned int buttons,
    SceCtrlData *pad
) {
    float axis = 0.0f;

    if (pad->Lx < 110)
        axis = -1.0f;
    else if (pad->Lx > 145)
        axis = 1.0f;

    if (buttons & PSP_CTRL_LEFT)
        axis = -1.0f;

    if (buttons & PSP_CTRL_RIGHT)
        axis = 1.0f;

    p->running = (buttons & PSP_CTRL_RTRIGGER) != 0;

    float speed = p->running ? RUN_SPEED : MOVE_SPEED;

    if (!p->attacking) {
        p->vx = axis * speed;

        if (axis < 0.0f)
            p->facing = -1;

        if (axis > 0.0f)
            p->facing = 1;
    } else {
        p->vx *= 0.85f;
    }

    /* Pulo */
    if ((buttons & PSP_CTRL_CROSS) && p->grounded) {
        p->vy = -JUMP_SPEED;
        p->grounded = 0;
        p->landTimer = 0.0f;
    }

    /* Gravidade */
    p->vy += GRAVITY * dt;

    p->x += p->vx * dt;
    p->y += p->vy * dt;

    /* Chão provisório */
    if (p->y >= 190.0f) {
        p->y = 190.0f;
        p->vy = 0.0f;

        if (!p->grounded)
            p->landTimer = 0.10f;

        p->grounded = 1;
    } else {
        p->grounded = 0;
    }

    if (p->landTimer > 0.0f)
        p->landTimer -= dt;

    /* Temporizador do ataque */
    if (p->attacking) {
        p->attackTimer -= dt;

        if (p->attackTimer <= 0.0f) {
            p->attackTimer = 0.0f;
            p->attacking = 0;
        }
    }

    /* Escolha da animação */
    if (p->attacking) {
        p->animState = ANIM_ATTACK;
    } else if (!p->grounded && p->vy < 0.0f) {
        p->animState = ANIM_JUMP;
    } else if (!p->grounded && p->vy >= 0.0f) {
        p->animState = ANIM_FALL;
    } else if (p->landTimer > 0.0f) {
        p->animState = ANIM_LAND;
    } else if (fabsf(p->vx) > 1.0f) {
        p->animState = p->running ? ANIM_RUN : ANIM_WALK;
    } else {
        p->animState = ANIM_IDLE;
    }

    p->animTime += dt;
}

/* -------------------------------------------------
   INICIALIZACAO DO PSP GU
   ------------------------------------------------- */

static void graphicsInit(void) {
    sceGuInit();

    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        512
    );

    sceGuDispBuffer(
        SCREEN_W,
        SCREEN_H,
        (void *)0x88000,
        512
    );

    sceGuDepthBuffer(
        (void *)0x110000,
        512
    );

    sceGuOffset(2048 - SCREEN_W / 2, 2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);

    sceGuDepthRange(0xC350, 0x2710);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuShadeModel(GU_SMOOTH);
    sceGuClearColor(DARK_BG);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceGuDisplay(GU_TRUE);
}

/* -------------------------------------------------
   LOOP PRINCIPAL
   ------------------------------------------------- */

int main(void) {
    SceCtrlData pad;
    unsigned int oldButtons = 0;

    float cameraX = 0.0f;
    float cameraY = 0.0f;

    memset(&player, 0, sizeof(Player));

    player.x = 80.0f;
    player.y = 190.0f;
    player.facing = 1;
    player.grounded = 1;
    player.animState = ANIM_IDLE;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    graphicsInit();

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int buttons = pad.Buttons;
        unsigned int pressed = buttons & ~oldButtons;

        if (pressed & PSP_CTRL_START)
            break;

        /* Ataque apenas no início do pressionamento */
        if ((pressed & PSP_CTRL_SQUARE) && !player.attacking) {
            player.attacking = 1;
            player.attackTimer = ATTACK_TIME;
        }

        /*
         * Câmera horizontal simples.
         * O jogador permanece próximo ao centro.
         */
        cameraX = player.x - 200.0f;

        if (cameraX < 0.0f)
            cameraX = 0.0f;

        cameraY = 0.0f;

        /*
         * Delta de tempo provisório.
         * O loop é sincronizado com o refresh do PSP.
         */
        float dt = 1.0f / 60.0f;

        playerUpdate(
            &player,
            dt,
            buttons,
            &pad
        );

        sceGuStart(GU_DIRECT, guList);

        sceGuClearColor(DARK_BG);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        /* Chão provisório */
        drawQuad(
            0.0f,
            230.0f,
            SCREEN_W,
            42.0f,
            GRAY
        );

        /* Linha do chão */
        drawQuad(
            0.0f,
            230.0f,
            SCREEN_W,
            2.0f,
            WHITE
        );

        playerDraw(
            &player,
            cameraX,
            cameraY
        );

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        oldButtons = buttons;
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
