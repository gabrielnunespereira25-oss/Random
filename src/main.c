/*
 * RANDOM ALPHA
 * Duas areas: Tutorial + Arena do Guardiao
 * PSP / PSPSDK
 *
 * CONTROLES:
 * Setas/analogico = mover
 * X = pular / avancar dialogo
 * Quadrado = espada
 * Circulo = magia (apos pegar cristal)
 * Start = pausar
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspdebug.h>
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

static unsigned int __attribute__((aligned(16)))
    displayList[262144];

/* PALETA MONOCROMATICA */
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

typedef struct {
    float x, y;
    float vx, vy;
    int hp;
    int alive;
    int frame;
    int type;
} Mob;

static int mode = MODE_MENU;
static int previousMode = MODE_TUTORIAL;

static float px = 55;
static float py = GROUND - PH;
static float vx = 0;
static float vy = 0;
static float cameraX = 0;

static int facing = 1;
static int grounded = 1;
static int jumps = 2;

static int hp = 5;
static int maxHP = 5;
static int magicUnlocked = 0;
static int xp = 0;
static int level = 1;

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

/* ---------- UTILITARIOS ---------- */

static int pressed(unsigned int now,
                   unsigned int old,
                   unsigned int button) {
    return (now & button) && !(old & button);
}

static void rectangle(float x, float y,
                      float w, float h, Color color) {
    if (w <= 0 || h <= 0) return;

    float vertices[12] __attribute__((aligned(16))) = {
        x,     y,     0,
        x + w, y,     0,
        x + w, y + h, 0,
        x,     y + h, 0
    };

    sceGuColor(color);
    sceGuDrawArray(
        GU_TRIANGLE_FAN,
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        4, 0, vertices
    );
}

static void worldRect(float x, float y,
                      float w, float h, Color c) {
    rectangle(x - cameraX, y, w, h, c);
}

/* Fonte pequena de emergencia para interface.
   Cada letra e desenhada com formas geometricas. */
static void drawLetter(int x, int y, char ch, Color c) {
    static const unsigned char letters[26][5] = {
        {31,17,31,17,17},{30,17,30,17,30},
        {15,16,16,16,15},{30,17,17,17,30},
        {31,16,30,16,31},{31,16,30,16,16},
        {15,16,23,17,15},{17,17,31,17,17},
        {31,4,4,4,31},{7,2,2,18,12},
        {17,18,28,18,17},{16,16,16,16,31},
        {17,27,21,17,17},{17,25,21,19,17},
        {14,17,17,17,14},{30,17,30,16,16},
        {14,17,21,18,13},{30,17,30,20,18},
        {15,16,14,1,30},{31,4,4,4,4},
        {17,17,17,17,14},{17,17,17,10,4},
        {17,17,21,21,10},{17,10,4,10,17},
        {17,10,4,4,4},{31,2,4,8,31}
    };

    if (ch >= 'a' && ch <= 'z')
        ch -= 'a' - 'A';

    if (ch >= 'A' && ch <= 'Z') {
        int i = ch - 'A';

        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 5; col++)
                if (letters[i][row] & (1 << (4-col)))
                    rectangle(x+col*2, y+row*2, 2, 2, c);
    } else if (ch >= '0' && ch <= '9') {
        static const unsigned char digits[10][5] = {
            {31,17,17,17,31},{4,12,4,4,14},
            {30,1,14,16,31},{30,1,14,1,30},
            {18,18,31,2,2},{31,16,30,1,30},
            {14,16,30,17,14},{31,1,2,4,4},
            {14,17,14,17,14},{14,17,15,1,14}
        };

        int i = ch - '0';
        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 5; col++)
                if (digits[i][row] & (1 << (4-col)))
                    rectangle(x+col*2, y+row*2, 2, 2, c);
    } else if (ch == '-') {
        rectangle(x, y+4, 8, 2, c);
    } else if (ch == ':') {
        rectangle(x+3, y+2, 2, 2, c);
        rectangle(x+3, y+8, 2, 2, c);
    }
}

