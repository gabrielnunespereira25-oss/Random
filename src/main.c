
/*
 * RANDOM
 * Essencia Escura
 * PSP / PSPSDK
 *
 * Personagem, movimento, salto, corrida,
 * espada animada e corte em meia-lua.
 *
 * Controles:
 * Direcional / analogico = mover
 * X = pular
 * Quadrado = atacar
 * R + direcao = correr
 * START = sair
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspdebug.h>
#include <psputils.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SW 480
#define SH 272
#define PI_F 3.14159265358979323846f

#define BLACK 0xFF000000
#define WHITE 0xFFFFFFFF
#define GRAY  0xFFD0D0D0
#define DARK  0xFF444444

#define GRAVITY 650.0f
#define WALK_SPEED 140.0f
#define RUN_SPEED 220.0f
#define JUMP_SPEED 300.0f
#define ATTACK_DURATION 0.50f

static unsigned int __attribute__((aligned(16))) list[262144];

typedef struct
{
    float x, y;
    float vx, vy;
    float animTime;
    float attackTimer;

    int facing;
    int grounded;
    int attacking;
    int wasAttackPressed;

    int animState;
} Player;

enum
{
    IDLE,
    WALK,
    RUN,
    JUMP,
    FALL,
    ATTACK
};

/* =================================================
   GEOMETRIA
   ================================================= */

typedef struct
{
    unsigned int color;
    float x, y, z;
} Vertex;

static void triangle(
    float x1, float y1,
    float x2, float y2,
    float x3, float y3,
    unsigned int color)
{
    Vertex v[3] =
    {
        {color, x1, y1, 0.0f},
        {color, x2, y2, 0.0f},
        {color, x3, y3, 0.0f}
    };

    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        3, 0, v
    );
}

static void quad(
    float x, float y,
    float w, float h,
    unsigned int color)
{
    if (w <= 0.0f || h <= 0.0f)
        return;

    triangle(x, y, x + w, y, x, y + h, color);
    triangle(x + w, y, x + w, y + h, x, y + h, color);
}

/* =================================================
   PERSONAGEM
   ================================================= */

static void playerInit(Player *p)
{
    memset(p, 0, sizeof(Player));

    p->x = 100.0f;
    p->y = 200.0f;
    p->facing = 1;
    p->grounded = 1;
    p->animState = IDLE;
}

static void playerStartAttack(Player *p)
{
    if (p->attacking)
        return;

    p->attacking = 1;
    p->attackTimer = ATTACK_DURATION;
}

static void playerUpdate(Player *p, float dt)
{
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    int left = (pad.Buttons & PSP_CTRL_LEFT) != 0;
    int right = (pad.Buttons & PSP_CTRL_RIGHT) != 0;
    int jump = (pad.Buttons & PSP_CTRL_CROSS) != 0;
    int attack = (pad.Buttons & PSP_CTRL_SQUARE) != 0;
    int run = (pad.Buttons & PSP_CTRL_RTRIGGER) != 0;

    p->vx = 0.0f;

    float speed = run ? RUN_SPEED : WALK_SPEED;

    if (left)
    {
        p->vx = -speed;
        p->facing = -1;
    }

    if (right)
    {
        p->vx = speed;
        p->facing = 1;
    }

    if (jump && p->grounded)
    {
        p->vy = -JUMP_SPEED;
        p->grounded = 0;
    }

    /* Gravidade e piso temporario */
    if (!p->grounded)
    {
        p->vy += GRAVITY * dt;
        p->y += p->vy * dt;

        if (p->y >= 200.0f)
        {
            p->y = 200.0f;
            p->vy = 0.0f;
            p->grounded = 1;
        }
    }

    p->x += p->vx * dt;

    if (p->x < 8.0f)
        p->x = 8.0f;

    if (p->x > SW - 30.0f)
        p->x = SW - 30.0f;

    /* Ataque ao pressionar o botao */
    if (attack && !p->wasAttackPressed)
        playerStartAttack(p);

    p->wasAttackPressed = attack;

    if (p->attacking)
    {
        p->attackTimer -= dt;

        if (p->attackTimer <= 0.0f)
        {
            p->attackTimer = 0.0f;
            p->attacking = 0;
        }
    }

    p->animTime += dt;

    if (p->attacking)
        p->animState = ATTACK;
    else if (!p->grounded && p->vy < 0.0f)
        p->animState = JUMP;
    else if (!p->grounded)
        p->animState = FALL;
    else if (fabsf(p->vx) > 0.0f && run)
        p->animState = RUN;
    else if (fabsf(p->vx) > 0.0f)
        p->animState = WALK;
    else
        p->animState = IDLE;
}

/* =================================================
   ESPADA E CORTE EM MEIA-LUA
   ================================================= */

