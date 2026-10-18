
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define BUF_WIDTH 512
#define WORLD_W 2400.0f
#define PW 18.0f
#define PH 30.0f
#define ENEMY_COUNT 12
#define SHOT_COUNT 6

#define WHITE 0xFFFFFFFF
#define BLACK 0xFF000000
/* Cores no formato ABGR usado pela GU do PSP. */
#define BLUE  0xFFFF0000

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, w, h;
} Rect;

typedef struct {
    float x, y, vx, vy;
    int active, target;
} Shot;

typedef struct {
    float x, y, baseY;
    float w, h, phase;
    int active, hp;
} Enemy;

static unsigned int __attribute__((aligned(16)))
displayList[262144];

static int state = 0;
static int magicUnlocked = 0;
static int grounded = 0;
static int jumps = 0;
static int cooldown = 0;
static int room = 0;
static int frame = 0;

static float px = 35.0f;
static float py = 190.0f;
static float pvy = 0.0f;
static float cameraX = 0.0f;

static unsigned int previousButtons = 0;

static Shot shots[SHOT_COUNT];

static Enemy enemies[ENEMY_COUNT] = {
    {160,170,170,18,18,0.0f,1,1},
    {300,130,130,18,18,1.2f,1,1},
    {430,175,175,18,18,2.1f,1,1},
    {610,150,150,18,18,0.7f,1,1},
    {760,110,110,18,18,1.8f,1,1},
    {900,170,170,18,18,2.8f,1,1},
    {1080,140,140,18,18,0.3f,1,1},
    {1230,100,100,18,18,1.4f,1,1},
    {1400,160,160,18,18,2.5f,1,1},
    {1630,135,135,18,18,0.9f,1,1},
    {1870,100,100,18,18,1.7f,1,1},
    {2200,145,145,18,18,2.7f,1,1}
};

static void initGraphics(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
    sceGuDispBuffer(SW, SH, (void *)0x88000, BUF_WIDTH);
    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);
    sceGuScissor(0, 0, SW, SH);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_FLAT);

    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void beginFrame(void)
{
    sceGuStart(GU_DIRECT, displayList);
    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

static void endFrame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void rect(float x, float y, float w, float h,
                 unsigned int color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, 0, v
    );
}

static void wr(float x, float y, float w, float h,
               unsigned int color)
{
    rect(x - cameraX, y, w, h, color);
}

/* Letras simples desenhadas sem texturas. */
static void letter(int x, int y, char c, unsigned int color)
{
    static const unsigned char glyph[26][5] = {
        {126,17,17,17,126},{127,73,73,73,54},
        {62,65,65,65,34},{127,65,65,34,28},
        {127,73,73,73,65},{127,9,9,9,1},
        {62,65,73,73,122},{127,8,8,8,127},
        {0,65,127,65,0},{32,64,65,63,1},
        {127,8,20,34,65},{127,64,64,64,64},
        {127,2,12,2,127},{127,4,8,16,127},
        {62,65,65,65,62},{127,9,9,9,6},
        {62,65,81,33,94},{127,9,25,41,70},
        {70,73,73,73,49},{1,1,127,1,1},
        {63,64,64,64,63},{31,32,64,32,31},
        {63,64,56,64,63},{99,20,8,20,99},
        {7,8,112,8,7},{97,81,73,69,67}
    };

    if (c < 'A' || c > 'Z')
        return;

    const unsigned char *g = glyph[c - 'A'];

    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (g[col] & (1 << row))
                rect(x + col * 2, y + row * 2, 2, 2, color);
        }
    }
}

static void text(int x, int y, const char *s,
                 unsigned int color)
{
    while (*s) {
        if (*s == ' ') {
            x += 12;
        } else {
            letter(x, y, *s, color);
            x += 12;
        }
        s++;
    }
}

static void platform(float x, float y, float w)
{
    wr(x, y, w, 4, WHITE);
    wr(x + 3, y + 4, w - 6, 2, BLACK);

    for (float i = 9; i < w - 5; i += 21) {
        wr(x + i, y + 1, 2, 3, BLACK);
    }
}

static void stalactite(float x, float width, float length)
{
    wr(x, 0, width, 5, WHITE);
    wr(x + width * 0.25f, 5, width * 0.5f,
       length * 0.55f, WHITE);
    wr(x + width * 0.4f, length * 0.55f,
       width * 0.2f, length * 0.45f, WHITE);
}

static void crack(float x, float y)
{
    wr(x, y, 14, 2, WHITE);
    wr(x + 5, y + 2, 2, 7, WHITE);
    wr(x + 5, y + 9, 9, 2, WHITE);
    wr(x + 12, y + 11, 2, 5, WHITE);
}

