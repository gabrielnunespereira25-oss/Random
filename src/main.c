
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define W 480
#define H 272
#define PITCH 512
#define FLOOR_Y 230.0f

#define BLACK 0xFF080808
#define DARK  0xFF242424
#define GRAY  0xFF626262
#define LIGHT 0xFFAAAAAA
#define WHITE 0xFFF5F5F0

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, vx, vy;
    float scale, animTime;
    int direction, anim, grounded;
    int attackTimer, dashTimer, hp;
} Player;

enum { IDLE, RUN, HIT, JUMP, DASH };

static unsigned int __attribute__((aligned(16))) displayList[262144];
static Player player;
static int running = 1;

/* ===== FORMAS GEOMETRICAS ===== */

static void rect(float x, float y, float w, float h,
                 unsigned int color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 2);
    if (!v) return;

    v[0] = (Vertex){color, x, y, 0};
    v[1] = (Vertex){color, x + w, y + h, 0};

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, NULL, v
    );
}

static void triangle(float x1, float y1,
                     float x2, float y2,
                     float x3, float y3,
                     unsigned int color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 3);
    if (!v) return;

    v[0] = (Vertex){color, x1, y1, 0};
    v[1] = (Vertex){color, x2, y2, 0};
    v[2] = (Vertex){color, x3, y3, 0};

    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        3, NULL, v
    );
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

    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 4);
    if (!v) return;

    v[0] = (Vertex){color, x1 + px, y1 + py, 0};
    v[1] = (Vertex){color, x2 + px, y2 + py, 0};
    v[2] = (Vertex){color, x2 - px, y2 - py, 0};
    v[3] = (Vertex){color, x1 - px, y1 - py, 0};

    sceGuDrawArray(
        GU_TRIANGLE_FAN,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        4, NULL, v
    );
}

/* ===== CABELO ===== */

static void hair(float x, float y, float s)
{
    triangle(x+10*s,y+43*s, x+9*s,y+15*s,
             x+22*s,y+4*s, BLACK);
    triangle(x+15*s,y+39*s, x+30*s,y+1*s,
             x+35*s,y+40*s, BLACK);
    triangle(x+27*s,y+38*s, x+43*s,y+12*s,
             x+49*s,y+35*s, DARK);
    triangle(x+12*s,y+35*s, x+26*s,y+24*s,
             x+42*s,y+41*s, GRAY);
    triangle(x+9*s,y+39*s, x+17*s,y+53*s,
             x+8*s,y+51*s, BLACK);
    rect(x+13*s,y+32*s,34*s,11*s,BLACK);
}

/* ===== ROSTO ===== */

static void face(float x, float y, float s)
{
    triangle(x+15*s,y+37*s, x+40*s,y+37*s,
             x+38*s,y+60*s, WHITE);
    triangle(x+15*s,y+37*s, x+38*s,y+60*s,
             x+26*s,y+70*s, WHITE);
    triangle(x+15*s,y+37*s, x+26*s,y+70*s,
             x+12*s,y+58*s, WHITE);
    triangle(x+36*s,y+41*s, x+40*s,y+39*s,
             x+37*s,y+57*s, LIGHT);
}

/* ===== GOLA E TRONCO ===== */

static void collar(float x, float y, float s)
{
    rect(x+22*s,y+65*s,12*s,12*s,DARK);
    triangle(x+11*s,y+72*s, x+43*s,y+72*s,
             x+37*s,y+91*s,BLACK);
    triangle(x+11*s,y+72*s, x+37*s,y+91*s,
             x+7*s,y+84*s,GRAY);
    triangle(x+15*s,y+74*s, x+36*s,y+75*s,
             x+25*s,y+83*s,WHITE);
}

static void torso(float x, float y, float s)
{
    triangle(x+13*s,y+82*s, x+25*s,y+77*s,
             x+13*s,y+111*s,DARK);
    triangle(x+25*s,y+77*s, x+43*s,y+83*s,
             x+39*s,y+113*s,BLACK);
    triangle(x+13*s,y+82*s, x+39*s,y+83*s,
             x+37*s,y+120*s,BLACK);
    triangle(x+13*s,y+82*s, x+37*s,y+120*s,
             x+13*s,y+117*s,DARK);

    triangle(x+13*s,y+86*s, x+22*s,y+91*s,
             x+39*s,y+82*s,GRAY);
    triangle(x+13*s,y+86*s, x+17*s,y+100*s,
             x+22*s,y+91*s,LIGHT);

    rect(x+13*s,y+116*s,27*s,6*s,DARK);
    rect(x+17*s,y+119*s,19*s,3*s,GRAY);
}

