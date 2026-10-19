/*
 * RANDOM ALPHA
 * PSP / PSPSDK
 *
 * AREAS:
 * 0 - Tutorial: labirinto e minions
 * 1 - Arena do Guardiao
 *
 * CONTROLES:
 * Direcional / analogico = mover
 * X = pular / avancar dialogo
 * Quadrado = espada
 * Circulo = magia (apos pegar cristal)
 * Start = pausar
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM ALPHA", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define GROUND 238
#define PW 20
#define PH 34
#define WORLD_END 1600

typedef unsigned int Color;

typedef struct {
    float x, y, w, h;
} Box;

typedef struct {
    float x, y, vx, vy;
    int hp, alive, frame, type;
} Mob;

static unsigned int __attribute__((aligned(16))) displayList[262144];

#define C_BLACK 0xFF101010
#define C_DARK  0xFF292929
#define C_MID   0xFF555555
#define C_GRAY  0xFF888888
#define C_LIGHT 0xFFBBBBBB
#define C_WHITE 0xFFFFFFFF

enum {
    MODE_MENU,
    MODE_TUTORIAL,
    MODE_DIALOG,
    MODE_BOSS,
    MODE_PAUSE,
    MODE_WIN,
    MODE_GAMEOVER
};

static int mode = MODE_MENU;
static int previousMode = MODE_TUTORIAL;

static float px = 55, py = GROUND - PH;
static float vx = 0, vy = 0, cameraX = 0;

static int facing = 1;
static int grounded = 1;
static int jumps = 2;

static int hp = 5, maxHP = 5;
static int magicUnlocked = 0;
static int xp = 0, level = 1;

static int attackTimer = 0;
static int magicTimer = 0;
static int invincible = 0;
static int animationFrame = 0;
static int frameCounter = 0;

static int dialogPage = 0;
static int menuSelection = 0;
static int tutorialComplete = 0;

static int bossHP = 30;
static int bossMaxHP = 30;
static int bossAlive = 1;
static int bossAttackTimer = 0;
static int bossAnimation = 0;
static int bossHitFlash = 0;

static int crystalTaken = 0;
static int minionsDefeated = 0;

static Mob mobs[4];

static int pressed(unsigned int now,
                   unsigned int old,
                   unsigned int button)
{
    return (now & button) && !(old & button);
}

/* CORRECAO: colisao entre duas caixas */
static int hit(Box a, Box b)
{
    return a.x < b.x + b.w &&
           a.x + a.w > b.x &&
           a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

/* Retangulo desenhado com memoria fornecida pela GU */
static void rectangle(float x, float y,
                      float w, float h, Color color)
{
    float *v;

    if (w <= 0 || h <= 0)
        return;

    sceGuColor(color);

    v = (float *)sceGuGetMemory(12 * sizeof(float));

    v[0] = x;     v[1] = y;     v[2] = 0;
    v[3] = x + w; v[4] = y;     v[5] = 0;
    v[6] = x + w; v[7] = y + h; v[8] = 0;
    v[9] = x;     v[10] = y+h;  v[11] = 0;

    sceGuDrawArray(
        GU_TRIANGLE_FAN,
        GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        4, 0, v
    );
}

static void worldRect(float x, float y,
                      float w, float h, Color color)
{
    rectangle(x - cameraX, y, w, h, color);
}

/* Fonte pequena desenhada em pixels */
static void drawLetter(char c, int x, int y, Color color)
{
    static const unsigned char letters[][5] = {
        {14,17,19,21,14}, /* 0 */
        {4,12,4,4,14},    /* 1 */
        {14,17,2,4,31},   /* 2 */
        {30,1,6,1,30},    /* 3 */
        {2,6,10,31,2},    /* 4 */
        {31,16,30,1,30},  /* 5 */
        {14,16,30,17,14}, /* 6 */
        {31,1,2,4,4},     /* 7 */
        {14,17,14,17,14}, /* 8 */
        {14,17,15,1,14}   /* 9 */
    };

    unsigned char p[5];
    int i, j;

    if (c >= '0' && c <= '9') {
        memcpy(p, letters[c - '0'], 5);
    } else {
        switch (c) {
        case 'A': { unsigned char a[5]={14,17,31,17,17}; memcpy(p,a,5); break; }
        case 'B': { unsigned char a[5]={30,17,30,17,30}; memcpy(p,a,5); break; }
        case 'C': { unsigned char a[5]={14,17,16,17,14}; memcpy(p,a,5); break; }
        case 'D': { unsigned char a[5]={28,18,17,18,28}; memcpy(p,a,5); break; }
        case 'E': { unsigned char a[5]={31,16,30,16,31}; memcpy(p,a,5); break; }
        case 'F': { unsigned char a[5]={31,16,30,16,16}; memcpy(p,a,5); break; }
        case 'G': { unsigned char a[5]={14,16,23,17,14}; memcpy(p,a,5); break; }
        case 'H': { unsigned char a[5]={17,17,31,17,17}; memcpy(p,a,5); break; }
        case 'I': { unsigned char a[5]={14,4,4,4,14}; memcpy(p,a,5); break; }
        case 'J': { unsigned char a[5]={7,2,2,18,12}; memcpy(p,a,5); break; }
        case 'K': { unsigned char a[5]={17,18,28,18,17}; memcpy(p,a,5); break; }
        case 'L': { unsigned char a[5]={16,16,16,16,31}; memcpy(p,a,5); break; }
        case 'M': { unsigned char a[5]={17,27,21,17,17}; memcpy(p,a,5); break; }
        case 'N': { unsigned char a[5]={17,25,21,19,17}; memcpy(p,a,5); break; }
        case 'O': { unsigned char a[5]={14,17,17,17,14}; memcpy(p,a,5); break; }
        case 'P': { unsigned char a[5]={30,17,30,16,16}; memcpy(p,a,5); break; }
        case 'Q': { unsigned char a[5]={14,17,17,21,15}; memcpy(p,a,5); break; }
        case 'R': { unsigned char a[5]={30,17,30,20,18}; memcpy(p,a,5); break; }
        case 'S': { unsigned char a[5]={15,16,14,1,30}; memcpy(p,a,5); break; }
        case 'T': { unsigned char a[5]={31,4,4,4,4}; memcpy(p,a,5); break; }
        case 'U': { unsigned char a[5]={17,17,17,17,14}; memcpy(p,a,5); break; }
        case 'V': { unsigned char a[5]={17,17,17,10,4}; memcpy(p,a,5); break; }
        case 'W': { unsigned char a[5]={17,17,21,27,17}; memcpy(p,a,5); break; }
        case 'X': { unsigned char a[5]={17,10,4,10,17}; memcpy(p,a,5); break; }
        case 'Y': { unsigned char a[5]={17,10,4,4,4}; memcpy(p,a,5); break; }
        case 'Z': { unsigned char a[5]={31,2,4,8,31}; memcpy(p,a,5); break; }
        case ':': { unsigned char a[5]={0,4,0,4,0}; memcpy(p,a,5); break; }
        case '.': { unsigned char a[5]={0,0,0,0,4}; memcpy(p,a,5); break; }
        case '-': { unsigned char a[5]={0,0,31,0,0}; memcpy(p,a,5); break; }
        case '!': { unsigned char a[5]={4,4,4,0,4}; memcpy(p,a,5); break; }
        default:  { unsigned char a[5]={0,0,0,0,0}; memcpy(p,a,5); break; }
        }
    }

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            if (p[i] & (1 << (4-j)))
                rectangle(x + j*2, y + i*2, 2, 2, color);
        }
    }
}

