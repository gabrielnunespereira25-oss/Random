
/*
 * RANDOM - PSP
 * Caverna (Zona 1) + Floresta (Zona 2)
 * Renderizacao por PSP GU, sem PNG ou GIF.
 *
 * ATENCAO:
 * Esta e uma versao consolidada baseada no codigo enviado.
 * Precisa ser compilada com o PSPSDK e testada no PSP.
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

#define SW 480
#define SH 272
#define STRIDE 512
#define WW 1600
#define GROUND 250
#define PW 18
#define PH 30
#define CAVE 0
#define FOREST 1
#define N_MOBS 4
#define N_SHOTS 8

typedef unsigned int Color;

typedef struct {
    Color c;
    float x, y, z;
} Vtx;

typedef struct {
    float x, y, w, h;
} Box;

typedef struct {
    float x, y, min, max, speed;
    int alive, hp, cd;
} Mob;

typedef struct {
    int on;
    float x, y, vx;
} Shot;

static unsigned int __attribute__((aligned(16)))
    list[262144];

#define BLACK  0xFF080808
#define DARK   0xFF202020
#define STONE  0xFF454545
#define GRAY   0xFF777777
#define LIGHT  0xFFBBBBBB
#define WHITE  0xFFFFFFFF

/* Cores da interface e dos efeitos */
#define GREEN  0xFF35B85A
#define PURPLE 0xFFB050E8
#define RED    0xFF3030E8
#define YELLOW 0xFF30E8F8
#define BROWN  0xFF493020
#define SKIN   0xFF70B8E8

static int zone = CAVE;
static int paused = 0;
static int face = 1;

static int grounded = 1;
static int jumps = 2;

static float cam = 0;
static float px = 60;
static float py = 220;
static float vy = 0;

static int hp = 5;
static int maxHP = 5;
static int magic = 0;
static int magicCD = 0;

static int xp = 0;
static int level = 1;
static int nextXP = 20;
static int points = 0;

static int magicPower = 1;
static int swordDamage = 1;
static int speedLevel = 1;
static int defense = 0;
static int selected = 0;

static int cpZone = CAVE;
static int cpReached[3] = {0, 0, 0};
static float cpX = 60;
static float cpY = 220;

static Mob mobs[N_MOBS] = {
    {300,220,250,380,.8f,1,1,0},
    {610,220,550,680,1.0f,1,1,0},
    {960,220,900,1040,.9f,1,1,0},
    {1190,220,1150,1250,1.1f,1,1,0}
};

static struct {
    float x, y, vy;
    int grounded, timer, active, hp, cd;
} boss = {1350,218,0,1,50,1,12,0};

static Shot shots[N_SHOTS];

/* =====================================================
   INPUT
   ===================================================== */

static int pressed(unsigned int now,
                   unsigned int old,
                   unsigned int button)
{
    return (now & button) && !(old & button);
}

/* =====================================================
   RENDERIZACAO BASICA
   ===================================================== */

/*
 * Vtx usa cor + coordenadas 2D.
 * Os retangulos recebem coordenadas de tela.
 * A camera e aplicada somente por wr().
 */
static void rect(float x, float y,
                 float w, float h, Color color)
{
    Vtx *v;

    if (w <= 0 || h <= 0)
        return;

    if (x >= SW || y >= SH ||
        x + w <= 0 || y + h <= 0)
        return;

    v = (Vtx *)sceGuGetMemory(2 * sizeof(Vtx));

    v[0].c = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].c = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v
    );
}

/* Coordenadas do mundo -> coordenadas da tela */
static void wr(float x, float y,
               float w, float h, Color color)
{
    rect(x - cam, y, w, h, color);
}

