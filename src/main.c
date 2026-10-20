
/*
 * RANDOM
 * Prototipo jogavel para PSP / PSPSDK
 * Personagem cartoon desenhado inteiramente em C.
 * Animacoes: correr, pular e atacar.
 * Sem imagens externas.
 */

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
#define PLAYER_H 152.0f

#define BLACK 0xFF080808
#define DARK  0xFF242424
#define GRAY  0xFF626262
#define LIGHT 0xFFAAAAAA
#define WHITE 0xFFF5F5F0
#define BG    0xFFE5E5DD

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y;
    float vx, vy;
    float scale, animTime;
    int direction;
    int anim;
    int grounded;
    int attackTimer;
    int hp;
    unsigned int previousButtons;
} Player;

enum {
    IDLE,
    RUN,
    HIT,
    JUMP
};

static unsigned int __attribute__((aligned(16)))
    displayList[262144];

static Player player;
static int running = 1;

/* ==================================================
   FORMAS GEOMETRICAS
   sceGuGetMemory reserva vertices dentro da lista GU.
   ================================================== */

static void rect(float x, float y, float w, float h,
                 unsigned int color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 2);
    if (!v) return;

    v[0] = (Vertex){color, x, y, 0.0f};
    v[1] = (Vertex){color, x + w, y + h, 0.0f};

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

    v[0] = (Vertex){color, x1, y1, 0.0f};
    v[1] = (Vertex){color, x2, y2, 0.0f};
    v[2] = (Vertex){color, x3, y3, 0.0f};

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

    v[0] = (Vertex){color, x1 + px, y1 + py, 0.0f};
    v[1] = (Vertex){color, x2 + px, y2 + py, 0.0f};
    v[2] = (Vertex){color, x2 - px, y2 - py, 0.0f};
    v[3] = (Vertex){color, x1 - px, y1 - py, 0.0f};

    sceGuDrawArray(
        GU_TRIANGLE_FAN,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        4, NULL, v
    );
}

/* ==================================================
   CABECA E CABELO CARTOON
   ================================================== */

static void hair(float x, float y, float s)
{
    triangle(x+8*s,  y+37*s, x+5*s,  y+13*s,
             x+19*s, y+20*s, BLACK);

    triangle(x+12*s, y+33*s, x+25*s, y+2*s,
             x+32*s, y+33*s, BLACK);

    triangle(x+26*s, y+32*s, x+43*s, y+10*s,
             x+47*s, y+35*s, DARK);

    triangle(x+8*s,  y+32*s, x+25*s, y+20*s,
             x+43*s, y+36*s, GRAY);

    triangle(x+10*s, y+31*s, x+18*s, y+45*s,
             x+8*s,  y+43*s, BLACK);

    rect(x+10*s, y+28*s, 36*s, 12*s, BLACK);
}

static void face(float x, float y, float s)
{
    /* Rosto liso, sem olhos ou boca. */
    rect(x+12*s, y+32*s, 29*s, 27*s, WHITE);

    triangle(x+12*s, y+39*s, x+19*s, y+57*s,
             x+12*s, y+54*s, LIGHT);

    triangle(x+41*s, y+37*s, x+41*s, y+54*s,
             x+35*s, y+59*s, LIGHT);

    rect(x+17*s, y+28*s, 19*s, 5*s, WHITE);
}

static void collar(float x, float y, float s)
{
    rect(x+22*s, y+57*s, 11*s, 10*s, DARK);

    triangle(x+11*s, y+63*s, x+43*s, y+63*s,
             x+38*s, y+79*s, BLACK);

    triangle(x+11*s, y+63*s, x+38*s, y+79*s,
             x+15*s, y+78*s, GRAY);

    triangle(x+17*s, y+64*s, x+35*s, y+64*s,
             x+26*s, y+73*s, WHITE);
}

/* ==================================================
   TRONCO
   ================================================== */

static void torso(float x, float y, float s)
{
    triangle(x+14*s, y+72*s, x+39*s, y+72*s,
             x+43*s, y+104*s, BLACK);

    triangle(x+14*s, y+72*s, x+43*s, y+104*s,
             x+11*s, y+104*s, DARK);

    /* Faixa diagonal da roupa. */
    triangle(x+14*s, y+77*s, x+23*s, y+82*s,
             x+39*s, y+73*s, GRAY);

    triangle(x+14*s, y+77*s, x+18*s, y+92*s,
             x+23*s, y+82*s, LIGHT);

    rect(x+12*s, y+101*s, 30*s, 7*s, BLACK);
}