static void drawText(int x, int y, const char *s, Color c) {
    while (*s) {
        if (*s == ' ') {
            x += 7;
        } else {
            drawLetter(x, y, *s, c);
            x += 12;
        }
        s++;
    }
}

static void drawBar(int x, int y, int w,
                    int h, int value, int maxValue) {
    rectangle(x, y, w, h, C_DARK);

    if (maxValue > 0 && value > 0) {
        int fill = w * value / maxValue;
        rectangle(x, y, fill, h, C_WHITE);
    }

    rectangle(x, y, w, 1, C_LIGHT);
}

/* ---------- PERSONAGEM ---------- */

/* Silhueta original: cabelo espetado, casaco,
   cinto, calcas, botas e espada. */
static void drawPlayer(void) {
    float x = px - cameraX;
    float bob = 0;

    if (grounded && (vx > 0.1f || vx < -0.1f))
        bob = (animationFrame % 2) ? -2 : 0;

    float y = py + bob;

    /* Sombra */
    rectangle(x-3, GROUND-2, 27, 3, C_DARK);

    /* Cabelo espetado */
    rectangle(x+4, y+1, 12, 4, C_BLACK);
    rectangle(x+2, y+3, 5, 5, C_BLACK);
    rectangle(x+7, y-2, 4, 5, C_BLACK);
    rectangle(x+13, y-1, 4, 5, C_BLACK);

    /* Rosto */
    rectangle(x+5, y+5, 11, 10, C_LIGHT);

    if (facing) {
        rectangle(x+12, y+8, 2, 2, C_BLACK);
    } else {
        rectangle(x+7, y+8, 2, 2, C_BLACK);
    }

    /* Gola */
    rectangle(x+6, y+14, 10, 4, C_WHITE);

    /* Casaco */
    rectangle(x+3, y+17, 15, 10, C_DARK);
    rectangle(x+7, y+17, 4, 10, C_LIGHT);
    rectangle(x+3, y+26, 15, 3, C_BLACK);

    /* Bracos */
    rectangle(x+1, y+18, 4, 9, C_LIGHT);
    rectangle(x+16, y+18, 4, 9, C_LIGHT);

    /* Pernas animadas */
    if (grounded && fabsf(vx) > 0.1f) {
        if (animationFrame % 2) {
            rectangle(x+4, y+29, 5, 5, C_LIGHT);
            rectangle(x+12, y+28, 5, 6, C_GRAY);
        } else {
            rectangle(x+4, y+28, 5, 6, C_GRAY);
            rectangle(x+12, y+29, 5, 5, C_LIGHT);
        }
    } else {
        rectangle(x+4, y+28, 5, 5, C_GRAY);
        rectangle(x+12, y+28, 5, 5, C_GRAY);
    }

    /* Botas */
    rectangle(x+2, y+32, 7, 3, C_WHITE);
    rectangle(x+11, y+32, 8, 3, C_WHITE);

    /* Espada animada durante o ataque */
    if (attackTimer > 0) {
        if (facing) {
            rectangle(x+15, y+15, 17, 4, C_WHITE);
            rectangle(x+19, y+13, 11, 2, C_LIGHT);
            rectangle(x+14, y+12, 3, 10, C_GRAY);
            rectangle(x+10, y+16, 5, 3, C_BLACK);
        } else {
            rectangle(x-12, y+15, 17, 4, C_WHITE);
            rectangle(x-10, y+13, 11, 2, C_LIGHT);
            rectangle(x+3, y+12, 3, 10, C_GRAY);
            rectangle(x+5, y+16, 5, 3, C_BLACK);
        }
    } else {
        if (facing) {
            rectangle(x+15, y+18, 13, 3, C_WHITE);
            rectangle(x+18, y+17, 9, 1, C_LIGHT);
            rectangle(x+14, y+15, 3, 8, C_GRAY);
            rectangle(x+11, y+18, 4, 3, C_BLACK);
        } else {
            rectangle(x-8, y+18, 13, 3, C_WHITE);
            rectangle(x-7, y+17, 9, 1, C_LIGHT);
            rectangle(x+3, y+15, 3, 8, C_GRAY);
            rectangle(x+5, y+18, 4, 3, C_BLACK);
        }
    }

    /* Efeito de invulnerabilidade */
    if (invincible > 0 && animationFrame % 2 == 0) {
        rectangle(x-4, y-4, 27, 2, C_WHITE);
    }
}

