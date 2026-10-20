
/*
 * RANDOM - ESSENCIA ESCURA
 * BETA 0.2 | PSP / PSPSDK
 *
 * ZONA 1: CAVERNAS DA ESSENCIA
 * ZONA 2: FLORESTA ESQUECIDA (ARANHA DE GRAMA)
 * ZONA 3: FORTALEZA DO GOLEM
 *
 * ANALOGICO / DIRECIONAL: MOVER
 * X: PULAR
 * QUADRADO: ESPADA
 * TRIANGULO: MAGIA
 * R: CORRER
 * START: SAIR
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspge.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512
#define PI 3.14159265f
#define FLOOR_Y 224.0f
#define ZONE_W 2400.0f
#define FOREST_START 2400.0f
#define BOSS_START 4250.0f
#define WORLD_W 4800.0f

#define BLACK  0xFF08080C
#define DARK   0xFF191923
#define GRAY   0xFF777783
#define WHITE  0xFFF4F4FF
#define PURPLE 0xFF713B91
#define GREEN  0xFF4C8B45
#define LGREEN 0xFF91C95A

#define WALL_MAX 100
#define ZOMBIE_MAX 10
#define SPIDER_MAX 6
#define MAGIC_MAX 10

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, w, h;
} Wall;

typedef struct {
    float x, y, vx, vy;
    float anim, attack, hp;
    int facing, grounded, alive, crippled;
    float hurt;
} Actor;

typedef struct {
    float x, y, vx, vy, life;
    int active;
} Magic;

typedef struct {
    float x, y, vx, vy, anim, hp;
    int facing, grounded, alive, state;
    float timer, hurt;
} Golem;

typedef struct {
    float x, y, vx, vy, anim, hp, hurt;
    int facing, grounded, alive, state;
    float jumpCooldown;
} GrassSpider;

static unsigned int __attribute__((aligned(16))) list[262144];

static Actor player;
static Actor zombies[ZOMBIE_MAX];
static GrassSpider spiders[SPIDER_MAX];
static Magic shots[MAGIC_MAX];
static Golem boss;
static Wall walls[WALL_MAX];

static int wallCount = 0;
static int running = 1;
static int gameOver = 0;
static int victory = 0;
static int swordHitDone = 0;

static float cameraX = 0.0f;
static float worldTime = 0.0f;
static float magicCooldown = 0.0f;
static float bossSpawnX = BOSS_START;

/* DESENHO */

static void tri(float x1, float y1, float x2, float y2,
                float x3, float y3, unsigned int color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 3);
    v[0].color = color;
    v[0].x = x1; v[0].y = y1; v[0].z = 0;
    v[1].color = color;
    v[1].x = x2; v[1].y = y2; v[1].z = 0;
    v[2].color = color;
    v[2].x = x3; v[2].y = y3; v[2].z = 0;

    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        3, 0, v);
}

static void rect(float x, float y, float w, float h,
                 unsigned int color)
{
    tri(x, y, x+w, y, x, y+h, color);
    tri(x+w, y, x+w, y+h, x, y+h, color);
}

static void circle(float x, float y, float r, unsigned int color)
{
    int i;
    const int n = 16;

    for (i = 0; i < n; i++) {
        float a = 2.0f * PI * i / n;
        float b = 2.0f * PI * (i+1) / n;

        tri(x, y,
            x + cosf(a)*r, y + sinf(a)*r,
            x + cosf(b)*r, y + sinf(b)*r,
            color);
    }
}

static void line(float x1, float y1, float x2, float y2,
                 float width, unsigned int color)
{
    float dx = x2-x1;
    float dy = y2-y1;
    float len = sqrtf(dx*dx + dy*dy);
    float nx, ny;

    if (len < 0.001f) return;

    nx = -dy / len * width * 0.5f;
    ny =  dx / len * width * 0.5f;

    tri(x1+nx,y1+ny,x1-nx,y1-ny,x2+nx,y2+ny,color);
    tri(x1-nx,y1-ny,x2-nx,y2-ny,x2+nx,y2+ny,color);
}