static void drawSword(Player *p)
{
    float x = p->x;
    float y = p->y;
    float dir = (float)p->facing;

    float elapsed = ATTACK_DURATION - p->attackTimer;

    /*
     * A espada acompanha o golpe.
     * Durante o ataque, muda de angulo.
     */
    float angle = 0.0f;

    if (p->attacking)
    {
        float progress = elapsed / ATTACK_DURATION;

        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        angle = -1.0f + progress * 2.0f;
    }
    else
    {
        angle = -0.30f;
    }

    float handX = x + 19.0f * dir;
    float handY = y + 18.0f;

    float length = p->attacking ? 34.0f : 22.0f;

    float tipX = handX + cosf(angle) * length * dir;
    float tipY = handY + sinf(angle) * length;

    /* Cabo */
    quad(
        handX - 2.0f,
        handY - 2.0f,
        5.0f,
        5.0f,
        DARK
    );

    /* Lamina como triangulo fino */
    float perpX = -sinf(angle) * 2.0f;
    float perpY = cosf(angle) * 2.0f;

    triangle(
        handX + perpX,
        handY + perpY,
        handX - perpX,
        handY - perpY,
        tipX,
        tipY,
        WHITE
    );

    if (p->attacking)
    {
        float progress = elapsed / ATTACK_DURATION;

        if (progress >= 0.08f && progress <= 0.38f)
        {
            float t = (progress - 0.08f) / 0.30f;

            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            float start = -1.25f;
            float finish = 1.25f;
            float endAngle = start + (finish - start) * t;

            float outer = 43.0f;
            float inner = 34.0f;
            int segments = 14;

            for (int i = 0; i < segments; i++)
            {
                float a0 = start +
                    (endAngle - start) * ((float)i / segments);

                float a1 = start +
                    (endAngle - start) * ((float)(i + 1) / segments);

                float cx = handX + 3.0f * dir;
                float cy = handY;

                float ox0 = cx + cosf(a0) * outer * dir;
                float oy0 = cy + sinf(a0) * outer;

                float ox1 = cx + cosf(a1) * outer * dir;
                float oy1 = cy + sinf(a1) * outer;

                float ix0 = cx + cosf(a0) * inner * dir;
                float iy0 = cy + sinf(a0) * inner;

                float ix1 = cx + cosf(a1) * inner * dir;
                float iy1 = cy + sinf(a1) * inner;

                triangle(
                    ox0, oy0,
                    ox1, oy1,
                    ix0, iy0,
                    WHITE
                );

                triangle(
                    ix0, iy0,
                    ox1, oy1,
                    ix1, iy1,
                    GRAY
                );
            }
        }
    }
}

/* =================================================
   ANIMACAO DO CORPO
   ================================================= */

static void playerDraw(Player *p)
{
    float x = p->x;
    float y = p->y;

    float bob = 0.0f;
    float lean = 0.0f;

    if (p->animState == IDLE)
        bob = sinf(p->animTime * 3.0f) * 1.0f;

    if (p->animState == WALK)
        bob = fabsf(sinf(p->animTime * 9.0f)) * 2.0f;

    if (p->animState == RUN)
    {
        bob = fabsf(sinf(p->animTime * 14.0f)) * 3.0f;
        lean = 3.0f;
    }

    if (p->animState == ATTACK)
        lean = 2.0f;

    y += bob;

    /*
     * Duas pernas independentes.
     * O comprimento nunca fica negativo.
     */
    float swing = 0.0f;

    if (p->animState == WALK)
        swing = sinf(p->animTime * 9.0f) * 3.0f;

    if (p->animState == RUN)
        swing = sinf(p->animTime * 14.0f) * 5.0f;

    float leftLegH = 12.0f + swing;
    float rightLegH = 12.0f - swing;

    if (leftLegH < 7.0f) leftLegH = 7.0f;
    if (rightLegH < 7.0f) rightLegH = 7.0f;

    quad(
        x + 7.0f,
        y + 26.0f,
        5.0f,
        leftLegH,
        BLACK
    );

    quad(
        x + 16.0f,
        y + 26.0f,
        5.0f,
        rightLegH,
        BLACK
    );

    /* Tronco */
    triangle(
        x + 5.0f + lean,
        y + 11.0f,
        x + 24.0f + lean,
        y + 12.0f,
        x + 16.0f,
        y + 30.0f,
        BLACK
    );

    /* Braco de tras */
    quad(
        x + 3.0f,
        y + 14.0f,
        5.0f,
        12.0f,
        BLACK
    );

    /* Braco da espada */
    quad(
        x + 19.0f,
        y + 14.0f,
        5.0f,
        9.0f,
        BLACK
    );

    /* Chama da cabeca */
    triangle(
        x + 6.0f,
        y + 11.0f,
        x + 12.0f,
        y - 3.0f,
        x + 20.0f,
        y + 11.0f,
        BLACK
    );

    /* Cabeca */
    quad(
        x + 6.0f,
        y + 5.0f,
        14.0f,
        11.0f,
        BLACK
    );

    /* Olho branco fino */
    if (p->facing > 0)
    {
        quad(
            x + 14.0f,
            y + 9.0f,
            5.0f,
            1.5f,
            WHITE
        );
    }
    else
    {
        quad(
            x + 7.0f,
            y + 9.0f,
            5.0f,
            1.5f,
            WHITE
        );
    }

    /* Espada e efeito desenhados por cima */
    drawSword(p);
}

/* =================================================
   GRAFICOS
   ================================================= */

static void graphicsInit(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        512
    );

    sceGuDispBuffer(
        SW,
        SH,
        (void *)0x88000,
        512
    );

    sceGuDepthBuffer(
        (void *)0x110000,
        512
    );

    sceGuOffset(
        2048 - SW / 2,
        2048 - SH / 2
    );

    sceGuViewport(2048, 2048, SW, SH);
    sceGuScissor(0, 0, SW, SH);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* =================================================
   MAIN
   ================================================= */

int main(void)
{
    Player player;
    playerInit(&player);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    graphicsInit();

    while (1)
    {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_START)
            break;

        /*
         * Tempo fixo de 60 FPS para o teste inicial.
         */
        float dt = 1.0f / 60.0f;

        playerUpdate(&player, dt);

        sceGuStart(GU_DIRECT, list);

        sceGuClearColor(0xFFFFFFFF);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        /* Piso temporario */
        quad(
            0.0f,
            230.0f,
            SW,
            42.0f,
            GRAY
        );

        playerDraw(&player);

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