/* ==================================================
   BRACOS CARTOON
   ================================================== */

static void arms(float x, float y, float s, float swing)
{
    /* Manga esquerda. */
    triangle(x+14*s, y+75*s, x+23*s, y+78*s,
             x+19*s, y+96*s, BLACK);

    line(x+16*s, y+94*s,
         x+8*s, y+108*s+swing, 6*s, DARK);

    line(x+8*s, y+108*s+swing,
         x+8*s, y+119*s+swing, 5*s, BLACK);

    /* Mao esquerda. */
    rect(x+3*s, y+116*s+swing, 10*s, 8*s, BLACK);
    rect(x+6*s, y+118*s+swing, 4*s, 4*s, WHITE);

    /* Manga direita. */
    triangle(x+36*s, y+74*s, x+45*s, y+79*s,
             x+47*s, y+96*s, BLACK);

    line(x+43*s, y+94*s,
         x+49*s, y+108*s-swing, 7*s, BLACK);

    line(x+49*s, y+108*s-swing,
         x+54*s, y+121*s-swing, 5*s, DARK);

    /* Mao direita. */
    triangle(x+49*s, y+117*s-swing,
             x+58*s, y+117*s-swing,
             x+57*s, y+127*s-swing, BLACK);

    triangle(x+49*s, y+117*s-swing,
             x+57*s, y+127*s-swing,
             x+51*s, y+125*s-swing, WHITE);
}

/* ==================================================
   CALCA E TENIS
   ================================================== */

static void legs(float x, float y, float s, float step)
{
    /* Perna esquerda da calca. */
    triangle(x+13*s, y+104*s, x+28*s, y+104*s,
             x+25*s, y+129*s, BLACK);

    triangle(x+13*s, y+104*s, x+25*s, y+129*s,
             x+8*s, y+139*s, DARK);

    triangle(x+8*s, y+139*s, x+25*s, y+129*s,
             x+23*s, y+143*s, GRAY);

    /* Perna direita. */
    triangle(x+28*s, y+104*s, x+41*s, y+104*s,
             x+46*s, y+137*s, BLACK);

    triangle(x+28*s, y+104*s, x+46*s, y+137*s,
             x+37*s, y+143*s, GRAY);

    /* Faixa clara. */
    triangle(x+30*s, y+109*s, x+34*s, y+110*s,
             x+42*s, y+135*s, WHITE);

    /* Pernas finas animadas. */
    line(x+17*s, y+140*s,
         x+16*s+step, y+146*s, 5*s, BLACK);

    line(x+40*s, y+140*s,
         x+41*s-step, y+146*s, 5*s, BLACK);

    /* Tenis cartoon. */
    triangle(x+8*s+step, y+143*s,
             x+21*s+step, y+143*s,
             x+25*s+step, y+149*s, WHITE);

    rect(x+7*s+step, y+147*s, 19*s, 4*s, BLACK);

    triangle(x+35*s-step, y+143*s,
             x+47*s-step, y+143*s,
             x+52*s-step, y+149*s, WHITE);

    rect(x+35*s-step, y+147*s, 18*s, 4*s, BLACK);
}

/* ==================================================
   PERSONAGEM COMPLETO E ATAQUE
   ================================================== */

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
        swing = -3.0f;
        step = -2.0f;
    }

    legs(x, y, s, step);
    torso(x, y, s);
    collar(x, y, s);
    arms(x, y, s, swing);
    face(x, y, s);
    hair(x, y, s);

    /* Meia-lua do ataque. */
    if (p->anim == HIT && p->attackTimer > 0) {
        float sign = (float)p->direction;
        float ax = x + (sign > 0 ? 45*s : -12*s);

        line(ax, y+80*s,
             ax+12*s*sign, y+68*s, 4*s, WHITE);

        line(ax+12*s*sign, y+68*s,
             ax+27*s*sign, y+77*s, 3*s, LIGHT);

        line(ax+4*s*sign, y+88*s,
             ax+22*s*sign, y+87*s, 2*s, WHITE);
    }
}