/* MAPA E COLISOES
   Actor.y representa a linha dos pes. */

static void addWall(float x, float y, float w, float h)
{
    if (wallCount >= WALL_MAX) return;

    walls[wallCount].x = x;
    walls[wallCount].y = y;
    walls[wallCount].w = w;
    walls[wallCount].h = h;
    wallCount++;
}

static int overlaps(float x, float y, float w, float h, Wall *a)
{
    return x < a->x+a->w && x+w > a->x &&
           y < a->y+a->h && y+h > a->y;
}

static void collideActor(Actor *a, float dt)
{
    int i;
    float oldY, nextY;

    a->x += a->vx * dt;

    for (i = 0; i < wallCount; i++) {
        Wall *w = &walls[i];

        if (a->y + 2 <= w->y ||
            a->y - 38 >= w->y + w->h)
            continue;

        if (a->x + 8 > w->x &&
            a->x - 8 < w->x + w->w) {
            if (a->vx > 0)
                a->x = w->x - 8;
            else if (a->vx < 0)
                a->x = w->x + w->w + 8;
        }
    }

    oldY = a->y;
    nextY = oldY + a->vy * dt;
    a->y = nextY;
    a->grounded = 0;

    if (a->vy >= 0 && a->y + 2 >= FLOOR_Y) {
        a->y = FLOOR_Y - 2;
        a->vy = 0;
        a->grounded = 1;
    }

    for (i = 0; i < wallCount; i++) {
        Wall *w = &walls[i];

        if (a->x + 8 <= w->x ||
            a->x - 8 >= w->x + w->w)
            continue;

        if (a->vy >= 0 && oldY + 2 <= w->y &&
            nextY + 2 >= w->y) {
            a->y = w->y - 2;
            a->vy = 0;
            a->grounded = 1;
        } else if (a->vy < 0 &&
                   oldY - 38 >= w->y + w->h &&
                   nextY - 38 <= w->y + w->h) {
            a->y = w->y + w->h + 38;
            a->vy = 0;
        }
    }

    if (a->x < 8) a->x = 8;
    if (a->x > WORLD_W - 8) a->x = WORLD_W - 8;
}
/* ============================
   PARTE 2: PERSONAGENS E INIMIGOS
   ============================ */

/* JOGADOR: manter o desenho original.
   Corpo triangular, cabelo de fogo em circulos
   e uma perna ausente. */

static void drawPlayer(void)
{
    float x = player.x - cameraX;
    float y = player.y;
    float legX = x + (player.facing > 0 ? 3 : -3);

    /* Corpo triangular */
    tri(x, y-32, x-10, y-9, x+10, y-9, PURPLE);

    /* Cabeca */
    circle(x, y-37, 6, 0xFF29202F);

    /* Cabelo de fogo */
    circle(x-5, y-43, 4, 0xFFB83232);
    circle(x,   y-46, 5, 0xFFFF682F);
    circle(x+5, y-42, 4, 0xFFFFA52F);
    circle(x+1, y-40, 3, 0xFFFFD45A);

    /* Um olho */
    rect(x+(player.facing*2), y-38, 2, 2, WHITE);

    /* Uma unica perna */
    line(x, y-10, legX, y-2, 3, 0xFF25252D);

    /* Espada durante o ataque */
    if (player.attack > 0) {
        float sx = x + player.facing*10;
        float sy = y-22;
        line(sx, sy, sx+player.facing*18, sy-7, 3, GRAY);
        line(sx+player.facing*18, sy-7,
             sx+player.facing*21, sy-8, 2, WHITE);
    }
}

/* ZUMBIS */