/* ---------- MINIONS ---------- */

/* Dois tipos: perseguidor e saltador.
   Cada um tem corpo e animacao proprios. */
static void drawMinion(Mob *m) {
    if (!m->alive) return;

    float x = m->x - cameraX;
    float y = m->y;
    int step = m->frame % 2;

    /* Sombra */
    rectangle(x-2, GROUND-2, 23, 3, C_DARK);

    if (m->type == 0) {
        /* Perseguidor: criatura baixa com chifres */
        rectangle(x+4, y+5, 13, 16, C_GRAY);
        rectangle(x+2, y+9, 17, 10, C_LIGHT);

        rectangle(x+2, y+3, 4, 7, C_WHITE);
        rectangle(x+14, y+3, 4, 7, C_WHITE);

        rectangle(x+5, y+10, 3, 3, C_BLACK);
        rectangle(x+13, y+10, 3, 3, C_BLACK);

        rectangle(x+4, y+20, 5, 5-step, C_WHITE);
        rectangle(x+13, y+20, 5, 5+step, C_WHITE);
    } else {
        /* Saltador: corpo estreito, olhos grandes */
        rectangle(x+5, y+2, 11, 20, C_LIGHT);
        rectangle(x+2, y+7, 17, 9, C_GRAY);

        rectangle(x+4, y+9, 4, 4, C_WHITE);
        rectangle(x+13, y+9, 4, 4, C_WHITE);
        rectangle(x+5, y+10, 2, 2, C_BLACK);
        rectangle(x+14, y+10, 2, 2, C_BLACK);

        rectangle(x+1, y+16, 5, 7-step, C_WHITE);
        rectangle(x+15, y+16, 5, 7+step, C_WHITE);
    }
}

/* ---------- GUARDIAO ---------- */

/* Chefe grande, com chifres, armadura, olhos e bracos.
   A animacao altera os bracos e o corpo. */
static void drawBoss(void) {
    if (!bossAlive) return;

    float x = 1270 - cameraX;
    float y = 174;

    int swing = 0;

    if (bossAttackTimer > 35 && bossAttackTimer < 60)
        swing = 9;

    if (bossHitFlash > 0)
        rectangle(x-8, y-12, 63, 76, C_LIGHT);

    /* Sombra */
    rectangle(x-7, GROUND-3, 64, 4, C_DARK);

    /* Chifres */
    rectangle(x+9, y+1, 7, 14, C_WHITE);
    rectangle(x+14, y-4, 5, 8, C_LIGHT);
    rectangle(x+34, y+1, 7, 14, C_WHITE);
    rectangle(x+35, y-4, 5, 8, C_LIGHT);

    /* Cabeca */
    rectangle(x+11, y+8, 29, 24, C_GRAY);
    rectangle(x+15, y+13, 21, 14, C_DARK);

    /* Olhos */
    rectangle(x+16, y+16, 7, 4, C_WHITE);
    rectangle(x+29, y+16, 7, 4, C_WHITE);
    rectangle(x+18, y+17, 3, 3, C_BLACK);
    rectangle(x+31, y+17, 3, 3, C_BLACK);

    /* Mandibula */
    rectangle(x+18, y+25, 16, 5, C_LIGHT);
    rectangle(x+21, y+25, 3, 4, C_BLACK);
    rectangle(x+28, y+25, 3, 4, C_BLACK);

    /* Armadura do tronco */
    rectangle(x+7, y+32, 38, 24, C_WHITE);
    rectangle(x+12, y+35, 28, 17, C_GRAY);
    rectangle(x+20, y+35, 12, 17, C_DARK);

    /* Placas */
    rectangle(x+2, y+34, 10, 12, C_LIGHT);
    rectangle(x+40, y+34, 10, 12, C_LIGHT);

    /* Bracos animados */
    rectangle(x-2, y+39+swing, 9, 17, C_GRAY);
    rectangle(x+45, y+39-swing, 9, 17, C_GRAY);

    rectangle(x-4, y+53+swing, 13, 8, C_WHITE);
    rectangle(x+43, y+53-swing, 13, 8, C_WHITE);

    /* Pernas */
    rectangle(x+11, y+55, 11, 14, C_GRAY);
    rectangle(x+31, y+55, 11, 14, C_GRAY);

    rectangle(x+7, y+67, 17, 5, C_WHITE);
    rectangle(x+29, y+67, 17, 5, C_WHITE);

    /* Marcas da armadura */
    rectangle(x+22, y+39, 4, 9, C_WHITE);
    rectangle(x+17, y+46, 14, 3, C_LIGHT);
}