static void drawText(const char *s, int x, int y, Color color)
{
    while (*s) {
        if (*s != ' ')
            drawLetter(*s, x, y, color);

        x += 12;
        s++;
    }
}

static void drawBar(int x, int y, int w, int h,
                    int value, int maximum)
{
    int fill;

    rectangle(x, y, w, h, C_WHITE);
    rectangle(x+2, y+2, w-4, h-4, C_BLACK);

    if (maximum <= 0)
        return;

    fill = (w-4) * value / maximum;

    if (fill > 0)
        rectangle(x+2, y+2, fill, h-4, C_LIGHT);
}

static void drawPlayer(void)
{
    float sx = px - cameraX;
    float bob = 0;

    if (!grounded)
        bob = -2;
    else if (vx != 0)
        bob = (animationFrame & 1) ? -2 : 0;

    if (invincible > 0 && (frameCounter & 3) == 0)
        return;

    /* Pernas */
    rectangle(sx+4, py+25+bob, 5, 9, C_DARK);
    rectangle(sx+12, py+25-bob, 5, 9, C_DARK);

    /* Botas */
    rectangle(sx+2, py+31+bob, 8, 3, C_WHITE);
    rectangle(sx+11, py+31-bob, 8, 3, C_WHITE);

    /* Corpo e ombreiras */
    rectangle(sx+3, py+12+bob, 14, 15, C_LIGHT);
    rectangle(sx, py+14+bob, 5, 8, C_WHITE);
    rectangle(sx+15, py+14+bob, 5, 8, C_WHITE);

    /* Cabeca */
    rectangle(sx+4, py+1+bob, 12, 11, C_WHITE);
    rectangle(sx+6, py+4+bob, 3, 3, C_BLACK);
    rectangle(sx+12, py+4+bob, 3, 3, C_BLACK);

    /* Cabelo */
    rectangle(sx+3, py+bob, 14, 3, C_DARK);

    /* Espada */
    if (attackTimer > 0) {
        if (facing > 0) {
            rectangle(sx+18, py+12, 20, 4, C_WHITE);
            rectangle(sx+20, py+13, 16, 2, C_DARK);
            rectangle(sx+18, py+9, 3, 10, C_LIGHT);
        } else {
            rectangle(sx-18, py+12, 20, 4, C_WHITE);
            rectangle(sx-16, py+13, 16, 2, C_DARK);
            rectangle(sx-1, py+9, 3, 10, C_LIGHT);
        }
    } else {
        if (facing > 0) {
            rectangle(sx+16, py+17, 3, 12, C_WHITE);
            rectangle(sx+17, py+14, 2, 5, C_LIGHT);
        } else {
            rectangle(sx+1, py+17, 3, 12, C_WHITE);
            rectangle(sx+1, py+14, 2, 5, C_LIGHT);
        }
    }
}