static void drawZombie(Actor *z)
{
    float x = z->x-cameraX;
    float y = z->y;

    if (!z->alive) return;

    rect(x-7, y-26, 14, 17, 0xFF426B43);
    circle(x, y-31, 6, 0xFF71885A);

    rect(x-4, y-33, 2, 2, WHITE);
    rect(x+2, y-33, 2, 2, WHITE);

    line(x-4, y-9, x-7, y-2, 3, 0xFF353D35);
    line(x+4, y-9, x+5, y-2, 3, 0xFF353D35);

    line(x-6, y-23, x-12, y-16, 3, 0xFF426B43);
    line(x+6, y-23, x+12, y-16, 3, 0xFF426B43);

    if (z->hurt > 0)
        rect(x-8, y-36, 16, 2, WHITE);
}

/* ARANHA DE GRAMA */

static void drawSpider(GrassSpider *s)
{
    float x = s->x-cameraX;
    float y = s->y;
    int i;
    float wave;

    if (!s->alive) return;

    wave = sinf(s->anim*8.0f)*3.0f;

    /* Oito patas animadas */
    for (i = 0; i < 4; i++) {
        float side = (i < 2) ? -1.0f : 1.0f;
        float row = (i % 2 == 0) ? -1.0f : 1.0f;
        float legY = y-9+(float)(i%2)*3;

        line(x+side*4, legY,
             x+side*(10+i*2), legY-5+wave*row,
             2, 0xFF355B2D);

        line(x+side*(10+i*2), legY-5+wave*row,
             x+side*(13+i*2), legY+2+wave*row,
             2, 0xFF4C8B45);
    }

    /* Corpo coberto de grama */
    circle(x, y-11, 8, 0xFF315C2D);
    circle(x-4, y-14, 4, GREEN);
    circle(x+3, y-15, 5, 0xFF39753A);
    circle(x, y-19, 3, LGREEN);

    /* Olhos */
    circle(x-3, y-12, 1.5f, WHITE);
    circle(x+3, y-12, 1.5f, WHITE);

    if (s->hurt > 0)
        circle(x, y-13, 9, WHITE);
}

/* GOLEM */

static void drawBoss(void)
{
    float x = boss.x-cameraX;
    float y = boss.y;

    if (!boss.alive) return;

    /* Corpo rochoso */
    rect(x-18, y-40, 36, 32, 0xFF655E68);
    rect(x-13, y-47, 26, 10, 0xFF817985);

    /* Brilho dos olhos */
    rect(x-9, y-42, 5, 4, 0xFFFF4D4D);
    rect(x+4, y-42, 5, 4, 0xFFFF4D4D);

    /* Bracos */
    line(x-16, y-34, x-25, y-17, 7, 0xFF514B55);
    line(x+16, y-34, x+25, y-17, 7, 0xFF514B55);

    /* Pernas */
    rect(x-15, y-9, 11, 7, 0xFF45414A);
    rect(x+4, y-9, 11, 7, 0xFF45414A);

    /* Barra de vida */
    rect(x-22, y-57, 44, 4, 0xFF442222);
    if (boss.hp > 0) {
        float hpWidth = 44.0f * boss.hp / 100.0f;
        if (hpWidth < 0) hpWidth = 0;
        if (hpWidth > 44) hpWidth = 44;
        rect(x-22, y-57, hpWidth, 4, 0xFFE53E4E);
    }
}

/* INICIALIZACAO DO MAPA */