/* ==================================================
   CENARIO DE TESTE
   ================================================== */

static void drawBackground(void)
{
    rect(0, 0, W, H, BG);

    /* Paredes. */
    rect(0, 0, 12, H, GRAY);
    rect(W-12, 0, 12, H, GRAY);

    /* Chao. */
    rect(0, FLOOR_Y, W, H-FLOOR_Y, DARK);
    rect(0, FLOOR_Y, W, 4, LIGHT);

    /* Plataformas. */
    rect(80, 180, 105, 10, DARK);
    rect(80, 180, 105, 3, LIGHT);

    rect(225, 145, 95, 10, DARK);
    rect(225, 145, 95, 3, LIGHT);

    rect(350, 185, 75, 10, DARK);
    rect(350, 185, 75, 3, LIGHT);
}

/* ==================================================
   MOVIMENTO E CONTROLES
   ================================================== */

static void updatePlayer(void)
{
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    player.animTime += 1.0f / 60.0f;

    if (player.attackTimer > 0)
        player.attackTimer--;

    int left = (pad.Buttons & PSP_CTRL_LEFT) != 0;
    int right = (pad.Buttons & PSP_CTRL_RIGHT) != 0;

    /* Movimento horizontal. */
    player.vx = 0.0f;

    if (left) {
        player.vx = -2.8f;
        player.direction = -1;
    }

    if (right) {
        player.vx = 2.8f;
        player.direction = 1;
    }

    /* Detecta o pressionamento novo de X. */
    int crossNow = (pad.Buttons & PSP_CTRL_CROSS) != 0;
    int crossBefore =
        (player.previousButtons & PSP_CTRL_CROSS) != 0;

    if (crossNow && !crossBefore && player.grounded) {
        player.vy = -8.0f;
        player.grounded = 0;
    }

    /* Detecta o pressionamento novo de Quadrado. */
    int squareNow = (pad.Buttons & PSP_CTRL_SQUARE) != 0;
    int squareBefore =
        (player.previousButtons & PSP_CTRL_SQUARE) != 0;

    if (squareNow && !squareBefore && player.attackTimer == 0)
        player.attackTimer = 12;

    player.previousButtons = pad.Buttons;

    /* Gravidade. */
    player.vy += 0.38f;

    if (player.vy > 8.0f)
        player.vy = 8.0f;

    player.x += player.vx;
    player.y += player.vy;

    if (player.x < 15.0f) player.x = 15.0f;
    if (player.x > 410.0f) player.x = 410.0f;

    /* Colisao com o chao. */
    if (player.y + PLAYER_H >= FLOOR_Y) {
        player.y = FLOOR_Y - PLAYER_H;
        player.vy = 0.0f;
        player.grounded = 1;
    } else {
        player.grounded = 0;
    }

    /* Prioridade das animacoes. */
    if (player.attackTimer > 0)
        player.anim = HIT;
    else if (!player.grounded)
        player.anim = JUMP;
    else if (player.vx != 0.0f)
        player.anim = RUN;
    else
        player.anim = IDLE;
}

/* ==================================================
   INICIALIZACAO GRAFICA
   ================================================== */

static void initGU(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, PITCH);
    sceGuDispBuffer(W, H, (void *)0x88000, PITCH);
    sceGuDepthBuffer((void *)0x110000, PITCH);

    sceGuOffset(2048-W/2, 2048-H/2);
    sceGuViewport(2048, 2048, W, H);
    sceGuScissor(0, 0, W, H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_CULL_FACE);

    sceGuClearColor(BG);
    sceGuClearDepth(65535);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* ==================================================
   MAIN
   ================================================== */

int main(void)
{
    memset(&player, 0, sizeof(player));

    player.x = 45.0f;
    player.y = FLOOR_Y - PLAYER_H;
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

        if (pad.Buttons & PSP_CTRL_START)
            running = 0;

        updatePlayer();

        sceGuStart(GU_DIRECT, displayList);

        sceGuClearColor(BG);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        drawBackground();
        drawPlayer(&player);

        /* Indicador de vida. */
        for (int i = 0; i < player.hp; i++)
            rect(20+i*13, 20, 10, 7, BLACK);

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
