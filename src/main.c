
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <math.h>

PSP_MODULE_INFO("RANDOM_ALPHA", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define W 480
#define H 272
#define BW 512
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
    IDLE, RUN, JUMP, FALL, LAND, CLIMB, ATTACK
} Animation;

typedef struct {
    float x, y, vx, vy, time, landTime;
    int facing, grounded;
    Animation anim;
} Player;

static unsigned int __attribute__((aligned(16))) list[262144];

static float clampf(float v, float a, float b)
{
    return v < a ? a : (v > b ? b : v);
}

static void triangle(float x1, float y1, float x2, float y2,
                     float x3, float y3, unsigned int color)
{
    Vertex v[3] = {
        {color, x1, y1, 0},
        {color, x2, y2, 0},
        {color, x3, y3, 0}
    };

    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        3, 0, v);
}

static void quad(float ax, float ay, float bx, float by,
                 float cx, float cy, float dx, float dy,
                 unsigned int color)
{
    triangle(ax, ay, bx, by, cx, cy, color);
    triangle(ax, ay, cx, cy, dx, dy, color);
}

static void shard(float x, float y, float s,
                  unsigned int color, float tilt)
{
    triangle(x, y-s, x+s*.65f+tilt, y+s*.2f,
             x-s*.35f, y+s*.35f, color);
}

static void playerInit(Player *p)
{
    p->x = 160;
    p->y = 220;
    p->vx = p->vy = p->time = p->landTime = 0;
    p->facing = 1;
    p->grounded = 1;
    p->anim = IDLE;
}

static void playerUpdate(Player *p, float dt, unsigned int buttons)
{
    dt = clampf(dt, 0, .04f);
    p->time += dt;
    p->vx = 0;

    if (buttons & PSP_CTRL_LEFT) {
        p->vx = -105;
        p->facing = -1;
    }
    if (buttons & PSP_CTRL_RIGHT) {
        p->vx = 105;
        p->facing = 1;
    }

    if ((buttons & PSP_CTRL_CIRCLE) && p->grounded) {
        p->vy = -175;
        p->grounded = 0;
        p->landTime = 0;
    }

    if (!p->grounded) {
        p->vy += 360 * dt;
        p->y += p->vy * dt;

        /* Piso provisório para esta demonstração. */
        if (p->y >= 220) {
            p->y = 220;
            p->vy = 0;
            p->grounded = 1;
            p->landTime = .15f;
        }
    }

    p->x += p->vx * dt;
    p->x = clampf(p->x, 20, 460);

    if (buttons & PSP_CTRL_UP)
        p->anim = CLIMB;
    else if (buttons & PSP_CTRL_CROSS)
        p->anim = ATTACK;
    else if (!p->grounded)
        p->anim = p->vy < 0 ? JUMP : FALL;
    else if (p->landTime > 0) {
        p->landTime -= dt;
        p->anim = LAND;
    } else if (p->vx != 0)
        p->anim = RUN;
    else
        p->anim = IDLE;
}