static void initWorld(void)
{
    int i;

    wallCount = 0;

    /* Plataformas das cavernas */
    addWall(180, 176, 100, 10);
    addWall(390, 150, 110, 10);
    addWall(650, 184, 130, 10);
    addWall(900, 160, 100, 10);
    addWall(1150, 178, 130, 10);
    addWall(1450, 145, 120, 10);
    addWall(1750, 177, 140, 10);
    addWall(2050, 155, 120, 10);

    /* Plataformas da floresta */
    addWall(2520, 174, 120, 10);
    addWall(2740, 146, 100, 10);
    addWall(2980, 180, 130, 10);
    addWall(3220, 155, 110, 10);
    addWall(3470, 176, 130, 10);
    addWall(3700, 145, 110, 10);

    /* Fortaleza */
    addWall(4000, 177, 100, 10);
    addWall(4210, 151, 120, 10);
    addWall(4490, 178, 120, 10);

    /* Zumbis */
    for (i = 0; i < ZOMBIE_MAX; i++) {
        zombies[i].x = 350.0f + i*190.0f;
        zombies[i].y = FLOOR_Y-2;
        zombies[i].vx = 0;
        zombies[i].vy = 0;
        zombies[i].anim = 0;
        zombies[i].attack = 0;
        zombies[i].hp = 3;
        zombies[i].facing = -1;
        zombies[i].grounded = 1;
        zombies[i].alive = 1;
        zombies[i].crippled = 0;
        zombies[i].hurt = 0;
    }

    /* Aranhas de grama na floresta */
    for (i = 0; i < SPIDER_MAX; i++) {
        spiders[i].x = 2520.0f + i*300.0f;
        spiders[i].y = FLOOR_Y-2;
        spiders[i].vx = 0;
        spiders[i].vy = 0;
        spiders[i].anim = 0;
        spiders[i].hp = 3;
        spiders[i].hurt = 0;
        spiders[i].facing = -1;
        spiders[i].grounded = 1;
        spiders[i].alive = 1;
        spiders[i].state = 0;
        spiders[i].jumpCooldown = 1.0f+(float)i*0.3f;
    }

    /* Golem da fortaleza */
    boss.x = bossSpawnX;
    boss.y = FLOOR_Y-2;
    boss.vx = 0;
    boss.vy = 0;
    boss.anim = 0;
    boss.hp = 100;
    boss.facing = -1;
    boss.grounded = 1;
    boss.alive = 1;
    boss.state = 0;
    boss.timer = 0;
    boss.hurt = 0;
}

/* PROJETEIS DE MAGIA */

static void spawnMagic(void)
{
    int i;

    if (magicCooldown > 0) return;

    for (i = 0; i < MAGIC_MAX; i++) {
        if (!shots[i].active) {
            shots[i].x = player.x + player.facing*12;
            shots[i].y = player.y-22;
            shots[i].vx = player.facing*260.0f;
            shots[i].vy = -20.0f;
            shots[i].life = 1.5f;
            shots[i].active = 1;
            magicCooldown = 0.35f;
            return;
        }
    }
}

/* ATAQUE DA ESPADA: cada inimigo recebe no maximo
   um golpe por ataque. */

static int swordHits(float x, float y, float radius)
{
    float sx = player.x + player.facing*20;
    float sy = player.y-22;
    float dx = x-sx;
    float dy = y-sy;

    if (player.attack <= 0 || swordHitDone)
        return 0;

    return dx*dx+dy*dy <= radius*radius;
}

/* ARANHA PERSEGUE O JOGADOR E SALTA */

static void updateSpider(GrassSpider *s, float dt)
{
    float dx;
    Actor a;

    if (!s->alive) return;

    s->anim += dt;
    if (s->hurt > 0) s->hurt -= dt;
    if (s->jumpCooldown > 0) s->jumpCooldown -= dt;

    dx = player.x-s->x;
    s->facing = (dx >= 0) ? 1 : -1;

    if (fabsf(dx) < 260.0f) {
        s->vx = s->facing*65.0f;

        if (s->grounded && s->jumpCooldown <= 0 &&
            fabsf(dx) < 190.0f) {
            s->vy = -185.0f;
            s->grounded = 0;
            s->jumpCooldown = 1.7f;
        }
    } else {
        s->vx = 0;
    }

    s->vy += 420.0f*dt;
    if (s->vy > 330.0f) s->vy = 330.0f;

    /* Usa a colisao de Actor para manter a aranha
       dentro do mundo e sobre o chao. */
    a.x = s->x;
    a.y = s->y;
    a.vx = s->vx;
    a.vy = s->vy;
    a.grounded = s->grounded;

    collideActor(&a, dt);

    s->x = a.x;
    s->y = a.y;
    s->vx = a.vx;
    s->vy = a.vy;
    s->grounded = a.grounded;

    if (fabsf(dx) < 18.0f && player.hurt <= 0) {
        player.hurt = 0.65f;
        player.hp -= 1;
    }

    if (swordHits(s->x, s->y-12, 15.0f)) {
        s->hp -= 1;
        s->hurt = 0.12f;
        swordHitDone = 1;
        if (s->hp <= 0) s->alive = 0;
    }
}