/* ---------- AREAS ---------- */

static void drawTutorial(void) {
    rectangle(0, 0, SW, SH, C_BLACK);

    /* Teto rochoso */
    worldRect(0, 0, 900, GROUND, C_DARK);

    for (int i=0; i<18; i++) {
        float x = i * 52.0f;
        worldRect(x, 0, 8, 10+(i%3)*5, C_GRAY);
    }

    /* Chao */
    worldRect(0, GROUND, 900, 34, C_GRAY);
    worldRect(0, GROUND+31, 900, 8, C_BLACK);

    /* Plataformas */
    worldRect(150, 188, 92, 8, C_WHITE);
    worldRect(300, 155, 85, 8, C_LIGHT);
    worldRect(450, 185, 90, 8, C_WHITE);
    worldRect(590, 145, 100, 8, C_LIGHT);

    /* Obstaculos */
    worldRect(265, 201, 12, 37, C_MID);
    worldRect(405, 202, 12, 36, C_MID);
    worldRect(550, 198, 12, 40, C_MID);

    /* Cristal de magia */
    if (!crystalTaken) {
        worldRect(625, 111, 20, 20, C_LIGHT);
        worldRect(630, 105, 10, 32, C_WHITE);
        worldRect(634, 108, 3, 25, C_BLACK);
    }

    /* Portal para a arena */
    worldRect(820, 145, 10, 93, C_LIGHT);
    worldRect(845, 145, 10, 93, C_LIGHT);
    worldRect(820, 140, 35, 7, C_WHITE);

    /* Minions */
    for (int i=0; i<4; i++)
        drawMinion(&mobs[i]);

    drawPlayer();

    drawText(12, 9, "AREA 1", C_WHITE);
    drawText(12, 23, "TUTORIAL", C_LIGHT);

    drawText(12, 47, "X PULAR", C_WHITE);
    drawText(12, 62, "QUADRADO ESPADA", C_WHITE);
    drawText(12, 77, "CIRCULO MAGIA", C_WHITE);

    if (!crystalTaken)
        drawText(275, 88, "CRISTAL", C_WHITE);

    drawText(12, 103, "ENCONTRE A SAIDA", C_LIGHT);
}

static void drawArena(void) {
    rectangle(0, 0, SW, SH, C_BLACK);

    /* Parede da arena */
    worldRect(900, 0, 700, GROUND, C_DARK);

    for (int i=0; i<13; i++) {
        float x=930+i*50;
        worldRect(x, 20, 8, 20+(i%3)*8, C_GRAY);
    }

    /* Colunas */
    worldRect(970, 95, 22, 143, C_MID);
    worldRect(1510, 95, 22, 143, C_MID);
    worldRect(960, 95, 42, 8, C_WHITE);
    worldRect(1500, 95, 42, 8, C_WHITE);

    /* Chao da arena */
    worldRect(900, GROUND, 700, 34, C_GRAY);
    worldRect(900, GROUND+31, 700, 8, C_BLACK);

    /* Marcas no chao */
    for (int i=0; i<7; i++)
        worldRect(1010+i*70, 228, 28, 3, C_LIGHT);

    drawBoss();
    drawPlayer();

    drawText(12, 9, "AREA 2", C_WHITE);
    drawText(12, 23, "ARENA DO GUARDIAO", C_LIGHT);

    if (bossAlive) {
        drawText(160, 9, "GUARDIAO", C_WHITE);
        drawBar(160, 23, 180, 8, bossHP, bossMaxHP);
    } else {
        drawText(150, 60, "CHEFE DERROTADO", C_WHITE);
    }
}

