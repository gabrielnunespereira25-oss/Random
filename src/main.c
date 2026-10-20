
/*
 * RANDOM REMAKE - AREA DE TESTE
 * PSP / PSPSDK
 * Protagonista e cenário desenhados inteiramente em C.
 * Sem PNG, GIF ou imagens externas.
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM_REMAKE", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define W 480
#define H 272
#define FLOOR_Y 230.0f

#define BLACK  0xFF080808
#define DARK   0xFF202020
#define GRAY   0xFF555555
#define GRAY2  0xFF999999
#define WHITE  0xFFF5F5F0
#define BLUE   0xFFB08030
#define RED    0xFF3030D0

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y;
    float vx, vy;
    float scale;
    float animTime;
    int direction;
    int anim;
    int grounded;
    int attackTimer;
    int dashTimer;
    int hp;
} Player;

enum {
    IDLE,
    RUN,
    HIT,
    JUMP,
    DASH,
    DOWN_ATTACK
};

static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned int __attribute__((aligned(16))) fb0[W * H];
static unsigned int __attribute__((aligned(16))) fb1[W * H];
static unsigned int __attribute__((aligned(16))) zbuf[W * H];

static Player player;
static int running = 1;
static int targetHP = 5;
static int targetFlash = 0;

static void rect(float x, float y, float w, float h,
                 unsigned int color)
{
    Vertex v[2] = {
        {color, x,     y,     0},
        {color, x + w, y + h, 0}
    };

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, 0, v
    );
}

static void poly(Vertex *v, int n)
{
    sceGuDrawArray(
        GU_TRIANGLE_FAN,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        n, 0, v
    );
}

static void triangle(float x1, float y1,
                     float x2, float y2,
                     float x3, float y3,
                     unsigned int color)
{
    Vertex v[3] = {
        {color, x1, y1, 0},
        {color, x2, y2, 0},
        {color, x3, y3, 0}
    };
    poly(v, 3);
}

static void line(float x1, float y1, float x2, float y2,
                 float thickness, unsigned int color)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);

    if (len < 0.1f) return;

    float px = -dy / len * thickness * 0.5f;
    float py =  dx / len * thickness * 0.5f;

    Vertex v[4] = {
        {color, x1 + px, y1 + py, 0},
        {color, x2 + px, y2 + py, 0},
        {color, x2 - px, y2 - py, 0},
        {color, x1 - px, y1 - py, 0}
    };
    poly(v, 4);
}

/* ---------- CENARIO DE TESTE ---------- */

static void drawBackground(void)
{
    rect(0, 0, W, H, 0xFFF0F0E8);

    /* Fundo com silhuetas de cavernas e paredes. */
    triangle(0, 0, 125, 0, 0, 110, 0xFFE2E2DA);
    triangle(480, 0, 370, 0, 480, 90, 0xFFE4E4DC);

    /* Paredes verticais. */
    rect(0, 0, 12, H, GRAY);
    rect(468, 0, 12, H, GRAY);

    /* Chão. */
    rect(0, FLOOR_Y, W, H - FLOOR_Y, DARK);
    rect(0, FLOOR_Y, W, 4, GRAY2);

    /* Plataformas. */
    rect(80, 180, 105, 10, DARK);
    rect(80, 180, 105, 3, GRAY2);

    rect(225, 145, 95, 10, DARK);
    rect(225, 145, 95, 3, GRAY2);

    rect(350, 185, 75, 10, DARK);
    rect(350, 185, 75, 3, GRAY2);

    /* Marcas do chão. */
    line(25, 246, 95, 246, 2, GRAY);
    line(120, 257, 190, 257, 2, GRAY);
    line(310, 248, 380, 248, 2, GRAY);
}

/* ---------- PROTAGONISTA DESENHADO EM C ---------- */

