
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <math.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512

#define WHITE 0xFFFFFFFF
#define GRAY  0xFF999999
#define DARK  0xFF242424
#define BLACK 0xFF050505

#define PI 3.14159265f

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef enum {
    IDLE,
    WALK,
    RUN,
    JUMP,
    FALL,
    LAND,
    CLIMB,
    ATTACK
} Animation;

typedef struct {
    float x, y;
    float vx, vy;
    float time;
    float landTime;
    int facing;
    int grounded;
    Animation anim;
} Player;

static unsigned int __attribute__((aligned(16))) displayList[262144];

/* =========================
   FUNCOES GEOMETRICAS
   ========================= */

static float clampf(float v, float min, float max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static void triangle(float x1, float y1,
                     float x2, float y2,
                     float x3, float y3,
                     unsigned int color)
{
    Vertex v[3] = {
        {color, x1, y1, 0.0f},
        {color, x2, y2, 0.0f},
        {color, x3, y3, 0.0f}
    };

    sceGuDisable(GU_TEXTURE_2D);

    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        3, 0, v
    );
}

static void quad(float ax, float ay,
                 float bx, float by,
                 float cx, float cy,
                 float dx, float dy,
                 unsigned int color)
{
    triangle(ax, ay, bx, by, cx, cy, color);
    triangle(ax, ay, cx, cy, dx, dy, color);
}

static void shard(float x, float y, float size,
                  unsigned int color, float tilt)
{
    triangle(
        x, y-size,
        x+size*0.65f+tilt, y+size*0.20f,
        x-size*0.35f, y+size*0.35f,
        color
    );
}

/* =========================
   INICIALIZACAO DO JOGADOR
   ========================= */

static void playerInit(Player *p)
{
    p->x = 160.0f;
    p->y = 220.0f;

    p->vx = 0.0f;
    p->vy = 0.0f;

    p->time = 0.0f;
    p->landTime = 0.0f;

    p->facing = 1;
    p->grounded = 1;
    p->anim = IDLE;
}

/* =========================
   MOVIMENTO E ANIMACOES
   ========================= */

static void playerUpdate(Player *p, float dt,
                         unsigned int buttons)
{
    float speed = 0.0f;

    dt = clampf(dt, 0.0f, 0.04f);
    p->time += dt;

    /* Caminhar: analógico ou direcional */
    if (buttons & PSP_CTRL_LEFT) {
        speed = -55.0f;
        p->facing = -1;
    }

    if (buttons & PSP_CTRL_RIGHT) {
        speed = 55.0f;
        p->facing = 1;
    }

    /* Correr mais rápido com R pressionado */
    if (buttons & PSP_CTRL_RTRIGGER) {
        if (speed < 0) speed = -115.0f;
        if (speed > 0) speed = 115.0f;
    }

    p->vx = speed;

    /* Pular com O */
    if ((buttons & PSP_CTRL_CIRCLE) && p->grounded) {
        p->vy = -175.0f;
        p->grounded = 0;
        p->landTime = 0.0f;
    }

    /* Física vertical */
    if (!p->grounded) {
        p->vy += 360.0f * dt;
        p->y += p->vy * dt;

        /* Piso provisório da demonstração */
        if (p->y >= 220.0f) {
            p->y = 220.0f;
            p->vy = 0.0f;
            p->grounded = 1;
            p->landTime = 0.18f;
        }
    }

    /* Movimento horizontal */
    p->x += p->vx * dt;
    p->x = clampf(p->x, 20.0f, 460.0f);

    /* Seleção da animação */
    if (!p->grounded) {
        if (p->vy < 0.0f)
            p->anim = JUMP;
        else
            p->anim = FALL;
    }
    else if (p->landTime > 0.0f) {
        p->landTime -= dt;
        p->anim = LAND;
    }
    else if (buttons & PSP_CTRL_UP) {
        p->anim = CLIMB;
    }
    else if (buttons & PSP_CTRL_CROSS) {
        p->anim = ATTACK;
    }
    else if (speed != 0.0f) {
        if (buttons & PSP_CTRL_RTRIGGER)
            p->anim = RUN;
        else
            p->anim = WALK;
    }
    else {
        p->anim = IDLE;
    }
}

/* =========================
   DESENHO DO PROTAGONISTA
   ========================= */