/* ---------- HUD E TELAS ---------- */

static void drawHUD(void) {
    drawText(10, 249, "VIDA", C_WHITE);
    drawBar(68, 249, 75, 9, hp, maxHP);

    drawText(165, 249, "NIVEL", C_WHITE);
    drawText(225, 249, "XP", C_WHITE);
    drawBar(253, 249, 75, 9, xp, 50);

    if (magicUnlocked)
        drawText(345, 249, "MAGIA", C_WHITE);
}

static void drawDialog(void) {
    rectangle(15, 172, 450, 84, C_BLACK);
    rectangle(15, 172, 450, 2, C_WHITE);
    rectangle(15, 254, 450, 2, C_WHITE);

    if (dialogPage == 0) {
        drawText(28, 187, "BEM VINDO AO RANDOM", C_WHITE);
        drawText(28, 204, "USE AS SETAS PARA ANDAR", C_LIGHT);
        drawText(28, 221, "X PARA PULAR", C_LIGHT);
    } else if (dialogPage == 1) {
        drawText(28, 187, "PEGUE O CRISTAL", C_WHITE);
        drawText(28, 204, "CIRCULO USA MAGIA", C_LIGHT);
        drawText(28, 221, "QUADRADO ATACA", C_LIGHT);
    } else {
        drawText(28, 187, "O GUARDIAO DESPERTOU", C_WHITE);
        drawText(28, 204, "ATAQUE COM ESPADA E MAGIA", C_LIGHT);
        drawText(28, 221, "X PARA CONTINUAR", C_WHITE);
    }
}

static void drawMenu(void) {
    rectangle(0, 0, SW, SH, C_BLACK);
    rectangle(30, 25, 420, 220, C_DARK);
    rectangle(38, 33, 404, 204, C_BLACK);

    drawText(115, 55, "RANDOM ALPHA", C_WHITE);
    drawText(140, 88, "TUTORIAL E CHEFE", C_LIGHT);

    const char *opts[2] = {"JOGAR", "REINICIAR"};

    for (int i=0; i<2; i++) {
        if (i==menuSelection)
            rectangle(140, 125+i*30, 200, 22, C_MID);

        drawText(160, 132+i*30, opts[i],
                 i==menuSelection ? C_WHITE : C_GRAY);
    }

    drawText(72, 208, "DEMO PSP", C_LIGHT);
}

static void drawPause(void) {
    rectangle(100, 80, 280, 100, C_BLACK);
    rectangle(100, 80, 280, 3, C_WHITE);
    drawText(180, 100, "PAUSADO", C_WHITE);
    drawText(120, 130, "START CONTINUA", C_LIGHT);
}

static void drawWin(void) {
    rectangle(35, 65, 410, 135, C_BLACK);
    rectangle(35, 65, 410, 3, C_WHITE);
    drawText(115, 88, "GUARDIAO DERROTADO", C_WHITE);
    drawText(105, 120, "FIM DA DEMO", C_LIGHT);
    drawText(110, 150, "START MENU", C_WHITE);
}

/* ---------- JOGO ---------- */

static void initMobs(void) {
    memset(mobs, 0, sizeof(mobs));

    mobs[0]=(Mob){270, GROUND-25, 0, 0, 2, 1, 0, 0};
    mobs[1]=(Mob){410, GROUND-25, 0, 0, 2, 1, 0, 1};
    mobs[2]=(Mob){510, GROUND-25, 0, 0, 2, 1, 0, 0};
    mobs[3]=(Mob){690, GROUND-25, 0, 0, 2, 1, 0, 1};
}