static void drawMinion(Mob *m)
{
    float sx = m->x - cameraX;
    float bob = (m->frame & 1) ? 2 : 0;

    if (!m->alive)
        return;

    /* Corpo com pontas */
    rectangle(sx+5, m->y+5+bob, 13, 15, C_DARK);
    rectangle(sx+2, m->y+9+bob, 4, 8, C_MID);
    rectangle(sx+17, m->y+9+bob, 4, 8, C_MID);

    /* Cabeca */
    rectangle(sx+4, m->y+bob, 15, 10, C_GRAY);
    rectangle(sx+6, m->y+3+bob, 4, 3, C_WHITE);
    rectangle(sx+13, m->y+3+bob, 4, 3, C_WHITE);

    /* Pernas */
    rectangle(sx+6, m->y+19+bob, 4, 6, C_WHITE);
    rectangle(sx+14, m->y+19-bob, 4, 6, C_WHITE);
}

static void drawBoss(void)
{
    float x = 1270 - cameraX;
    float y = 163;
    int swing = (bossAnimation & 1) ? 5 : 0;
    Color armor = bossHitFlash > 0 ? C_WHITE : C_DARK;

    if (!bossAlive)
        return;

    /* Capa */
    rectangle(x+13, y+22, 42, 48, C_MID);
    rectangle(x+20, y+35, 28, 32, C_DARK);

    /* Pernas */
    rectangle(x+19, y+60, 12, 13, armor);
    rectangle(x+38, y+60, 12, 13, armor);
    rectangle(x+16, y+70, 17, 5, C_WHITE);
    rectangle(x+37, y+70, 17, 5, C_WHITE);

    /* Bracos articulados */
    rectangle(x+4, y+25+swing, 13, 29, armor);
    rectangle(x+51, y+25-swing, 13, 29, armor);

    /* Ombreiras */
    rectangle(x, y+21+swing, 19, 13, C_LIGHT);
    rectangle(x+49, y+21-swing, 19, 13, C_LIGHT);

    /* Torso angular */
    rectangle(x+16, y+17, 37, 44, armor);
    rectangle(x+22, y+24, 25, 25, C_MID);
    rectangle(x+27, y+30, 6, 7, C_WHITE);
    rectangle(x+37, y+30, 6, 7, C_WHITE);

    /* Chifres */
    rectangle(x+18, y+1, 7, 15, C_WHITE);
    rectangle(x+43, y+1, 7, 15, C_WHITE);

    /* Cabeca */
    rectangle(x+20, y+6, 29, 19, C_LIGHT);
    rectangle(x+23, y+11, 7, 5, C_BLACK);
    rectangle(x+39, y+11, 7, 5, C_BLACK);
    rectangle(x+31, y+16, 7, 3, C_DARK);

    /* Marca central */
    rectangle(x+31, y+39, 7, 13, C_WHITE);
}