static int hit(Box a, Box b)
{
    return a.x < b.x + b.w &&
           a.x + a.w > b.x &&
           a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

static Box pbox(void)
{
    return (Box){px, py, PW, PH};
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* =====================================================
   XP E HABILIDADES
   ===================================================== */

static void addXP(int amount)
{
    xp += amount;

    while (xp >= nextXP) {
        xp -= nextXP;
        level++;
        points++;
        nextXP += 10;

        if (hp < maxHP)
            hp++;
    }
}

static void spend(void)
{
    if (points <= 0)
        return;

    switch (selected) {
        case 0:
            maxHP++;
            hp++;
            break;
        case 1:
            magicPower++;
            break;
        case 2:
            swordDamage++;
            break;
        case 3:
            speedLevel++;
            break;
        case 4:
            defense++;
            break;
    }

    points--;
}

static int damage(void)
{
    int d = 2 - defense;
    return d < 1 ? 1 : d;
}

/* =====================================================
   CENARIO: CAVERNA
   ===================================================== */

static void caveRock(float x, float y, float s)
{
    wr(x, y + 8, s, s / 2, DARK);
    wr(x + 4, y + 4, s - 8, s / 2, STONE);
    wr(x + 8, y, s - 16, 5, GRAY);
    wr(x + 9, y + 5, s / 3, 3, LIGHT);
}

static void caveSpike(float x, float y, float s)
{
    wr(x, y, s, 5, DARK);
    wr(x + 3, y + 5, s - 6, 5, STONE);
    wr(x + 6, y + 10, s - 12, 5, GRAY);
    wr(x + 10, y + 15, s - 20, 5, LIGHT);
    wr(x + s / 2 - 2, y + 20, 4, 5, WHITE);
}

static void crystal(float x, float y)
{
    wr(x + 6, y, 6, 5, LIGHT);
    wr(x + 3, y + 5, 12, 5, WHITE);
    wr(x, y + 10, 18, 8, LIGHT);
    wr(x + 3, y + 18, 12, 5, GRAY);
    wr(x + 6, y + 23, 6, 5, STONE);
}

static void drawCave(void)
{
    int x;

    /* Fundo fixo na tela */
    rect(0, 0, SW, SH, BLACK);

    /* Teto */
    wr(0, 0, WW, 24, STONE);

    for (x = 40; x < WW; x += 130) {
        caveSpike((float)x, 24, 14);
    }

    /* Parede distante */
    wr(0, 55, WW, 5, DARK);
    wr(0, 105, WW, 3, DARK);

    /* Chao */
    wr(0, GROUND, WW, 22, STONE);
    wr(0, GROUND + 22, WW, 20, DARK);

    /* Plataformas */
    wr(190,190,100,10,STONE);
    wr(430,160,110,10,STONE);
    wr(650,190,100,10,STONE);
    wr(680,135,70,10,GRAY);
    wr(850,180,100,10,STONE);
    wr(1080,155,100,10,STONE);

    /* Rochas */
    caveRock(130, 220, 30);
    caveRock(370, 220, 34);
    caveRock(790, 220, 36);
    caveRock(1010, 220, 32);

    /* Cristal da magia */
    if (!magic)
        crystal(713, 100);

    /* Portas desenhadas */
    wr(690,65,5,40,LIGHT);
    wr(730,65,5,40,LIGHT);
    wr(695,65,35,4,WHITE);

    wr(1470,185,5,65,LIGHT);
    wr(1495,185,5,65,LIGHT);
    wr(1475,185,20,4,WHITE);

    /* Arena do miniboss */
    wr(1300,235,150,15,GRAY);
}

/* =====================================================
   CENARIO: FLORESTA
   ===================================================== */

static void tree(float x, float y)
{
    wr(x + 17, y + 35, 14, 85, DARK);
    wr(x + 21, y + 40, 4, 70, GRAY);

    wr(x + 5, y + 25, 38, 15, STONE);
    wr(x, y + 10, 48, 17, DARK);
    wr(x + 9, y, 30, 15, GRAY);
    wr(x + 14, y + 5, 12, 4, LIGHT);

    wr(x + 28, y + 55, 32, 5, STONE);
}

static void drawForest(void)
{
    int x;

    /* Fundo monocromatico */
    rect(0, 0, SW, SH, BLACK);

    /* Nevoeiro */
    wr(0, 95, WW, 55, DARK);
    wr(0, 145, WW, 4, STONE);

    /* Silhuetas de arvores */
    for (x = 40; x < WW; x += 170)
        tree((float)x, 80);

    /* Chao */
    wr(0, GROUND, WW, 22, STONE);
    wr(0, GROUND + 22, WW, 20, DARK);

    /* Plataformas */
    wr(220,190,100,10,STONE);
    wr(520,160,100,10,GRAY);
    wr(800,190,120,10,STONE);
    wr(1080,150,100,10,GRAY);

    caveRock(400, 220, 32);
    caveRock(940, 220, 36);

    /* Passagens de ida e volta */
    wr(100,185,5,65,LIGHT);
    wr(125,185,5,65,LIGHT);
    wr(105,185,20,4,WHITE);

    wr(1240,85,5,50,LIGHT);
    wr(1280,85,5,50,LIGHT);
    wr(1245,85,35,4,WHITE);
}

/* =====================================================
   COLISOES DO CENARIO
   ===================================================== */

/*
 * Plataformas de cada zona.
 * O chao e tratado separadamente.
 */
static int platformAt(int z, float x, float y, float w)
{
    static const Box cave[] = {
        {190,190,100,10},
        {430,160,110,10},
        {650,190,100,10},
        {680,135,70,10},
        {850,180,100,10},
        {1080,155,100,10}
    };

    static const Box forest[] = {
        {220,190,100,10},
        {520,160,100,10},
        {800,190,120,10},
        {1080,150,100,10}
    };

    const Box *p;
    int n, i;

    if (z == CAVE) {
        p = cave;
        n = sizeof(cave) / sizeof(cave[0]);
    } else {
        p = forest;
        n = sizeof(forest) / sizeof(forest[0]);
    }

    for (i = 0; i < n; i++) {
        if (x + w > p[i].x &&
            x < p[i].x + p[i].w &&
            y <= p[i].y + 12 &&
            y >= p[i].y - 2)
            return (int)p[i].y;
    }

    return -1;
}

/* =====================================================
   PERSONAGEM
   ===================================================== */

static void drawPlayer(void)
{
    float x = px - cam;

    rect(x + 4, py + 2, 10, 12, SKIN);
    rect(x + 2, py + 13, 14, 13, LIGHT);
    rect(x, py + 25, 7, 5, GRAY);
    rect(x + 11, py + 25, 7, 5, GRAY);

    rect(x + (face ? 13 : -7), py + 15,
         12, 3, WHITE);

    rect(8, 8, 80, 7, DARK);
    rect(9, 9, 78.0f * hp / (maxHP ? maxHP : 1),
         5, RED);
}

/* =====================================================
   INIMIGOS: APENAS NA CAVERNA
   ===================================================== */

static void drawMobs(void)
{
    int i;

    if (zone != CAVE)
        return;

    for (i = 0; i < N_MOBS; i++) {
        if (!mobs[i].alive)
            continue;

        wr(mobs[i].x, mobs[i].y, 20, 25, GRAY);
        wr(mobs[i].x + 4, mobs[i].y + 5, 4, 4, WHITE);
        wr(mobs[i].x + 13, mobs[i].y + 5, 4, 4, WHITE);
    }

    if (boss.active) {
        wr(boss.x, boss.y, 36, 32, LIGHT);
        wr(boss.x + 5, boss.y + 7, 7, 6, BLACK);
        wr(boss.x + 23, boss.y + 7, 7, 6, BLACK);

        rect(345, 8, 125, 7, DARK);
        rect(346, 9, 123.0f * boss.hp / 12.0f, 5, RED);
    }
}

/* =====================================================
   CHECKPOINTS E HUD
   ===================================================== */

static void drawCheckpoints(void)
{
    float xs[3] = {520, 1050, 800};
    int zs[3] = {CAVE, CAVE, FOREST};
    int i;

    for (i = 0; i < 3; i++) {
        if (zs[i] != zone)
            continue;

        wr(xs[i], 192, 4, 28, GRAY);
        wr(xs[i] + 4, 192, 15, 10,
           cpReached[i] ? GREEN : YELLOW);
    }
}

static void drawHUD(void)
{
    Color colors[5] = {RED, PURPLE, YELLOW, GREEN, WHITE};
    int i;

    rect(8, 20, 80, 5, DARK);
    rect(9, 21, 78.0f * xp / nextXP, 3, YELLOW);
    rect(8, 29, points ? 40 : 8, 4,
         points ? GREEN : GRAY);

    for (i = 0; i < 5; i++)
        rect(100 + i * 16, 8, 12, 12,
             i == selected ? colors[i] : GRAY);

    if (magic)
        rect(184, 8, 12, 12, PURPLE);

    if (paused) {
        rect(130, 75, 220, 110, DARK);
        rect(145, 90, 190, 5, WHITE);
        rect(145, 110, 150, 5, GRAY);
        rect(145, 130, 170, 5, YELLOW);
        rect(145, 150, 120, 5, GREEN);
    }
}

/* =====================================================
   DESENHO DO QUADRO
   ===================================================== */

static void draw(void)
{
    sceGuStart(GU_DIRECT, list);

    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);

    if (zone == CAVE)
        drawCave();
    else
        drawForest();

    drawCheckpoints();
    drawMobs();

    {
        int i;
        for (i = 0; i < N_SHOTS; i++) {
            if (shots[i].on)
                wr(shots[i].x, shots[i].y, 9, 7, PURPLE);
        }
    }

    drawPlayer();
    drawHUD();

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

/* =====================================================
   MAGIA E COMBATE
   ===================================================== */

static void cast(void)
{
    int i;

    if (!magic || magicCD)
        return;

    for (i = 0; i < N_SHOTS; i++) {
        if (!shots[i].on) {
            shots[i] = (Shot){
                1,
                px + (face ? PW : -8),
                py + 12,
                face ? 5 : -5
            };

            magicCD = 18;
            break;
        }
    }
}

static void updateShots(void)
{
    int i, m;

    for (i = 0; i < N_SHOTS; i++) {
        if (!shots[i].on)
            continue;

        shots[i].x += shots[i].vx;

        if (shots[i].x < 0 || shots[i].x > WW) {
            shots[i].on = 0;
            continue;
        }

        if (zone != CAVE)
            continue;

        for (m = 0; m < N_MOBS; m++) {
            if (!mobs[m].alive)
                continue;

            if (hit((Box){shots[i].x, shots[i].y, 9, 7},
                    (Box){mobs[m].x, mobs[m].y, 20, 25})) {
                mobs[m].hp -= magicPower;
                shots[i].on = 0;

                if (mobs[m].hp <= 0) {
                    mobs[m].alive = 0;
                    addXP(5);
                }

                break;
            }
        }

        if (shots[i].on && boss.active &&
            hit((Box){shots[i].x, shots[i].y, 9, 7},
                (Box){boss.x, boss.y, 36, 32})) {
            boss.hp -= magicPower;
            shots[i].on = 0;

            if (boss.hp <= 0) {
                boss.active = 0;
                addXP(50);
            }
        }
    }
}

static void updateMobs(void)
{
    int i;

    if (zone != CAVE)
        return;

    for (i = 0; i < N_MOBS; i++) {
        if (!mobs[i].alive)
            continue;

        mobs[i].x += mobs[i].speed;

        if (mobs[i].x < mobs[i].min ||
            mobs[i].x > mobs[i].max)
            mobs[i].speed = -mobs[i].speed;

        if (mobs[i].cd > 0)
            mobs[i].cd--;

        if (hit(pbox(), (Box){mobs[i].x, mobs[i].y, 20, 25}) &&
            !mobs[i].cd) {
            hp -= damage();
            mobs[i].cd = 35;
        }
    }

    if (boss.active) {
        if (boss.cd > 0)
            boss.cd--;

        if (boss.grounded &&
            fabsf(px - boss.x) < 300 &&
            boss.timer <= 0) {
            boss.vy = -6;
            boss.grounded = 0;
            boss.timer = 70;
        }

        if (boss.timer > 0)
            boss.timer--;

        boss.vy += .25f;
        boss.y += boss.vy;

        if (boss.y >= 218) {
            boss.y = 218;
            boss.vy = 0;
            boss.grounded = 1;
        }

        if (px < boss.x)
            boss.x -= .5f;
        else
            boss.x += .5f;

        boss.x = clampf(boss.x, 1300, 1410);

        if (hit(pbox(), (Box){boss.x, boss.y, 36, 32}) &&
            !boss.cd) {
            hp -= damage();
            boss.cd = 45;
        }
    }
}

static void sword(int down)
{
    static int was = 0;
    Box attack;
    int i;

    if (!down) {
        was = 0;
        return;
    }

    if (was)
        return;

    was = 1;

    if (zone != CAVE)
        return;

    attack = (Box){
        px + (face ? PW : -24),
        py + 5, 24, 20
    };

    for (i = 0; i < N_MOBS; i++) {
        if (!mobs[i].alive)
            continue;

        if (hit(attack, (Box){mobs[i].x, mobs[i].y, 20, 25})) {
            mobs[i].hp -= swordDamage;

            if (mobs[i].hp <= 0) {
                mobs[i].alive = 0;
                addXP(5);
            }
        }
    }

    if (boss.active && !boss.cd &&
        hit(attack, (Box){boss.x, boss.y, 36, 32})) {
        boss.hp -= swordDamage;
        boss.cd = 12;

        if (boss.hp <= 0) {
            boss.active = 0;
            addXP(50);
        }
    }
}

/* =====================================================
   CHECKPOINTS
   ===================================================== */

static void checkpoints(void)
{
    float xs[3] = {520, 1050, 800};
    int zs[3] = {CAVE, CAVE, FOREST};
    int i;

    for (i = 0; i < 3; i++) {
        if (zs[i] != zone)
            continue;

        if (hit(pbox(), (Box){xs[i], 190, 22, 60})) {
            cpZone = zone;
            cpX = xs[i] - 10;
            cpY = 220;
            cpReached[i] = 1;
        }
    }
}

static void respawn(void)
{
    zone = cpZone;
    px = cpX;
    py = cpY;
    vy = 0;
    grounded = 1;
    jumps = 2;
    hp = maxHP;

    cam = clampf(px - SW / 2, 0, WW - SW);
}

/* =====================================================
   PASSAGENS ENTRE ZONAS
   ===================================================== */

static void transition(void)
{
    if (zone == CAVE) {
        if (px >= 1470 && py > 170) {
            zone = FOREST;
            px = 125;
            py = 220;
            vy = 0;
        } else if (px >= 690 && px <= 735 && py < 115) {
            zone = FOREST;
            px = 1270;
            py = 115;
            vy = 0;
        }
    } else {
        if (px <= 125 && py > 170) {
            zone = CAVE;
            px = 1450;
            py = 220;
            vy = 0;
        } else if (px >= 1240 && px <= 1290 && py < 135) {
            zone = CAVE;
            px = 740;
            py = 115;
            vy = 0;
        }
    }

    px = clampf(px, 0, WW - PW);
}

/* =====================================================
   FISICA DO JOGADOR
   ===================================================== */

static void update(SceCtrlData p, unsigned int old)
{
    float sp = 2.0f + .25f * (speedLevel - 1);
    float oldBottom;
    int left = (p.Buttons & PSP_CTRL_LEFT) || p.Lx < 90;
    int right = (p.Buttons & PSP_CTRL_RIGHT) || p.Lx > 165;
    int platformY;

    if (left) {
        px -= sp;
        face = 0;
    }

    if (right) {
        px += sp;
        face = 1;
    }

    px = clampf(px, 0, WW - PW);

    if (pressed(p.Buttons, old, PSP_CTRL_CROSS) && jumps > 0) {
        vy = -6.3f;
        grounded = 0;
        jumps--;
    }

    oldBottom = py + PH;

    vy += .34f;
    py += vy;
    grounded = 0;

    /* Chao da zona */
    if (py + PH >= GROUND) {
        py = GROUND - PH;
        vy = 0;
        grounded = 1;
        jumps = 2;
    }

    /*
     * Colisao com plataformas da zona atual.
     * Apenas aterrissagem de cima, quando o jogador cai.
     */
    if (vy >= 0) {
        platformY = platformAt(zone, px, py + PH, PW);

        if (platformY >= 0 &&
            oldBottom <= platformY + 3 &&
            py + PH >= platformY) {
            py = platformY - PH;
            vy = 0;
            grounded = 1;
            jumps = 2;
        }
    }

    if (magicCD > 0)
        magicCD--;

    /* Pegar cristal */
    if (zone == CAVE && !magic &&
        hit(pbox(), (Box){713, 100, 24, 24}))
        magic = 1;

    transition();
    checkpoints();
    updateMobs();
    updateShots();

    if (hp <= 0)
        respawn();

    cam = clampf(px - SW / 2, 0, WW - SW);
}

/* =====================================================
   CONFIGURACAO DE VIDEO PSP
   ===================================================== */

static void initGraphics(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, list);

    /*
     * Enderecos sao offsets da VRAM.
     * Cada framebuffer 512 x 272 x 4 bytes = 0x88000.
     * Os dois buffers nao devem se sobrepor.
     */
    sceGuDrawBuffer(GU_PSM_8888, (void *)0x00000, STRIDE);
    sceGuDispBuffer(SW, SH, (void *)0x88000, STRIDE);
    sceGuDepthBuffer((void *)0x110000, STRIDE);

    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SW, SH);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);

    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* =====================================================
   MAIN
   ===================================================== */

int main(void)
{
    SceCtrlData p;
    unsigned int old = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    memset(shots, 0, sizeof(shots));

    initGraphics();

    while (1) {
        sceCtrlReadBufferPositive(&p, 1);

        if (pressed(p.Buttons, old, PSP_CTRL_START))
            paused = !paused;

        if (!paused) {
            if (pressed(p.Buttons, old, PSP_CTRL_CIRCLE))
                cast();

            if (pressed(p.Buttons, old, PSP_CTRL_TRIANGLE))
                spend();

            if (pressed(p.Buttons, old, PSP_CTRL_LTRIGGER))
                selected = (selected + 4) % 5;

            if (pressed(p.Buttons, old, PSP_CTRL_RTRIGGER))
                selected = (selected + 1) % 5;

            if (pressed(p.Buttons, old, PSP_CTRL_SELECT))
                respawn();

            sword((p.Buttons & PSP_CTRL_SQUARE) != 0);
            update(p, old);
        }

        draw();
        old = p.Buttons;

        sceKernelDelayThread(16667);
    }

    sceGuDisplay(GU_FALSE);
    sceGuTerm();

    sceKernelExitGame();
    return 0;
}