static void hair(float x, float y, float s)
{
    triangle(x + 8*s, y + 39*s,
             x + 12*s, y + 2*s,
             x + 25*s, y + 31*s, BLACK);

    triangle(x + 18*s, y + 31*s,
             x + 35*s, y + 8*s,
             x + 35*s, y + 40*s, BLACK);

    triangle(x + 29*s, y + 35*s,
             x + 45*s, y + 18*s,
             x + 43*s, y + 45*s, BLACK);

    triangle(x + 24*s, y + 17*s,
             x + 41*s, y + 29*s,
             x + 31*s, y + 43*s, GRAY);

    rect(x + 14*s, y + 32*s, 29*s, 12*s, BLACK);
}

static void face(float x, float y, float s)
{
    rect(x + 13*s, y + 38*s, 28*s, 31*s, WHITE);
    rect(x + 17*s, y + 32*s, 21*s, 8*s, WHITE);
    rect(x + 17*s, y + 66*s, 19*s, 8*s, WHITE);

    triangle(x + 35*s, y + 43*s,
             x + 42*s, y + 53*s,
             x + 35*s, y + 66*s, GRAY2);
}

static void torso(float x, float y, float s)
{
    rect(x + 22*s, y + 70*s, 13*s, 12*s, GRAY2);

    triangle(x + 14*s, y + 78*s,
             x + 39*s, y + 78*s,
             x + 44*s, y + 119*s, BLACK);

    triangle(x + 14*s, y + 79*s,
             x + 7*s, y + 115*s,
             x + 25*s, y + 124*s, BLACK);

    rect(x + 14*s, y + 91*s, 29*s, 29*s, BLACK);

    triangle(x + 14*s, y + 82*s,
             x + 25*s, y + 90*s,
             x + 9*s, y + 103*s, GRAY);

    triangle(x + 26*s, y + 84*s,
             x + 39*s, y + 82*s,
             x + 35*s, y + 97*s, GRAY2);

    triangle(x + 14*s, y + 78*s,
             x + 25*s, y + 91*s,
             x + 35*s, y + 79*s, WHITE);

    line(x + 15*s, y + 92*s,
         x + 34*s, y + 95*s, 3*s, GRAY2);

    rect(x + 13*s, y + 117*s, 30*s, 6*s, GRAY2);
    rect(x + 18*s, y + 117*s, 19*s, 3*s, BLACK);
}

static void arms(float x, float y, float s, float swing)
{
    /* Braço esquerdo. */
    line(x + 13*s, y + 85*s,
         x + 5*s, y + 109*s + swing, 7*s, BLACK);

    line(x + 5*s, y + 109*s + swing,
         x + 3*s, y + 129*s + swing, 5*s, BLACK);

    rect(x - 1*s, y + 126*s + swing, 8*s, 7*s, GRAY);
    rect(x - 2*s, y + 131*s + swing, 5*s, 5*s, WHITE);

    /* Braço direito comprido. */
    line(x + 39*s, y + 85*s,
         x + 48*s, y + 108*s - swing, 9*s, BLACK);

    line(x + 48*s, y + 108*s - swing,
         x + 59*s, y + 132*s - swing, 5*s, BLACK);

    line(x + 59*s, y + 132*s - swing,
         x + 63*s, y + 150*s - swing, 4*s, BLACK);

    rect(x + 58*s, y + 148*s - swing, 7*s, 6*s, BLACK);
    rect(x + 62*s, y + 151*s - swing, 5*s, 4*s, GRAY2);
}

static void legs(float x, float y, float s, float step)
{
    triangle(x + 14*s, y + 119*s,
             x + 29*s, y + 120*s,
             x + 25*s, y + 157*s, BLACK);

    triangle(x + 29*s, y + 120*s,
             x + 42*s, y + 119*s,
             x + 47*s, y + 155*s, BLACK);

    triangle(x + 30*s, y + 123*s,
             x + 37*s, y + 125*s,
             x + 47*s, y + 151*s, GRAY);

    triangle(x + 30*s, y + 129*s,
             x + 38*s, y + 138*s,
             x + 35*s, y + 146*s, WHITE);

    line(x + 21*s, y + 151*s,
         x + 19*s + step, y + 177*s, 5*s, BLACK);

    line(x + 41*s, y + 150*s,
         x + 44*s - step, y + 177*s, 5*s, BLACK);

    rect(x + 13*s + step, y + 175*s, 12*s, 5*s, BLACK);
    rect(x + 39*s - step, y + 175*s, 12*s, 5*s, BLACK);

    rect(x + 17*s + step, y + 177*s, 7*s, 2*s, WHITE);
    rect(x + 42*s - step, y + 177*s, 7*s, 2*s, WHITE);
}