static void drawTutorial(void)
{
    int i;

    /* Fundo monocromatico */
    rectangle(0, 0, SW, SH, C_BLACK);

    /* Montanhas */
    for (i = 0; i < 6; i++) {
        rectangle(i*95 - (int)(cameraX*0.15f),
                  80+(i%3)*13, 70, 100, C_DARK);
    }

    /* Plataformas do labirinto */
    worldRect(0, GROUND, 900, 34, C_MID);
    worldRect(120, 190, 100, 9, C_GRAY);
    worldRect(275, 165, 95, 9, C_GRAY);
    worldRect(430, 185, 110, 9, C_GRAY);
    worldRect(580, 160, 95, 9, C_GRAY);
    worldRect(720, 190, 90, 9, C_GRAY);

    /* Colunas e paredes */
    worldRect(235, 205, 22, 33, C_DARK);
    worldRect(390, 185, 20, 53, C_DARK);
    worldRect(550, 210, 20, 28, C_DARK);
    worldRect(680, 200, 18, 38, C_DARK);

    /* Cristal magico */
    if (!crystalTaken) {
        int pulse = (frameCounter & 1) ? 2 : 0;
        worldRect(625, 140-pulse, 12, 12, C_WHITE);
        worldRect(628, 143-pulse, 6, 6, C_BLACK);
        worldRect(630, 145-pulse, 2, 2, C_LIGHT);
    }

    /* Saida */
    worldRect(830, 185, 10, 53, C_WHITE);
    worldRect(842, 195, 4, 43, C_GRAY);

    for (i = 0; i < 4; i++)
        drawMinion(&mobs[i]);

    drawPlayer();

    drawText("TUTORIAL", 12, 10, C_WHITE);
    drawText("X PULA", 12, 25, C_LIGHT);
    drawText("QUADRADO ESPADA", 12, 38, C_LIGHT);

    if (!crystalTaken)
        drawText("PEGUE O CRISTAL", 12, 51, C_WHITE);
    else
        drawText("MAGIA LIBERADA", 12, 51, C_WHITE);

    drawText("ENCONTRE A SAIDA", 12, 64, C_WHITE);
}

static void drawArena(void)
{
    int i;

    rectangle(0, 0, SW, SH, C_BLACK);

    /* Colunas da arena */
    worldRect(900, 0, 25, GROUND, C_DARK);
    worldRect(1510, 0, 25, GROUND, C_DARK);

    for (i = 0; i < 7; i++) {
        worldRect(940+i*80, 80+(i%2)*20, 14, 100, C_MID);
        worldRect(946+i*80, 90+(i%2)*20, 2, 80, C_LIGHT);
    }

    /* Piso */
    worldRect(900, GROUND, 635, 34, C_MID);
    worldRect(900, GROUND, 635, 4, C_WHITE);

    drawBoss();
    drawPlayer();

    drawText("ARENA DO GUARDIAO", 12, 10, C_WHITE);
    drawText("QUADRADO: ATACAR", 12, 25, C_LIGHT);

    if (magicUnlocked)
        drawText("CIRCULO: MAGIA", 12, 38, C_LIGHT);
}