static void playerDraw(Player *p,
                       float cameraX,
                       float cameraY)
{
    float x = p->x-cameraX;
    float y = p->y-cameraY;
    float t = p->time;
    float d = (float)p->facing;

    float walking = (p->anim == WALK);
    float running = (p->anim == RUN);
    float moving = walking || running;

    float jumping = (p->anim == JUMP);
    float falling = (p->anim == FALL);
    float landing = (p->anim == LAND);
    float climbing = (p->anim == CLIMB);
    float attacking = (p->anim == ATTACK);
    float idle = (p->anim == IDLE);

    /* Frequência diferente para caminhar e correr */
    float frequency = walking ? 7.0f : 12.0f;

    float step = sinf(t*frequency)*moving;
    float opposite = sinf(t*frequency+PI)*moving;

    float bob = fabsf(sinf(t*frequency)) *
                (walking ? 1.5f : 3.0f)*moving;

    /* Inclinação maior durante corrida */
    float lean = walking ? 9.0f : 15.0f;
    if (!moving) lean = 2.0f;

    float bx = x+d*lean*0.35f;
    float by = y-bob+7.0f*landing-5.0f*jumping;

    /* Sombra */
    triangle(
        x-15, y+2,
        x+15, y+2,
        x, y+5,
        DARK
    );

    /* =====================
       ALMA DO NEGRO
       ===================== */

    float fx = bx+d*1.5f+
               sinf(t*3.5f)*moving*2.0f;

    float fy = by-48.0f+
               sinf(t*frequency)*moving*2.0f;

    shard(fx-5, fy, 10, GRAY, sinf(t*5)*2);
    shard(fx+2, fy-5, 14, WHITE, sinf(t*4)*3);
    shard(fx+7, fy+1, 7, WHITE, sinf(t*8)*2);
    shard(fx-12, fy+7, 5, GRAY, sinf(t*6));

    shard(
        fx-16+sinf(t*4)*2,
        fy-12+sinf(t*5)*3,
        3.5f, WHITE, 0
    );

    shard(
        fx+15+sinf(t*6)*2,
        fy-8+sinf(t*4)*3,
        3.0f, GRAY, 0
    );

    /* =====================
       CABECA ANGULAR
       ===================== */

    quad(
        bx-8, by-43,
        bx+7, by-45,
        bx+10+d*2, by-32,
        bx-5+d*2, by-29,
        DARK
    );

    /* Olho */
    quad(
        bx+d, by-39,
        bx+d*7, by-40,
        bx+d*7, by-37,
        bx+d, by-36,
        WHITE
    );

    /* Pescoço */
    triangle(
        bx-5, by-31,
        bx+6, by-32,
        bx+2, by-24,
        GRAY
    );

    /* =====================
       CAPA / RASTRO
       ===================== */

    triangle(
        bx-d*4, by-28,
        bx-d*22, by-24+step*2,
        bx-d*16, by-15,
        GRAY
    );

    triangle(
        bx-d*3, by-25,
        bx-d*(walking ? 22 : 29),
        by-17+opposite*2,
        bx-d*13, by-12,
        DARK
    );

    /* =====================
       TRONCO INCLINADO
       ===================== */

    quad(
        bx-7, by-27,
        bx+7, by-25,
        bx+d*10, by-8,
        bx-d*6, by-7,
        DARK
    );

    triangle(
        bx-5, by-25,
        bx+3, by-24,
        bx-d, by-14,
        GRAY
    );

    triangle(
        bx+3, by-23,
        bx+7, by-20,
        bx+d*7, by-10,
        BLACK
    );

    /* =====================
       BRACOS ARTICULADOS
       ===================== */

    float sx = bx+d*5;
    float sy = by-23;

    float arm1x = sx+d*(5+opposite*5);
    float arm1y = sy+9+step*2;

    float arm2x = sx-d*(5+step*5);
    float arm2y = sy+8+opposite*2;

    if (jumping) {
        arm1y -= 8;
        arm2y -= 5;
    }

    if (falling) {
        arm1y -= 3;
        arm2y -= 3;
    }

    if (climbing) {
        arm1x = sx+d*8;
        arm1y = sy-12;
        arm2x = sx-d*8;
        arm2y = sy-9;
    }

    if (attacking) {
        arm1x = sx+d*18;
        arm1y = sy-8;
    }

    triangle(
        sx, sy,
        arm1x, arm1y,
        arm1x-d*3, arm1y+5,
        GRAY
    );

    triangle(
        arm1x, arm1y,
        arm1x+d*3, arm1y+2,
        arm1x+d*2, arm1y+6,
        WHITE
    );

    triangle(
        sx-d*2, sy+1,
        arm2x, arm2y,
        arm2x-d*2, arm2y+5,
        DARK
    );

    triangle(
        arm2x, arm2y,
        arm2x-d*3, arm2y+2,
        arm2x, arm2y+6,
        GRAY
    );

    /* =====================
       QUADRIL
       ===================== */

    triangle(
        bx-6, by-10,
        bx+d*8, by-10,
        bx+d*5, by-3,
        DARK
    );

    /* =====================
       PERNAS ANIMADAS
       ===================== */

    float hipX = bx;
    float hipY = by-5;

    float air = 7.0f*jumping-5.0f*falling;

    float lx = hipX+d*(step*(walking ? 5 : 8)+air);
    float ly = hipY+10+falling*3;

    float fx1 = lx+d*(5+step*3);
    float fy1 = y-1-jumping*8+falling*2;

    float rx = hipX-d*(opposite*(walking ? 5 : 8)+air);
    float ry = hipY+10;

    float fx2 = rx-d*(4-opposite*3);
    float fy2 = y-1-jumping*2+falling*5;

    if (landing) {
        ly += 3;
        ry += 3;
        fy1 -= 1;
        fy2 -= 1;
    }

    /* Perna de trás */
    triangle(
        hipX-d*2, hipY,
        rx, ry,
        rx+d*4, ry+8,
        DARK
    );

    triangle(
        rx, ry,
        fx2+d*2, fy2-2,
        fx2-d*3, fy2+1,
        GRAY
    );

    triangle(
        fx2-d*5, fy2,
        fx2+d*4, fy2,
        fx2+d*3, fy2+3,
        BLACK
    );

    /* Perna da frente */
    triangle(
        hipX+d*2, hipY,
        lx, ly,
        lx+d*5, ly+5,
        GRAY
    );

    triangle(
        lx, ly,
        fx1-d*2, fy1-2,
        fx1+d*3, fy1+1,
        DARK
    );

    triangle(
        fx1-d*4, fy1,
        fx1+d*6, fy1,
        fx1+d*5, fy1+3,
        WHITE
    );

    /* =====================
       EFEITOS DE MOVIMENTO
       ===================== */

    if (moving) {
        shard(
            x-d*20, y-12+sinf(t*frequency),
            2, GRAY, 0
        );

        shard(
            x-d*26, y-7+sinf(t*frequency+1),
            2, WHITE, 0
        );
    }

    if (idle) {
        shard(
            fx+sinf(t*2)*3,
            fy-12, 3, WHITE, 0
        );
    }

    if (landing) {
        triangle(
            x-17, y+1,
            x-8, y-2,
            x-11, y+2,
            WHITE
        );

        triangle(
            x+17, y+1,
            x+8, y-2,
            x+11, y+2,
            GRAY
        );
    }

    if (attacking) {
        triangle(
            x+d*13, by-23,
            x+d*31, by-13,
            x+d*18, by-8,
            WHITE
        );

        triangle(
            x+d*18, by-30,
            x+d*35, by-21,
            x+d*25, by-17,
            GRAY
        );
    }
}