static void drawPlayer(Player *p)
{
    float x = p->x;
    float y = p->y;
    float s = p->scale;
    float swing = 0;
    float step = 0;

    if (p->anim == RUN) {
        swing = sinf(p->animTime * 12.0f) * 5.0f;
        step = sinf(p->animTime * 12.0f) * 7.0f;
    }

    if (p->anim == DASH) {
        x -= p->direction * 5;
    }

    legs(x, y, s, step);
    torso(x, y, s);
    arms(x, y, s, swing);
    face(x, y, s);
    hair(x, y, s);

    /* Ataque frontal: meia-lua desenhada com linhas. */
    if (p->anim == HIT && p->attackTimer > 0) {
        float ax = x + (p->direction > 0 ? 48*s : -22*s);
        float sign = (float)p->direction;

        line(ax, y + 92*s,
             ax + 18*s*sign, y + 78*s, 4*s, WHITE);

        line(ax + 18*s*sign, y + 78*s,
             ax + 38*s*sign, y + 88*s, 3*s, GRAY2);

        line(ax + 4*s*sign, y + 99*s,
             ax + 30*s*sign, y + 98*s, 2*s, WHITE);
    }

    /* Ataque para baixo. */
    if (p->anim == DOWN_ATTACK && p->attackTimer > 0) {
        line(x + 29*s, y + 130*s,
             x + 29*s, y + 157*s, 4*s, WHITE);

        line(x + 29*s, y + 157*s,
             x + 18*s, y + 148*s, 3*s, GRAY2);
    }

    /* Rastro do dash. */
    if (p->dashTimer > 0) {
        float back = x - p->direction * 12*s;

        line(back, y + 82*s,
             back - p->direction * 14*s, y + 82*s,
             3*s, GRAY2);

        line(back, y + 98*s,
             back - p->direction * 20*s, y + 98*s,
             2*s, WHITE);
    }
}

/* ---------- BONECO DE TREINO ---------- */

static void drawTarget(void)
{
    float x = 405;
    float y = FLOOR_Y - 57;

    unsigned int color = targetFlash ? WHITE : BLACK;

    rect(x - 10, y, 20, 25, color);
    rect(x - 7, y + 25, 14, 19, GRAY);
    line(x - 7, y + 29, x - 19, y + 43, 5, color);
    line(x + 7, y + 29, x + 19, y + 43, 5, color);
    line(x - 4, y + 43, x - 7, y + 56, 5, color);
    line(x + 4, y + 43, x + 7, y + 56, 5, color);

    rect(x - 6, y + 8, 12, 3, RED);
}

/* ---------- MOVIMENTO E COLISOES ---------- */

static int onPlatform(float x, float oldY, float newY, float w)
{
    const float tops[] = {180, 145, 185};
    const float lefts[] = {80, 225, 350};
    const float widths[] = {105, 95, 75};

    for (int i = 0; i < 3; i++) {
        if (x + w > lefts[i] && x < lefts[i] + widths[i] &&
            oldY + 180 <= tops[i] &&
            newY + 180 >= tops[i]) {
            return (int)tops[i];
        }
    }
    return 0;
}