static void resetGame(void) {
    px=55;
    py=GROUND-PH;
    vx=0;
    vy=0;
    cameraX=0;

    facing=1;
    grounded=1;
    jumps=2;

    hp=maxHP;
    xp=0;
    level=1;

    magicUnlocked=0;
    crystalTaken=0;
    tutorialComplete=0;

    attackTimer=0;
    magicTimer=0;
    invincible=0;
    animationFrame=0;
    frameCounter=0;

    bossHP=bossMaxHP;
    bossAlive=1;
    bossAttackTimer=0;
    bossAnimation=0;
    bossHitFlash=0;

    minionsDefeated=0;

    initMobs();

    dialogPage=0;
    mode=MODE_DIALOG;
}

static void damageBoss(int damage) {
    if (!bossAlive) return;

    bossHP-=damage;
    bossHitFlash=6;

    if (bossHP<=0) {
        bossHP=0;
        bossAlive=0;
        xp+=50;
        mode=MODE_WIN;
    }
}

static void updateMobs(void) {
    for (int i=0; i<4; i++) {
        Mob *m=&mobs[i];

        if (!m->alive) continue;

        m->frame++;

        float dx=px-m->x;

        if (fabsf(dx)<180 && fabsf(dx)>22) {
            m->x += dx>0 ? 0.65f : -0.65f;
        }

        if (m->x<180) m->x=180;
        if (m->x>780) m->x=780;

        Box player={px,py,PW,PH};
        Box enemy={m->x,m->y,22,25};

        if (hit(player,enemy) && invincible==0) {
            hp--;
            invincible=55;

            if (hp<=0) {
                hp=maxHP;
                px=55;
                py=GROUND-PH;
                vx=vy=0;
                cameraX=0;
            }
        }

        /* Ataque de espada contra minion */
        if (attackTimer==7 &&
            fabsf(px-m->x)<45 &&
            fabsf(py-m->y)<38) {
            m->hp--;

            if (m->hp<=0) {
                m->alive=0;
                minionsDefeated++;
                xp+=5;
            }
        }

        /* Magia tambem pode derrotar minions */
        if (magicTimer==12 &&
            fabsf(px-m->x)<150) {
            m->hp-=2;

            if (m->hp<=0) {
                m->alive=0;
                minionsDefeated++;
                xp+=5;
            }
        }
    }
}

static void updateGame(SceCtrlData p, unsigned int old) {
    int left=(p.Buttons&PSP_CTRL_LEFT)||(p.Lx<80);
    int right=(p.Buttons&PSP_CTRL_RIGHT)||(p.Lx>175);

    vx=0;

    if (left) {
        vx=-2.0f;
        facing=0;
    }

    if (right) {
        vx=2.0f;
        facing=1;
    }

    px+=vx;

    if (px<0) px=0;
    if (px>WORLD_END-PW) px=WORLD_END-PW;

    if (pressed(p.Buttons,old,PSP_CTRL_CROSS) && jumps>0) {
        vy=-6.7f;
        jumps--;
        grounded=0;
    }

    vy+=0.34f;
    py+=vy;

    if (py+PH>=GROUND) {
        py=GROUND-PH;
        vy=0;
        grounded=1;
        jumps=2;
    }

    if (py<0) {
        py=0;
        if (vy<0) vy=0;
    }

    if (attackTimer>0) attackTimer--;
    if (magicTimer>0) magicTimer--;
    if (invincible>0) invincible--;
    if (bossHitFlash>0) bossHitFlash--;

    frameCounter++;
    if (frameCounter>=8) {
        frameCounter=0;
        animationFrame++;
    }

    /* Cristal */
    if (!crystalTaken && px>610 && px<660 &&
        py<GROUND-5) {
        crystalTaken=1;
        magicUnlocked=1;
        mode=MODE_DIALOG;
        dialogPage=1;
    }

    /* Area 1: minions */
    if (mode==MODE_TUTORIAL)
        updateMobs();

    /* Transicao para area 2 */
    if (px>=860 && !tutorialComplete) {
        tutorialComplete=1;
        px=930;
        py=GROUND-PH;
        cameraX=px-SW/2;
        mode=MODE_DIALOG;
        dialogPage=2;
    }

    /* Arena do chefe */
    if (px>=900 && mode==MODE_BOSS && bossAlive) {
        bossAttackTimer++;
        bossAnimation++;

        if (bossAttackTimer>=85) {
            bossAttackTimer=0;

            if (fabsf(px-1270)<100 && invincible==0) {
                hp--;
                invincible=55;

                if (hp<=0) {
                    hp=maxHP;
                    px=930;
                    py=GROUND-PH;
                }
            }
        }

        /* Espada causa dano perto do chefe */
        if (attackTimer==7 && fabsf(px-1270)<85)
            damageBoss(2);

        /* Magia causa dano a distancia */
        if (magicTimer==12 && fabsf(px-1270)<260)
            damageBoss(3);
    }

    cameraX=px-SW/2;

    if (cameraX<0) cameraX=0;
    if (cameraX>WORLD_END-SW)
        cameraX=WORLD_END-SW;
}