/* =========================
   GRAFICOS PSP
   ========================= */

static void graphicsInit(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void*)0,
        BUF_W
    );

    sceGuDispBuffer(
        SCREEN_W,
        SCREEN_H,
        (void*)0x88000,
        BUF_W
    );

    sceGuDepthBuffer((void*)0x110000, BUF_W);

    sceGuOffset(
        2048-SCREEN_W/2,
        2048-SCREEN_H/2
    );

    sceGuViewport(2048,2048,SCREEN_W,SCREEN_H);
    sceGuScissor(0,0,SCREEN_W,SCREEN_H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuClearColor(BLACK);

    sceGuFinish();
    sceGuSync(0,0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* =========================
   PROGRAMA PRINCIPAL
   ========================= */

int main(void)
{
    Player player;
    SceCtrlData pad;

    float dt = 1.0f/60.0f;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    graphicsInit();
    playerInit(&player);

    while (1) {
        sceCtrlReadBufferPositive(&pad,1);

        if (pad.Buttons & PSP_CTRL_START)
            break;

        playerUpdate(
            &player,
            dt,
            pad.Buttons
        );

        sceGuStart(GU_DIRECT,displayList);

        sceGuClearColor(BLACK);
        sceGuClear(
            GU_COLOR_BUFFER_BIT |
            GU_DEPTH_BUFFER_BIT
        );

        /* Cenario provisório */
        triangle(0,180,80,130,145,180,DARK);
        triangle(80,130,145,180,180,155,GRAY);

        triangle(320,40,410,40,365,105,DARK);
        triangle(365,105,410,40,455,115,GRAY);

        quad(
            0,222,
            SCREEN_W,222,
            SCREEN_W,SCREEN_H,
            0,SCREEN_H,
            BLACK
        );

        quad(
            0,220,
            SCREEN_W,220,
            SCREEN_W,223,
            0,223,
            GRAY
        );

        /* Câmera parada nesta demonstração */
        playerDraw(&player,0,0);

        sceGuFinish();
        sceGuSync(0,0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