static void crystalDecoration(float x, float y)
{
    wr(x + 5, y, 5, 5, WHITE);
    wr(x + 2, y + 5, 11, 6, WHITE);
    wr(x, y + 11, 15, 5, WHITE);
    wr(x + 4, y + 16, 7, 4, WHITE);
    wr(x + 6, y + 5, 2, 7, BLACK);
}

static void caveRoom(int r)
{
    float start = r * 800.0f;

    /* Teto quebrado, sem linhas perfeitamente uniformes. */
    stalactite(start + 15, 54, 30);
    stalactite(start + 105, 35, 19);
    stalactite(start + 210, 60, 43);
    stalactite(start + 335, 42, 24);
    stalactite(start + 460, 65, 36);
    stalactite(start + 610, 45, 27);
    stalactite(start + 730, 52, 33);

    crack(start + 90, 76);
    crack(start + 280, 104);
    crack(start + 510, 70);
    crack(start + 675, 95);

    crystalDecoration(start + 170, 204);
    crystalDecoration(start + 560, 190);

    /* Piso principal dividido por fendas e abismos. */
    platform(start, 238, 120);
    platform(start + 145, 238, 105);
    platform(start + 275, 238, 165);
    platform(start + 470, 238, 115);
    platform(start + 620, 238, 180);

    /* Caminhos verticais e plataformas naturais. */
    platform(start + 65, 197, 47);
    platform(start + 150, 166, 43);
    platform(start + 245, 192, 40);
    platform(start + 330, 151, 52);
    platform(start + 430, 185, 42);
    platform(start + 535, 160, 46);
    platform(start + 650, 129, 55);
    platform(start + 740, 177, 40);

    /* Espinhos monocromáticos. */
    wr(start + 126, 232, 5, 6, WHITE);
    wr(start + 132, 226, 5, 12, WHITE);
    wr(start + 138, 232, 5, 6, WHITE);

    wr(start + 442, 232, 5, 6, WHITE);
    wr(start + 448, 226, 5, 12, WHITE);
    wr(start + 454, 232, 5, 6, WHITE);

    /* Passagem estreita na parte superior. */
    wr(start + 390, 48, 50, 4, WHITE);
    wr(start + 390, 52, 4, 24, WHITE);
    wr(start + 436, 52, 4, 24, WHITE);
}

static void drawCave(void)
{
    caveRoom(0);
    caveRoom(1);
    caveRoom(2);

    /* Aberturas entre as áreas. */
    wr(790, 200, 20, 38, BLACK);
    wr(1590, 200, 20, 38, BLACK);

    /* Paredes laterais com aberturas inferiores. */
    wr(0, 0, 5, 238, WHITE);
    wr(2395, 0, 5, 238, WHITE);
}

static void drawPlayer(void)
{
    float x = px - cameraX;
    float y = py;

    rect(x + 4, y, 10, 9, WHITE);
    rect(x + 2, y + 9, 14, 12, WHITE);
    rect(x, y + 11, 3, 10, WHITE);
    rect(x + 15, y + 11, 3, 10, WHITE);
    rect(x + 3, y + 21, 5, 9, WHITE);
    rect(x + 11, y + 21, 5, 9, WHITE);

    rect(x + 6, y + 3, 2, 2, BLACK);
    rect(x + 11, y + 3, 2, 2, BLACK);
}

static void drawEnemies(void)
{
    for (int i = 0; i < ENEMY_COUNT; i++) {
        Enemy *e = &enemies[i];

        if (!e->active)
            continue;

        float x = e->x - cameraX;
        float y = e->y;

        /* Cristal flutuante preto e branco. */
        rect(x + 7, y, 5, 4, WHITE);
        rect(x + 3, y + 4, 13, 5, WHITE);
        rect(x, y + 9, 18, 5, WHITE);
        rect(x + 3, y + 14, 12, 4, WHITE);

        rect(x + 6, y + 7, 3, 3, BLACK);
        rect(x + 11, y + 7, 3, 3, BLACK);

        /* Contorno e brilho alternados em pixels. */
        rect(x + 1, y + 9, 2, 4, BLACK);
        rect(x + 15, y + 9, 2, 4, BLACK);
    }
}

static int nearestEnemy(float x, float y)
{
    int result = -1;
    float best = 9999999.0f;

    for (int i = 0; i < ENEMY_COUNT; i++) {
        if (!enemies[i].active)
            continue;

        float dx = enemies[i].x + enemies[i].w * 0.5f - x;
        float dy = enemies[i].y + enemies[i].h * 0.5f - y;
        float d = dx * dx + dy * dy;

        if (d < best) {
            best = d;
            result = i;
        }
    }

    return result;
}