/* ---------- MAIN ---------- */

int main(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(GU_PSM_8888, (void*)0x00000, 512);
    sceGuDispBuffer(SW, SH, (void*)0x88000, 512);
    sceGuDepthBuffer((void*)0x110000, 512);

    sceGuOffset(2048-SW/2, 2048-SH/2);
    sceGuViewport(2048, 2048, SW, SH);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SW, SH);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData p;
    unsigned int old=0;

    while (1) {
        sceCtrlReadBufferPositive(&p, 1);

        if (mode==MODE_MENU) {
            if (pressed(p.Buttons,old,PSP_CTRL_UP) ||
                pressed(p.Buttons,old,PSP_CTRL_DOWN))
                menuSelection=(menuSelection+1)%2;

            if (pressed(p.Buttons,old,PSP_CTRL_CROSS))
                resetGame();
        }
        else if (mode==MODE_DIALOG) {
            if (pressed(p.Buttons,old,PSP_CTRL_CROSS) ||
                pressed(p.Buttons,old,PSP_CTRL_CIRCLE)) {
                if (dialogPage==0) {
                    mode=MODE_TUTORIAL;
                } else if (dialogPage==1) {
                    mode=MODE_TUTORIAL;
                } else {
                    mode=MODE_BOSS;
                }
            }
        }
        else if (mode==MODE_PAUSE) {
            if (pressed(p.Buttons,old,PSP_CTRL_START))
                mode=previousMode;
        }
        else if (mode==MODE_WIN || mode==MODE_GAMEOVER) {
            if (pressed(p.Buttons,old,PSP_CTRL_START))
                mode=MODE_MENU;
        }
        else {
            if (pressed(p.Buttons,old,PSP_CTRL_START)) {
                previousMode=mode;
                mode=MODE_PAUSE;
            } else {
                if (pressed(p.Buttons,old,PSP_CTRL_SQUARE))
                    attackTimer=12;

                if (pressed(p.Buttons,old,PSP_CTRL_CIRCLE) &&
                    magicUnlocked && magicTimer==0)
                    magicTimer=18;

                updateGame(p,old);
            }
        }

        sceGuStart(GU_DIRECT, displayList);
        sceGuClearColor(C_BLACK);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        if (mode==MODE_MENU) {
            drawMenu();
        } else if (mode==MODE_PAUSE) {
            if (previousMode==MODE_BOSS)
                drawArena();
            else
                drawTutorial();

            drawPause();
        } else if (mode==MODE_WIN) {
            drawArena();
            drawWin();
        } else if (mode==MODE_DIALOG) {
            if (tutorialComplete)
                drawArena();
            else
                drawTutorial();

            drawDialog();
        } else if (mode==MODE_BOSS) {
            drawArena();
            drawHUD();
        } else {
            drawTutorial();
            drawHUD();
        }

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        old=p.Buttons;
        sceKernelDelayThread(16667);
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