static void drawHUD(void)
{
    drawText("HP", 12, 249, C_WHITE);
    drawBar(42, 249, 90, 12, hp, maxHP);

    drawText("LV", 150, 249, C_WHITE);
    drawBar(174, 249, 60, 12, xp, 100);

    if (mode == MODE_BOSS || mode == MODE_DIALOG) {
        if (bossAlive) {
            drawText("GUARDIAO", 300, 10, C_WHITE);
            drawBar(300, 23, 150, 12, bossHP, bossMaxHP);
        }
    }
}

static void drawDialog(void)
{
    rectangle(0, 0, SW, SH, C_BLACK);

    if (!tutorialComplete) {
        drawText("RANDOM ALPHA", 145, 45, C_WHITE);
        drawText("VOCE DESPERTOU", 100, 100, C_LIGHT);
        drawText("NO LABIRINTO", 130, 120, C_LIGHT);
        drawText("X PARA CONTINUAR", 110, 190, C_WHITE);
    } else {
        drawText("GUARDIAO", 170, 45, C_WHITE);
        drawText("VOCE CHEGOU", 150, 100, C_LIGHT);
        drawText("A ARENA FINAL", 145, 120, C_LIGHT);
        drawText("X PARA LUTAR", 155, 190, C_WHITE);
    }
}

static void drawMenu(void)
{
    rectangle(0, 0, SW, SH, C_BLACK);

    drawText("RANDOM ALPHA", 160, 55, C_WHITE);
    drawText("TUTORIAL E ARENA", 130, 85, C_LIGHT);

    if (menuSelection == 0)
        drawText("> NOVO JOGO", 155, 140, C_WHITE);
    else
        drawText("  NOVO JOGO", 155, 140, C_LIGHT);

    if (menuSelection == 1)
        drawText("> CONTROLES", 155, 165, C_WHITE);
    else
        drawText("  CONTROLES", 155, 165, C_LIGHT);

    drawText("START PARA PAUSAR", 130, 225, C_GRAY);
}

static void drawPause(void)
{
    rectangle(0, 0, SW, SH, C_BLACK);
    drawText("JOGO PAUSADO", 160, 75, C_WHITE);
    drawText("START CONTINUA", 140, 120, C_LIGHT);
    drawText("SELECT REINICIA", 140, 145, C_LIGHT);
}

static void drawWin(void)
{
    rectangle(0, 0, SW, SH, C_BLACK);
    drawText("GUARDIAO DERROTADO", 120, 60, C_WHITE);
    drawText("VITORIA!", 190, 105, C_LIGHT);
    drawText("X PARA RECOMECAR", 130, 170, C_WHITE);
}

static void drawGameOver(void)
{
    rectangle(0, 0, SW, SH, C_BLACK);
    drawText("VOCE CAIU", 180, 80, C_WHITE);
    drawText("X PARA TENTAR DE NOVO", 100, 145, C_LIGHT);
}

static void initMobs(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        mobs[i].x = 270 + i*140;
        mobs[i].y = GROUND - 25;
        mobs[i].vx = 0;
        mobs[i].vy = 0;
        mobs[i].hp = 3;
        mobs[i].alive = 1;
        mobs[i].frame = i & 1;
        mobs[i].type = i;
    }

    minionsDefeated = 0;
}

static void resetGame(void)
{
    px = 55;
    py = GROUND - PH;
    vx = 0;
    vy = 0;
    cameraX = 0;

    facing = 1;
    grounded = 1;
    jumps = 2;

    hp = maxHP;
    xp = 0;
    level = 1;

    attackTimer = 0;
    magicTimer = 0;
    invincible = 0;
    animationFrame = 0;
    frameCounter = 0;

    tutorialComplete = 0;
    crystalTaken = 0;
    magicUnlocked = 0;

    bossHP = bossMaxHP;
    bossAlive = 1;
    bossAttackTimer = 0;
    bossAnimation = 0;
    bossHitFlash = 0;

    initMobs();

    mode = MODE_TUTORIAL;
}