static void fireMagic(void)
{
    int target = nearestEnemy(px + PW / 2, py + PH / 2);

    if (target < 0)
        return;

    for (int i = 0; i < SHOT_COUNT; i++) {
        if (shots[i].active)
            continue;

        shots[i].x = px + PW / 2;
        shots[i].y = py + PH / 2;
        shots[i].vx = 3.5f;
        shots[i].vy = 0.0f;
        shots[i].target = target;
        shots[i].active = 1;

        cooldown = 15;
        return;
    }
}

static void updateShots(void)
{
    for (int i = 0; i < SHOT_COUNT; i++) {
        Shot *s = &shots[i];

        if (!s->active)
            continue;

        if (s->target < 0 || s->target >= ENEMY_COUNT ||
            !enemies[s->target].active) {
            s->target = nearestEnemy(s->x, s->y);

            if (s->target < 0) {
                s->active = 0;
                continue;
            }
        }

        Enemy *e = &enemies[s->target];

        float dx = e->x + e->w / 2 - s->x;
        float dy = e->y + e->h / 2 - s->y;
        float d = sqrtf(dx * dx + dy * dy);

        if (d < 12.0f) {
            e->hp--;

            if (e->hp <= 0)
                e->active = 0;

            s->active = 0;
            continue;
        }

        if (d > 0.01f) {
            float vx = dx / d * 4.8f;
            float vy = dy / d * 4.8f;

            s->vx += (vx - s->vx) * 0.18f;
            s->vy += (vy - s->vy) * 0.18f;
        }

        s->x += s->vx;
        s->y += s->vy;

        if (s->x < 0 || s->x > WORLD_W ||
            s->y < -10 || s->y > SH + 20) {
            s->active = 0;
        }
    }
}

static void drawShots(void)
{
    for (int i = 0; i < SHOT_COUNT; i++) {
        Shot *s = &shots[i];

        if (!s->active)
            continue;

        float x = s->x - cameraX;
        float y = s->y;

        /* Rastro e projétil são as únicas partes azuis. */
        rect(x - s->vx * 3, y - s->vy * 3, 8, 4, BLUE);
        rect(x - 3, y - 3, 7, 7, BLUE);
        rect(x - 1, y - 1, 3, 3, WHITE);
    }
}

static void drawMenu(void)
{
    rect(0, 0, SW, SH, BLACK);

    rect(65, 45, 350, 3, WHITE);
    rect(65, 48, 3, 52, WHITE);
    rect(412, 48, 3, 52, WHITE);
    rect(65, 100, 350, 3, WHITE);

    text(174, 63, "RANDOM", WHITE);
    text(132, 124, "PRESS START", WHITE);
    text(126, 157, "EXPLORE CAVE", WHITE);

    rect(120, 215, 240, 3, WHITE);
}

static void drawIntro(void)
{
    rect(0, 0, SW, SH, BLACK);

    text(174, 36, "RANDOM", WHITE);
    text(102, 80, "EXPLORE CAVE", WHITE);
    text(90, 125, "X JUMP", WHITE);
    text(90, 151, "O MAGIC", WHITE);
    text(90, 177, "START MENU", WHITE);
    text(120, 225, "PRESS START", WHITE);
}