static void playerDraw(Player *p, float camX, float camY)
{
    float x = p->x - camX;
    float y = p->y - camY;
    float t = p->time;
    float d = (float)p->facing;

    int walking = (p->anim == RUN);
    int idle = (p->anim == IDLE);

    /* Passos mais lentos para caminhada */
    float step = sinf(t * 7.0f) * walking;
    float opposite = sinf(t * 7.0f + PI) * walking;

    /* Inclinação constante durante a caminhada */
    float lean = walking ? 9.0f : 2.0f;
    float bob = fabsf(sinf(t * 7.0f)) * 2.0f * walking;
    float bx = x + d * lean * 0.35f;
    float by = y - bob;

    /* Sombra */
    triangle(x - 13, y + 2, x + 13, y + 2,
             x, y + 5, DARK);

    /* Alma do Negro balança com o movimento */
    float fx = bx + d * 1.5f + sinf(t * 3.5f) * walking * 2.0f;
    float fy = by - 48 + sinf(t * 7.0f) * walking * 2.0f;

    shard(fx - 5, fy, 10, GRAY, sinf(t * 5.0f) * 2.0f);
    shard(fx + 2, fy - 5, 14, WHITE, sinf(t * 4.0f) * 3.0f);
    shard(fx + 7, fy + 1, 7, WHITE, sinf(t * 8.0f) * 2.0f);
    shard(fx - 12, fy + 7, 5, GRAY, sinf(t * 6.0f));

    /* Cabeça inclinada para a frente */
    quad(bx - 8, by - 43,
         bx + 7, by - 45,
         bx + 10 + d * 2, by - 32,
         bx - 5 + d * 2, by - 29, DARK);

    /* Olho */
    quad(bx + d * 1, by - 39,
         bx + d * 7, by - 40,
         bx + d * 7, by - 37,
         bx + d * 1, by - 36, WHITE);

    /* Pescoço */
    triangle(bx - 5, by - 31,
             bx + 6, by - 32,
             bx + 2, by - 24, GRAY);

    /* Capa arrastada para trás */
    triangle(bx - d * 4, by - 28,
             bx - d * 22, by - 24 + step * 2,
             bx - d * 16, by - 15, GRAY);

    triangle(bx - d * 3, by - 25,
             bx - d * 27, by - 17 + opposite * 2,
             bx - d * 13, by - 12, DARK);

    /* Tronco inclinado */
    quad(bx - 7, by - 27,
         bx + 7, by - 25,
         bx + d * 10, by - 8,
         bx - d * 6, by - 7, DARK);

    triangle(bx - 5, by - 25,
             bx + 3, by - 24,
             bx - d, by - 14, GRAY);

    /* Braços balançam em sentidos opostos */
    float sx = bx + d * 5;
    float sy = by - 23;

    float arm1x = sx + d * (5 + opposite * 5);
    float arm1y = sy + 9 + step * 2;

    float arm2x = sx - d * (5 + step * 5);
    float arm2y = sy + 8 + opposite * 2;

    triangle(sx, sy, arm1x, arm1y,
             arm1x - d * 3, arm1y + 5, GRAY);

    triangle(arm1x, arm1y,
             arm1x + d * 3, arm1y + 2,
             arm1x + d * 2, arm1y + 6, WHITE);

    triangle(sx - d * 2, sy + 1,
             arm2x, arm2y,
             arm2x - d * 2, arm2y + 5, DARK);

    triangle(arm2x, arm2y,
             arm2x - d * 3, arm2y + 2,
             arm2x, arm2y + 6, GRAY);

    /* Quadril */
    triangle(bx - 6, by - 10,
             bx + d * 8, by - 10,
             bx + d * 5, by - 3, DARK);

    /* Perna esquerda */
    float hipX = bx;
    float hipY = by - 5;

    float lx = hipX + d * step * 7;
    float ly = hipY + 10;

    triangle(hipX - d * 2, hipY,
             lx, ly,
             lx + d * 4, ly + 8, DARK);

    triangle(lx, ly,
             lx + d * (4 + step * 3), y - 2,
             lx + d * (9 + step * 3), y + 1, GRAY);

    /* Perna direita */
    float rx = hipX - d * opposite * 7;
    float ry = hipY + 10;

    triangle(hipX + d * 2, hipY,
             rx, ry,
             rx - d * 4, ry + 8, GRAY);

    triangle(rx, ry,
             rx - d * (4 - opposite * 3), y - 2,
             rx - d * (9 - opposite * 3), y + 1, DARK);

    /* Pés angulares */
    triangle(lx + d * step * 2, y - 1,
             lx + d * (10 + step * 3), y - 1,
             lx + d * (8 + step * 3), y + 2, WHITE);

    triangle(rx - d * opposite * 2, y - 1,
             rx - d * (10 - opposite * 3), y - 1,
             rx - d * (8 - opposite * 3), y + 2, GRAY);

    /* Pequenos fragmentos de movimento */
    if (walking) {
        shard(x - d * 20, y - 12 + sinf(t * 7), 2, GRAY, 0);
        shard(x - d * 26, y - 7 + sinf(t * 6), 2, WHITE, 0);
    }

    /* Quando parado, a alma continua viva */
    if (idle) {
        shard(fx + sinf(t * 2) * 3, fy - 12, 3, WHITE, 0);
    }
}

    }
}

static void graphicsInit(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,BW);
    sceGuDispBuffer(W,H,(void*)0x88000,BW);
    sceGuDepthBuffer((void*)0x110000,BW);
    sceGuOffset(2048-W/2,2048-H/2);
    sceGuViewport(2048,2048,W,H);
    sceGuScissor(0,0,W,H);
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

int main(void)
{
    Player p;
    SceCtrlData pad;
    float dt=1.0f/60.0f;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    graphicsInit();
    playerInit(&p);

    while (1) {
        sceCtrlReadBufferPositive(&pad,1);
        if (pad.Buttons & PSP_CTRL_START) break;

        playerUpdate(&p,dt,pad.Buttons);

        sceGuStart(GU_DIRECT,list);
        sceGuClearColor(BLACK);
        sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);

        /* Fundo demonstrativo monocromático */
        triangle(0,180,80,130,145,180,DARK);
        triangle(80,130,145,180,180,155,GRAY);
        triangle(320,40,410,40,365,105,DARK);
        triangle(365,105,410,40,455,115,GRAY);

        quad(0,222,480,222,480,272,0,272,BLACK);
        quad(0,220,480,220,480,223,0,223,GRAY);

        playerDraw(&p,0,0);

        sceGuFinish();
        sceGuSync(0,0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