static void damageBoss(int damage)
{
    if (!bossAlive)
        return;

    bossHP -= damage;
    bossHitFlash = 8;

    if (bossHP <= 0) {
        bossHP = 0;
        bossAlive = 0;
        xp += 100;
        mode = MODE_WIN;
    }
}

static void updateMobs(void)
{
    int i;

    Box player = { px, py, PW, PH };

    for (i = 0; i < 4; i++) {
        Mob *m = &mobs[i];
        Box enemy;

        if (!m->alive)
            continue;

        m->frame++;

        if (px < m->x)
            m->vx = -0.6f;
        else
            m->vx = 0.6f;

        m->x += m->vx;

        if (m->x < 30)
            m->x = 30;

        if (m->x > 850)
            m->x = 850;

        enemy.x = m->x;
        enemy.y = m->y;
        enemy.w = 22;
        enemy.h = 25;

        if (hit(player, enemy) && invincible == 0) {
            hp--;
            invincible = 60;

            if (hp <= 0) {
                hp = 0;
                mode = MODE_GAMEOVER;
            }
        }

        if (attackTimer == 7) {
            Box sword;

            sword.x = facing > 0 ? px + PW : px - 24;
            sword.y = py + 10;
            sword.w = 24;
            sword.h = 18;

            if (hit(sword, enemy)) {
                m->hp--;

                if (m->hp <= 0) {
                    m->alive = 0;
                    minionsDefeated++;
                    xp += 10;

                    if (xp >= 100) {
                        xp -= 100;
                        level++;
                        if (maxHP < 10)
                            maxHP++;
                        hp = maxHP;
                    }
                }
            }
        }
    }
}

static void updateGame(unsigned int buttons,
                       unsigned int oldButtons)
{
    int left = (buttons & PSP_CTRL_LEFT) ||
               (buttons & PSP_CTRL_ANALOG_LEFT);

    int right = (buttons & PSP_CTRL_RIGHT) ||
                (buttons & PSP_CTRL_ANALOG_RIGHT);

    if (left) {
        vx = -2.2f;
        facing = -1;
    } else if (right) {
        vx = 2.2f;
        facing = 1;
    } else {
        vx *= 0.65f;
        if (fabsf(vx) < 0.15f)
            vx = 0;
    }

    if (pressed(buttons, oldButtons, PSP_CTRL_CROSS)) {
        if (jumps > 0) {
            vy = -6.7f;
            grounded = 0;
            jumps--;
        }
    }

    if (pressed(buttons, oldButtons, PSP_CTRL_SQUARE))
        attackTimer = 12;

    if (pressed(buttons, oldButtons, PSP_CTRL_CIRCLE) &&
        magicUnlocked)
        magicTimer = 18;

    px += vx;
    py += vy;
    vy += 0.30f;

    if (py + PH >= GROUND) {
        py = GROUND - PH;
        vy = 0;
        grounded = 1;
        jumps = 2;
    }

    if (px < 0)
        px = 0;

    if (px > WORLD_END - PW)
        px = WORLD_END - PW;

    if (attackTimer > 0)
        attackTimer--;

    if (magicTimer > 0)
        magicTimer--;

    if (invincible > 0)
        invincible--;

    if (bossHitFlash > 0)
        bossHitFlash--;

    animationFrame++;
    frameCounter++;

    /* Pegar cristal */
    if (!crystalTaken &&
        px > 610 && px < 660 &&
        py < GROUND - 5) {
        crystalTaken = 1;
        magicUnlocked = 1;
    }

    if (mode == MODE_TUTORIAL) {
        updateMobs();

        if (px >= 860 && !tutorialComplete) {
            tutorialComplete = 1;
            px = 930;
            vx = 0;
            mode = MODE_DIALOG;
            dialogPage = 2;
        }
    }

    if (mode == MODE_BOSS && bossAlive) {
        bossAnimation++;
        bossAttackTimer++;

        if (attackTimer == 7 &&
            fabsf(px - 1270) < 85) {
            damageBoss(2);
        }

        if (magicTimer == 12 &&
            magicUnlocked &&
            fabsf(px - 1270) < 260) {
            damageBoss(4);
        }

        if (bossAttackTimer >= 90) {
            bossAttackTimer = 0;

            if (fabsf(px - 1270) < 100 &&
                invincible == 0) {
                hp--;
                invincible = 60;

                if (hp <= 0) {
                    hp = 0;
                    mode = MODE_GAMEOVER;
                }
            }
        }
    }

    cameraX = px - SW / 2.0f;

    if (cameraX < 0)
        cameraX = 0;

    if (cameraX > WORLD_END - SW)
        cameraX = WORLD_END - SW;
}