/* ===== BRACOS ===== */

static void arms(float x, float y, float s, float swing)
{
    triangle(x+13*s,y+84*s, x+23*s,y+87*s,
             x+20*s,y+108*s,BLACK);
    triangle(x+13*s,y+84*s, x+20*s,y+108*s,
             x+11*s,y+105*s,GRAY);

    line(x+15*s,y+105*s,
         x+9*s,y+128*s+swing,6*s,DARK);
    line(x+9*s,y+128*s+swing,
         x+7*s,y+144*s+swing,4*s,WHITE);

    rect(x+3*s,y+140*s+swing,9*s,9*s,BLACK);
    rect(x+5*s,y+144*s+swing,5*s,5*s,WHITE);

    triangle(x+36*s,y+83*s, x+46*s,y+89*s,
             x+47*s,y+108*s,BLACK);
    triangle(x+36*s,y+83*s, x+47*s,y+108*s,
             x+37*s,y+103*s,DARK);

    line(x+43*s,y+105*s,
         x+49*s,y+126*s-swing,7*s,BLACK);
    line(x+49*s,y+126*s-swing,
         x+56*s,y+146*s-swing,5*s,DARK);
    line(x+56*s,y+146*s-swing,
         x+60*s,y+161*s-swing,3*s,BLACK);

    triangle(x+55*s,y+155*s-swing,
             x+63*s,y+158*s-swing,
             x+59*s,y+169*s-swing,BLACK);
    triangle(x+55*s,y+155*s-swing,
             x+59*s,y+169*s-swing,
             x+54*s,y+163*s-swing,WHITE);
}

/* ===== CALCA E TENIS ===== */

static void legs(float x, float y, float s, float step)
{
    triangle(x+13*s,y+120*s, x+28*s,y+120*s,
             x+25*s,y+147*s,BLACK);
    triangle(x+13*s,y+120*s, x+25*s,y+147*s,
             x+8*s,y+162*s,DARK);
    triangle(x+8*s,y+162*s, x+25*s,y+147*s,
             x+23*s,y+174*s,GRAY);
    triangle(x+8*s,y+162*s, x+23*s,y+174*s,
             x+10*s,y+174*s,WHITE);

    triangle(x+27*s,y+120*s, x+40*s,y+120*s,
             x+48*s,y+158*s,DARK);
    triangle(x+27*s,y+120*s, x+48*s,y+158*s,
             x+37*s,y+172*s,GRAY);

    triangle(x+29*s,y+125*s, x+34*s,y+128*s,
             x+44*s,y+158*s,WHITE);
    triangle(x+34*s,y+128*s, x+44*s,y+158*s,
             x+40*s,y+162*s,LIGHT);

    line(x+15*s,y+171*s,
         x+14*s+step,y+178*s,4*s,BLACK);
    line(x+42*s,y+171*s,
         x+43*s-step,y+178*s,4*s,BLACK);

    triangle(x+8*s+step,y+176*s,
             x+20*s+step,y+176*s,
             x+22*s+step,y+180*s,WHITE);
    rect(x+5*s+step,y+178*s,17*s,2*s,BLACK);

    triangle(x+39*s-step,y+176*s,
             x+48*s-step,y+176*s,
             x+51*s-step,y+180*s,WHITE);
    rect(x+38*s-step,y+178*s,15*s,2*s,BLACK);
}

/* ===== PERSONAGEM E ATAQUE ===== */

static void drawPlayer(Player *p)
{
    float x = p->x;
    float y = p->y;
    float s = p->scale;
    float swing = 0.0f;
    float step = 0.0f;

    if (p->anim == RUN) {
        swing = sinf(p->animTime * 12.0f) * 5.0f;
        step = sinf(p->animTime * 12.0f) * 4.0f;
    }

    if (p->anim == JUMP) {
        swing = -2.0f;
        step = -2.0f;
    }

    float drawX = x;
    if (p->anim == DASH)
        drawX -= p->direction * 3.0f;

    legs(drawX,y,s,step);
    collar(drawX,y,s);
    torso(drawX,y,s);
    arms(drawX,y,s,swing);
    face(drawX,y,s);
    hair(drawX,y,s);

    if (p->anim == HIT && p->attackTimer > 0) {
        float sign = (float)p->direction;
        float ax = drawX + (sign > 0 ? 44*s : -15*s);

        line(ax,y+94*s,ax+14*s*sign,y+78*s,
             4*s,WHITE);
        line(ax+14*s*sign,y+78*s,
             ax+29*s*sign,y+89*s,3*s,LIGHT);
        line(ax+4*s*sign,y+101*s,
             ax+24*s*sign,y+98*s,2*s,WHITE);
    }
}