static void updatePlayer(void)
{
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    player.animTime += 1.0f / 60.0f;

    if (player.attackTimer > 0) player.attackTimer--;
    if (player.dashTimer > 0) player.dashTimer--;
    if (targetFlash > 0) targetFlash--;

    float oldY = player.y;
    float oldFeet = player.y + 180.0f;

    int left = (pad.Buttons & PSP_CTRL_LEFT) != 0;
    int right = (pad.Buttons & PSP_CTRL_RIGHT) != 0;
    int down = (pad.Buttons & PSP_CTRL_DOWN) != 0;

    if (player.dashTimer > 0) {
        player.vx = player.direction * 7.5f;
        player.anim = DASH;
    } else {
        player.vx = 0;

        if (left) {
            player.vx = -2.8f;
            player.direction = -1;
        }
        if (right) {
            player.vx = 2.8f;
            player.direction = 1;
        }

        if (player.attackTimer > 0) {
            player.anim = down ? DOWN_ATTACK : HIT;
        } else if (!player.grounded) {
            player.anim = JUMP;
        } else if (player.vx != 0) {
            player.anim = RUN;
        } else {
            player.anim = IDLE;
        }
    }

    if ((pad.Buttons & PSP_CTRL_CROSS) && player.grounded) {
        player.vy = -8.0f;
        player.grounded = 0;
    }

    if ((pad.Buttons & PSP_CTRL_RTRIGGER) && player.dashTimer == 0) {
        player.dashTimer = 10;
        player.vy = 0;
    }

    if ((pad.Buttons & PSP_CTRL_SQUARE) && player.attackTimer == 0) {
        player.attackTimer = 12;

        if (down && !player.grounded) {
            player.anim = DOWN_ATTACK;
            player.vy = 4.5f;
        } else {
            player.anim = HIT;
        }

        /* Dano simples no boneco de treino. */
        if (player.x > 350 && player.x < 425) {
            if (targetHP > 0) targetHP--;
            targetFlash = 8;
            if (targetHP <= 0) targetHP = 5;
        }
    }

    if (player.dashTimer == 0) {
        player.vy += 0.38f;
        if (player.vy > 8.0f) player.vy = 8.0f;
    }

    player.x += player.vx;
    player.y += player.vy;

    if (player.x < 15) player.x = 15;
    if (player.x > 410) player.x = 410;

    player.grounded = 0;

    /* Colisão com plataformas ao cair. */
    int platformY = onPlatform(player.x, oldY, player.y, 60);

    if (platformY && player.vy >= 0) {
        player.y = (float)platformY - 180.0f;
        player.vy = 0;
        player.grounded = 1;
    }

    /* Colisão com o chão. */
    if (player.y + 180.0f >= FLOOR_Y) {
        player.y = FLOOR_Y - 180.0f;
        player.vy = 0;
        player.grounded = 1;
    }

    (void)oldFeet;
}

/* ---------- INICIALIZACAO PSP GU ---------- */

static void initGU(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(GU_PSM_8888, (void*)0, W);
    sceGuDispBuffer(W, H, (void*)0x88000, W);
    sceGuDepthBuffer((void*)0x110000, W);

    sceGuOffset(2048 - W / 2, 2048 - H / 2);
    sceGuViewport(2048, 2048, W, H);
    sceGuDepthRange(0xC350, 0x2710);

    sceGuScissor(0, 0, W, H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_SMOOTH);

    sceGuClearColor(0xFFF0F0E8);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

int main(void)
{
    memset(&player, 0, sizeof(player));

    player.x = 45;
    player.y = FLOOR_Y - 180;
    player.scale = 1.0f;
    player.direction = 1;
    player.anim = IDLE;
    player.grounded = 1;
    player.hp = 4;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initGU();

    while (running) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_START) {
            running = 0;
        }

        updatePlayer();

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFFF0F0E8);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        drawBackground();
        drawTarget();
        drawPlayer(&player);

        /* Indicador de vida do boneco de treino. */
        for (int i = 0; i < targetHP; i++) {
            rect(380 + i * 10, 24, 7, 5, RED);
        }

        /* Indicador de vida do protagonista. */
        for (int i = 0; i < player.hp; i++) {
            rect(20 + i * 13, 20, 10, 7, BLACK);
        }

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