int main(void)
{
    SceCtrlData pad;
    unsigned int oldButtons = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();

    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(GU_PSM_8888,
                    (void *)0,
                    512);

    sceGuDispBuffer(SW, SH,
                    (void *)0x88000,
                    512);

    sceGuDepthBuffer((void *)0x110000,
                     512);

    sceGuOffset(2048 - (SW / 2),
                2048 - (SH / 2));

    sceGuViewport(2048, 2048, SW, SH);
    sceGuDepthRange(0, 65535);

    sceGuScissor(0, 0, SW, SH);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    resetGame();
    mode = MODE_MENU;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        sceGuStart(GU_DIRECT, displayList);

        sceGuClearColor(C_BLACK);
        sceGuClear(GU_COLOR_BUFFER_BIT |
                   GU_DEPTH_BUFFER_BIT);

        switch (mode) {
        case MODE_MENU:
            drawMenu();

            if (pressed(pad.Buttons, oldButtons,
                        PSP_CTRL_UP) ||
                pressed(pad.Buttons, oldButtons,
                        PSP_CTRL_DOWN)) {
                menuSelection = 1 - menuSelection;
            }

            if (pressed(pad.Buttons, oldButtons,
                        PSP_CTRL_CROSS)) {
                if (menuSelection == 0) {
                    resetGame();
                } else {
                    mode = MODE_DIALOG;
                    tutorialComplete = 0;
                    dialogPage = 0;
                }
            }
            break;

        case MODE_TUTORIAL:
            updateGame(pad.Buttons, oldButtons);
            drawTutorial();
            drawHUD();
            break;

        case MODE_DIALOG:
            drawDialog();

            if (pressed(pad.Buttons, oldButtons,
                        PSP_CTRL_CROSS)) {
                if (tutorialComplete) {
                    mode = MODE_BOSS;
                    px = 1050;
                    py = GROUND - PH;
                    vx = 0;
                    vy = 0;
                } else {
                    mode = MODE_TUTORIAL;
                }
            }
            break;

        case MODE_BOSS:
            updateGame(pad.Buttons, oldButtons);
            drawArena();
            drawHUD();
            break;

        case MODE_PAUSE:
            drawPause();

            if (pressed(pad.Buttons, oldButtons,
                        PSP_CTRL_START)) {
                mode = previousMode;
            }

            if (pressed(pad.Buttons, oldButtons,
                        PSP_CTRL_SELECT)) {
                resetGame();
            }
            break;

        case MODE_WIN:
            drawWin();

            if (pressed(pad.Buttons, oldButtons,
                        PSP_CTRL_CROSS)) {
                resetGame();
            }
            break;

        case MODE_GAMEOVER:
            drawGameOver();

            if (pressed(pad.Buttons, oldButtons,
                        PSP_CTRL_CROSS)) {
                resetGame();
            }
            break;
        }

        if ((mode == MODE_TUTORIAL ||
             mode == MODE_BOSS) &&
            pressed(pad.Buttons, oldButtons,
                    PSP_CTRL_START)) {
            previousMode = mode;
            mode = MODE_PAUSE;
        }

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        oldButtons = pad.Buttons;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
