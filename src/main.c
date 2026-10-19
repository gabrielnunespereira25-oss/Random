
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
    float x = p->x-camX, y = p->y-camY;
    float t = p->time, d = (float)p->facing;
    float run = p->anim == RUN;
    float jump = p->anim == JUMP;
    float fall = p->anim == FALL;
    float land = p->anim == LAND;
    float climb = p->anim == CLIMB;
    float attack = p->anim == ATTACK;

    float stride = sinf(t*12)*run;
    float swing = sinf(t*12+PI)*run;
    float bob = fabsf(sinf(t*12))*3*run;
    float bx = x+d*(7*run+4*attack)*.3f;
    float by = y-bob+7*land-5*jump;

    /* Sombra */
    triangle(x-15,y+2,x+15,y+2,x,y+5,DARK);

    /* Alma do Negro: chama facetada e fragmentos animados */
    {
        float fx=bx+d*1.5f, fy=by-48;
        shard(fx-5,fy+sinf(t*9)*2,10,GRAY,sinf(t*7)*2);
        shard(fx+2,fy-5+sinf(t*8),14,WHITE,sinf(t*6)*3);
        shard(fx+7,fy+1,7,WHITE,sinf(t*10)*2);
        shard(fx-12,fy+7,5,GRAY,sinf(t*11));
        shard(fx-16+sinf(t*4)*2,fy-12+sinf(t*5)*3,
              3.5f,WHITE,0);
        shard(fx+15+sinf(t*6)*2,fy-8+sinf(t*4)*3,
              3,GRAY,0);
    }

    /* Cabeça angular e olho */
    quad(bx-8,by-43,bx+7,by-45,bx+10,by-32,
         bx-5,by-29,DARK);
    quad(bx+d,by-39,bx+d*7,by-40,bx+d*7,by-37,
         bx+d,by-36,WHITE);

    /* Pescoço e capa */
    triangle(bx-5,by-31,bx+6,by-32,bx+2,by-24,GRAY);
    triangle(bx-d*4,by-28,bx-d*24,by-24+stride*2,
             bx-d*17,by-15,GRAY);
    triangle(bx-d*3,by-25,bx-d*29,by-17+swing,
             bx-d*14,by-12,DARK);

    /* Tronco inclinado em facetas */
    quad(bx-7,by-27,bx+7,by-25,bx+d*10,by-8,
         bx-d*6,by-7,DARK);
    triangle(bx-5,by-25,bx+3,by-24,bx-d,by-14,GRAY);
    triangle(bx+3,by-23,bx+7,by-20,bx+d*7,by-10,BLACK);

    /* Braços articulados */
    {
        float sx=bx+d*5, sy=by-23;
        float fx=sx+d*(8+swing*5);
        float fy=sy+8-attack*9;
        float rx=sx-d*(9+swing*4);
        float ry=sy+7+swing*3;

        if (jump) { fy-=8; ry-=5; }
        if (climb) {
            fx=sx+d*8; fy=sy-12;
            rx=sx-d*8; ry=sy-9;
        }

        triangle(sx,sy,fx,fy,fx-d*3,fy+5,GRAY);
        triangle(fx,fy,fx+d*4,fy+2,fx+d*2,fy+6,WHITE);
        triangle(sx-d*2,sy+1,rx,ry,rx-d*2,ry+5,DARK);
        triangle(rx,ry,rx-d*3,ry+2,rx,ry+6,GRAY);
    }

    /* Quadril */
    triangle(bx-6,by-10,bx+d*8,by-10,bx+d*5,by-3,DARK);

    /* Pernas com passada alternada */
    {
        float hx=bx, hy=by-5;
        float air=7*jump-5*fall;

        float lx=hx+d*(stride*8+air);
        float ly=hy+10+fall*3;
        float fx=lx+d*(5+stride*3);
        float fy=y-1-jump*8+fall*2;

        float rx=hx-d*(stride*8+air);
        float ry=hy+10;
        float tx=rx-d*(4-stride*3);
        float ty=y-1-jump*2+fall*5;

        triangle(hx-d*2,hy,rx,ry,rx+d*4,ry+8,DARK);
        triangle(rx,ry,tx+d*2,ty-2,tx-d*3,ty+1,GRAY);
        triangle(tx-d*5,ty,tx+d*4,ty,tx+d*3,ty+3,BLACK);

        triangle(hx+d*2,hy,lx,ly,lx+d*5,ly+5,GRAY);
        triangle(lx,ly,fx-d*2,fy-2,fx+d*3,fy+1,DARK);
        triangle(fx-d*4,fy,fx+d*6,fy,fx+d*5,fy+3,WHITE);
    }

    /* Rastro da corrida */
    if (run) {
        shard(x-d*22,y-17+sinf(t*15),3,GRAY,0);
        shard(x-d*30,y-8+sinf(t*12),2,WHITE,0);
    }

    /* Impacto ao aterrissar */
    if (land) {
        triangle(x-17,y+1,x-8,y-2,x-11,y+2,WHITE);
        triangle(x+17,y+1,x+8,y-2,x+11,y+2,GRAY);
    }

    /* Golpe geométrico */
    if (attack) {
        triangle(x+d*13,by-23,x+d*31,by-13,
                 x+d*18,by-8,WHITE);
        triangle(x+d*18,by-30,x+d*35,by-21,
                 x+d*25,by-17,GRAY);
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