/* FIM DA PARTE 2 */
/* ============================
   PARTE 3: LOGICA E LOOP PRINCIPAL
   ============================ */

static void updatePlayer(float dt, SceCtrlData *pad)
{
    float move = 0;
    unsigned int buttons = pad->Buttons;

    if (pad->Lx < 110) move = -1.0f;
    if (pad->Lx > 145) move = 1.0f;

    if (buttons & PSP_CTRL_LEFT) move = -1.0f;
    if (buttons & PSP_CTRL_RIGHT) move = 1.0f;

    if (move != 0) {
        player.facing = (move > 0) ? 1 : -1;
        player.vx = move * ((buttons & PSP_CTRL_RTRIGGER) ? 220.0f : 150.0f);
    } else {
        player.vx = 0;
    }

    if ((buttons & PSP_CTRL_CROSS) && player.grounded) {
        player.vy = -245.0f;
        player.grounded = 0;
    }

    if ((buttons & PSP_CTRL_SQUARE) && player.attack <= 0) {
        player.attack = 0.22f;
        swordHitDone = 0;
    }

    if (buttons & PSP_CTRL_TRIANGLE)
        spawnMagic();

    if (player.attack > 0) {
        player.attack -= dt;
        if (player.attack < 0) player.attack = 0;
    }

    if (magicCooldown > 0) {
        magicCooldown -= dt;
        if (magicCooldown < 0) magicCooldown = 0;
    }

    if (player.hurt > 0) {
        player.hurt -= dt;
        if (player.hurt < 0) player.hurt = 0;
    }

    player.vy += 520.0f * dt;
    if (player.vy > 400.0f) player.vy = 400.0f;

    collideActor(&player, dt);

    if (player.hp <= 0) {
        player.hp = 0;
        gameOver = 1;
    }
}

static void updateZombies(float dt)
{
    int i;

    for (i = 0; i < ZOMBIE_MAX; i++) {
        Actor *z = &zombies[i];
        float dx;

        if (!z->alive) continue;

        z->anim += dt;
        if (z->hurt > 0) z->hurt -= dt;

        dx = player.x-z->x;

        if (fabsf(dx) < 210.0f) {
            z->facing = dx >= 0 ? 1 : -1;
            z->vx = z->facing*42.0f;
        } else {
            z->vx = 0;
        }

        z->vy += 420.0f*dt;
        if (z->vy > 330.0f) z->vy = 330.0f;

        collideActor(z, dt);

        if (fabsf(player.x-z->x) < 17.0f &&
            player.hurt <= 0) {
            player.hp -= 1;
            player.hurt = 0.7f;
        }

        if (swordHits(z->x, z->y-15, 18.0f)) {
            z->hp -= 1;
            z->hurt = 0.15f;
            swordHitDone = 1;
            if (z->hp <= 0) z->alive = 0;
        }
    }
}