static int overlap(float ax, float ay, float aw, float ah,
                   float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

static void updateEnemies(void)
{
    for (int i = 0; i < ENEMY_COUNT; i++) {
        Enemy *e = &enemies[i];

        if (!e->active)
            continue;

        e->phase += 0.045f;
        e->y = e->baseY + sinf(e->phase) * 10.0f;

        /* O inimigo se aproxima do jogador quando está perto. */
        float dx = px - e->x;

        if (fabsf(dx) < 150.0f) {
            if (dx > 4.0f)
                e->x += 0.25f;
            else if (dx < -4.0f)
                e->x -= 0.25f;
        }

        if (overlap(px, py, PW, PH, e->x, e->y, e->w, e->h)) {
            px = 35.0f;
            py = 190.0f;
            pvy = 0.0f;
            grounded = 0;
            jumps = 0;
        }
    }
}

static void updateGame(SceCtrlData pad, unsigned int pressed)
{
    float vx = 0.0f;

    if (pad.Buttons & PSP_CTRL_LEFT)
        vx = -2.4f;

    if (pad.Buttons & PSP_CTRL_RIGHT)
        vx = 2.4f;

    px += vx;

    if (px < 0)
        px = 0;

    if (px > WORLD_W - PW)
        px = WORLD_W - PW;

    if ((pressed & PSP_CTRL_CROSS) && jumps < 2) {
        pvy = -7.2f;
        grounded = 0;
        jumps++;
    }

    float oldBottom = py + PH;

    pvy += 0.30f;

    if (pvy > 7.0f)
        pvy = 7.0f;

    py += pvy;
    grounded = 0;

    /* Pisos das três áreas. */
    Rect floors[] = {
        {0,238,120,34},
        {145,238,105,34},
        {275,238,165,34},
        {470,238,115,34},
        {620,238,180,34},
        {800,238,120,34},
        {945,238,105,34},
        {1075,238,165,34},
        {1270,238,115,34},
        {1420,238,180,34},
        {1600,238,120,34},
        {1745,238,105,34},
        {1875,238,165,34},
        {2070,238,115,34},
        {2220,238,180,34}
    };

    for (int i = 0; i < (int)(sizeof(floors) / sizeof(floors[0])); i++) {
        Rect f = floors[i];

        if (pvy >= 0 &&
            oldBottom <= f.y + 2 &&
            py + PH >= f.y &&
            px + PW > f.x &&
            px < f.x + f.w) {
            py = f.y - PH;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }
    }

    /* Plataformas naturais superiores. */
    Rect platforms[] = {
        {65,197,47,6},{150,166,43,6},
        {245,192,40,6},{330,151,52,6},
        {430,185,42,6},{535,160,46,6},
        {650,129,55,6},{740,177,40,6},
        {865,197,47,6},{950,166,43,6},
        {1045,192,40,6},{1130,151,52,6},
        {1230,185,42,6},{1335,160,46,6},
        {1450,129,55,6},{1540,177,40,6},
        {1665,197,47,6},{1750,166,43,6},
        {1845,192,40,6},{1930,151,52,6},
        {2030,185,42,6},{2135,160,46,6},
        {2250,129,55,6}
    };

    for (int i = 0; i < (int)(sizeof(platforms) / sizeof(platforms[0])); i++) {
        Rect p = platforms[i];

        if (pvy >= 0 &&
            oldBottom <= p.y + 2 &&
            py + PH >= p.y &&
            px + PW > p.x &&
            px < p.x + p.w) {
            py = p.y - PH;
            pvy = 0;
            grounded = 1;
            jumps = 0;
        }
    }

    /* A magia é encontrada no primeiro trecho da caverna. */
    if (!magicUnlocked &&
        overlap(px, py, PW, PH, 135, 155, 35, 35)) {
        magicUnlocked = 1;
    }

    if (cooldown > 0)
        cooldown--;

    if (magicUnlocked &&
        (pressed & PSP_CTRL_CIRCLE) &&
        cooldown == 0) {
        fireMagic();
    }

    updateEnemies();
    updateShots();

    if (py > SH + 60) {
        px = 35.0f;
        py = 190.0f;
        pvy = 0.0f;
        grounded = 0;
        jumps = 0;
    }

    cameraX = px - 210.0f;

    if (cameraX < 0)
        cameraX = 0;

    if (cameraX > WORLD_W - SW)
        cameraX = WORLD_W - SW;

    if (px < 800)
        room = 0;
    else if (px < 1600)
        room = 1;
    else
        room = 2;

    if (px > WORLD_W - 45) {
        px = 35.0f;
        py = 190.0f;
        pvy = 0.0f;
        grounded = 0;
        jumps = 0;
        magicUnlocked = 0;

        for (int i = 0; i < ENEMY_COUNT; i++) {
            enemies[i].active = 1;
            enemies[i].hp = 1;
        }

        for (int i = 0; i < SHOT_COUNT; i++)
            shots[i].active = 0;
    }
}

static void drawGame(void)
{
    beginFrame();

    drawCave();
    drawEnemies();
    drawShots();
    drawPlayer();

    text(8, 8, magicUnlocked ? "MAGIC READY" : "FIND MAGIC", WHITE);

    if (room == 0)
        text(350, 8, "CAVE", WHITE);
    else if (room == 1)
        text(350, 8, "ZONE", WHITE);
    else
        text(350, 8, "EXIT", WHITE);

    endFrame();
}

int main(void)
{
    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    memset(shots, 0, sizeof(shots));

    initGraphics();

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int pressed = pad.Buttons & ~previousButtons;
        previousButtons = pad.Buttons;

        if (state == 0) {
            beginFrame();
            drawMenu();
            endFrame();

            if (pressed & (PSP_CTRL_START |
                           PSP_CTRL_CROSS |
                           PSP_CTRL_CIRCLE)) {
                state = 1;
            }
        } else if (state == 1) {
            beginFrame();
            drawIntro();
            endFrame();

            if (pressed & (PSP_CTRL_START |
                           PSP_CTRL_CROSS |
                           PSP_CTRL_CIRCLE)) {
                state = 2;
            }
        } else {
            if (pressed & PSP_CTRL_START) {
                state = 0;
            } else {
                updateGame(pad, pressed);
                drawGame();
            }
        }

        frame++;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