/* ===== CENARIO DE TESTE ===== */

static void drawBackground(void)
{
    rect(0,0,W,H,0xFFF0F0E8);
    rect(0,0,12,H,GRAY);
    rect(468,0,12,H,GRAY);

    rect(0,FLOOR_Y,W,H-FLOOR_Y,DARK);
    rect(0,FLOOR_Y,W,4,LIGHT);

    rect(80,180,105,10,DARK);
    rect(80,180,105,3,LIGHT);
    rect(225,145,95,10,DARK);
    rect(225,145,95,3,LIGHT);
    rect(350,185,75,10,DARK);
    rect(350,185,75,3,LIGHT);
}

/* ===== CONTROLES E FISICA ===== */

static void updatePlayer(void)
{
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad,1);

    player.animTime += 1.0f/60.0f;

    if (player.attackTimer > 0)
        player.attackTimer--;

    if (player.dashTimer > 0)
        player.dashTimer--;

    int left  = (pad.Buttons & PSP_CTRL_LEFT) != 0;
    int right = (pad.Buttons & PSP_CTRL_RIGHT) != 0;

    player.vx = 0;

    if (player.dashTimer > 0) {
        player.vx = player.direction * 7.5f;
        player.anim = DASH;
    } else {
        if (left) {
            player.vx = -2.8f;
            player.direction = -1;
        }

        if (right) {
            player.vx = 2.8f;
            player.direction = 1;
        }
    }

    if ((pad.Buttons & PSP_CTRL_CROSS) && player.grounded) {
        player.vy = -8.0f;
        player.grounded = 0;
    }

    if ((pad.Buttons & PSP_CTRL_SQUARE) &&
        player.attackTimer == 0) {
        player.attackTimer = 12;
    }

    if (player.dashTimer == 0) {
        player.vy += 0.38f;
        if (player.vy > 8.0f)
            player.vy = 8.0f;
    }

    player.x += player.vx;
    player.y += player.vy;

    if (player.x < 15) player.x = 15;
    if (player.x > 410) player.x = 410;

    if (player.y + 180 >= FLOOR_Y) {
        player.y = FLOOR_Y - 180;
        player.vy = 0;
        player.grounded = 1;
    } else {
        player.grounded = 0;
    }

    if (player.dashTimer > 0)
        player.anim = DASH;
    else if (player.attackTimer > 0)
        player.anim = HIT;
    else if (!player.grounded)
        player.anim = JUMP;
    else if (player.vx != 0)
        player.anim = RUN;
    else
        player.anim = IDLE;
}

/* ===== INICIALIZACAO GU ===== */

static void initGU(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT,displayList);

    sceGuDrawBuffer(GU_PSM_8888,(void *)0,PITCH);
    sceGuDispBuffer(W,H,(void *)0x88000,PITCH);
    sceGuDepthBuffer((void *)0x110000,PITCH);

    sceGuOffset(2048-W/2,2048-H/2);
    sceGuViewport(2048,2048,W,H);
    sceGuScissor(0,0,W,H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_CULL_FACE);

    sceGuClearColor(0xFFF0F0E8);
    sceGuClearDepth(65535);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* ===== MAIN ===== */

int main(void)
{
    memset(&player,0,sizeof(player));

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
        sceCtrlReadBufferPositive(&pad,1);

        if (pad.Buttons & PSP_CTRL_START)
            running = 0;

        updatePlayer();

        sceGuStart(GU_DIRECT,displayList);
        sceGuClearColor(0xFFF0F0E8);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        drawBackground();
        drawPlayer(&player);

        for (int i=0;i<player.hp;i++)
            rect(20+i*13,20,10,7,BLACK);

        sceGuFinish();
        sceGuSync(0,0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