static void updateMagic(float dt)
{
    int i, j;

    for (i = 0; i < MAGIC_MAX; i++) {
        Magic *m = &shots[i];

        if (!m->active) continue;

        m->x += m->vx*dt;
        m->y += m->vy*dt;
        m->life -= dt;

        if (m->life <= 0 ||
            m->x < 0 || m->x > WORLD_W) {
            m->active = 0;
            continue;
        }

        for (j = 0; j < ZOMBIE_MAX; j++) {
            Actor *z = &zombies[j];

            if (!z->alive) continue;

            if (fabsf(m->x-z->x) < 17 &&
                fabsf(m->y-(z->y-18)) < 20) {
                z->hp -= 2;
                z->hurt = 0.15f;
                m->active = 0;

                if (z->hp <= 0) z->alive = 0;
                break;
            }
        }

        if (!m->active) continue;

        for (j = 0; j < SPIDER_MAX; j++) {
            GrassSpider *s = &spiders[j];

            if (!s->alive) continue;

            if (fabsf(m->x-s->x) < 18 &&
                fabsf(m->y-(s->y-12)) < 18) {
                s->hp -= 2;
                s->hurt = 0.15f;
                m->active = 0;

                if (s->hp <= 0) s->alive = 0;
                break;
            }
        }

        if (!m->active) continue;

        if (boss.alive &&
            fabsf(m->x-boss.x) < 25 &&
            fabsf(m->y-(boss.y-25)) < 30) {
            boss.hp -= 4;
            boss.hurt = 0.15f;
            m->active = 0;

            if (boss.hp <= 0) {
                boss.hp = 0;
                boss.alive = 0;
                victory = 1;
            }
        }
    }
}

static void updateBoss(float dt)
{
    float dx;

    if (!boss.alive || player.x < BOSS_START-180)
        return;

    boss.timer += dt;
    if (boss.hurt > 0) boss.hurt -= dt;

    dx = player.x-boss.x;
    boss.facing = dx >= 0 ? 1 : -1;

    if (fabsf(dx) > 30) {
        boss.vx = boss.facing*35.0f;
    } else {
        boss.vx = 0;
    }

    if (boss.grounded && boss.timer > 2.0f &&
        fabsf(dx) < 160.0f) {
        boss.vy = -155.0f;
        boss.grounded = 0;
        boss.timer = 0;
    }

    boss.x += boss.vx*dt;
    boss.vy += 420.0f*dt;
    if (boss.vy > 330.0f) boss.vy = 330.0f;
    boss.y += boss.vy*dt;

    if (boss.y+2 >= FLOOR_Y) {
        boss.y = FLOOR_Y-2;
        boss.vy = 0;
        boss.grounded = 1;
    }

    if (fabsf(dx) < 30.0f && player.hurt <= 0) {
        player.hp -= 2;
        player.hurt = 0.8f;
    }

    if (swordHits(boss.x, boss.y-25, 32.0f)) {
        boss.hp -= 2;
        boss.hurt = 0.15f;
        swordHitDone = 1;

        if (boss.hp <= 0) {
            boss.hp = 0;
            boss.alive = 0;
            victory = 1;
        }
    }
}

/* CENARIO */

static void drawBackground(void)
{
    int i;

    rect(0, 0, SW, SH, 0xFF10101E);

    /* Lua / luz distante */
    circle(405, 48, 18, 0xFFAAA3C8);
    circle(412, 43, 17, 0xFF10101E);

    /* Silhuetas ao fundo */
    if (player.x >= FOREST_START &&
        player.x < BOSS_START) {
        for (i = 0; i < 8; i++) {
            float tx = (float)i*80.0f -
                       fmodf(cameraX*0.25f, 80.0f);

            tri(tx, 190, tx+35, 70, tx+70, 190,
                0xFF182B25);
            rect(tx+30, 150, 10, 74, 0xFF182B25);
        }
    } else {
        for (i = 0; i < 7; i++) {
            float tx = (float)i*90.0f -
                       fmodf(cameraX*0.2f, 90.0f);

            tri(tx, 190, tx+45, 95, tx+90, 190,
                0xFF242034);
        }
    }

    /* Chao */
    rect(0, FLOOR_Y, SW, SH-FLOOR_Y, 0xFF292631);
    rect(0, FLOOR_Y, SW, 3, 0xFF57515F);

    /* Plataformas */
    for (i = 0; i < wallCount; i++) {
        float x = walls[i].x-cameraX;

        if (x > -walls[i].w && x < SW) {
            rect(x, walls[i].y, walls[i].w,
                 walls[i].h, 0xFF3D3948);
            rect(x, walls[i].y, walls[i].w,
                 2, 0xFF777080);
        }
    }
}

static void drawMagic(void)
{
    int i;

    for (i = 0; i < MAGIC_MAX; i++) {
        Magic *m = &shots[i];

        if (!m->active) continue;

        circle(m->x-cameraX, m->y, 6, 0xFF713B91);
        circle(m->x-cameraX, m->y, 3, 0xFFDAB4FF);
    }
}

static void drawHUD(void)
{
    int i;

    /* Vida do jogador */
    rect(10, 10, 104, 10, 0xFF3D2028);
    for (i = 0; i < (int)player.hp; i++) {
        rect(12+i*10, 12, 8, 6, 0xFFE84A55);
    }

    /* Nome da zona representado por barras */
    if (player.x < FOREST_START) {
        rect(190, 10, 100, 4, 0xFF55506B);
    } else if (player.x < BOSS_START) {
        rect(190, 10, 100, 4, 0xFF4C8B45);
    } else {
        rect(190, 10, 100, 4, 0xFF817985);
    }
}

static void drawGame(void)
{
    int i;

    sceGuStart(GU_DIRECT, list);

    drawBackground();

    for (i = 0; i < ZOMBIE_MAX; i++)
        drawZombie(&zombies[i]);

    for (i = 0; i < SPIDER_MAX; i++)
        drawSpider(&spiders[i]);

    drawMagic();
    drawBoss();

    /* Piscar durante invulnerabilidade */
    if (player.hurt <= 0 ||
        ((int)(worldTime*12.0f)%2 == 0)) {
        drawPlayer();
    }

    drawHUD();

    if (gameOver) {
        rect(100, 100, 280, 45, 0xFF521D2A);
        rect(120, 120, 240, 4, WHITE);
    } else if (victory) {
        rect(100, 100, 280, 45, 0xFF244B35);
        rect(120, 120, 240, 4, LGREEN);
    }

    sceGuFinish();
    sceGuSync(0, 0);
}

/* INICIALIZACAO DO PSP */

static void initGraphics(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888,
        (void*)0, STRIDE);
    sceGuDispBuffer(SW, SH,
        (void*)0x88000, STRIDE);
    sceGuDepthBuffer((void*)0x110000, STRIDE);

    sceGuOffset(2048-(SW/2), 2048-(SH/2));
    sceGuViewport(2048, 2048, SW, SH);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SW, SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuShadeModel(GU_FLAT);
    sceGuFinish();
    sceGuSync(0, 0);

    sceGuDisplay(GU_TRUE);
}

/* LOOP PRINCIPAL */

int main(void)
{
    SceCtrlData pad;
    unsigned int oldButtons = 0;
    float dt = 1.0f/60.0f;
    int i;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initGraphics();
    initWorld();

    memset(&player, 0, sizeof(player));
    player.x = 80;
    player.y = FLOOR_Y-2;
    player.hp = 10;
    player.facing = 1;
    player.grounded = 1;
    player.alive = 1;

    for (i = 0; i < MAGIC_MAX; i++)
        shots[i].active = 0;

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);

        if ((pad.Buttons & PSP_CTRL_START) &&
            !(oldButtons & PSP_CTRL_START)) {
            running = 0;
        }

        worldTime += dt;

        if (!gameOver && !victory) {
            updatePlayer(dt, &pad);
            updateZombies(dt);

            for (i = 0; i < SPIDER_MAX; i++)
                updateSpider(&spiders[i], dt);

            updateMagic(dt);
            updateBoss(dt);
        }

        cameraX = player.x-(SW/2);

        if (cameraX < 0) cameraX = 0;
        if (cameraX > WORLD_W-SW)
            cameraX = WORLD_W-SW;

        drawGame();

        sceDisplayWaitVblankStart();
        oldButtons = pad.Buttons;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
