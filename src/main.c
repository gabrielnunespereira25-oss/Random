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
 *
 * Nota: codigo-fonte de beta, ainda precisa ser compilado e testado
 * no PSPSDK/PPSSPP.
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
#define RED    0xFF3535D5

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

/* -------------------------------------------------- */
/* DESENHO                                             */
/* -------------------------------------------------- */

static void tri(float x1, float y1, float x2, float y2,
                float x3, float y3, unsigned int color)
{
    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 3);
    v[0].color = color; v[0].x = x1; v[0].y = y1; v[0].z = 0;
    v[1].color = color; v[1].x = x2; v[1].y = y2; v[1].z = 0;
    v[2].color = color; v[2].x = x3; v[2].y = y3; v[2].z = 0;
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
        tri(x, y, x + cosf(a)*r, y + sinf(a)*r,
            x + cosf(b)*r, y + sinf(b)*r, color);
    }
}

static void line(float x1, float y1, float x2, float y2,
                 float width, unsigned int color)
{
    float dx = x2-x1, dy = y2-y1;
    float len = sqrtf(dx*dx + dy*dy);
    float nx, ny;
    if (len < 0.001f) return;
    nx = -dy / len * width * 0.5f;
    ny =  dx / len * width * 0.5f;
    tri(x1+nx,y1+ny,x1-nx,y1-ny,x2+nx,y2+ny,color);
    tri(x1-nx,y1-ny,x2-nx,y2-ny,x2+nx,y2+ny,color);
}

/* -------------------------------------------------- */
/* MAPA E COLISOES                                     */
/* Actor.y representa a linha dos pes.                 */
/* Caixa do Actor: x +/- 8 e y-38 ate y+2.             */
/* -------------------------------------------------- */

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

/* Colisao por eixo. Plataformas so seguram o ator ao cair sobre elas. */
static void collideActor(Actor *a, float dt)
{
    int i;
    float oldY, nextY;

    a->x += a->vx * dt;

    for (i = 0; i < wallCount; i++) {
        Wall *w = &walls[i];
        if (a->y + 2 <= w->y || a->y - 38 >= w->y + w->h)
            continue;

        if (a->x + 8 > w->x && a->x - 8 < w->x + w->w) {
            if (a->vx > 0) a->x = w->x - 8;
            else if (a->vx < 0) a->x = w->x + w->w + 8;
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

        if (a->x + 8 <= w->x || a->x - 8 >= w->x + w->w)
            continue;

        if (a->vy >= 0 && oldY + 2 <= w->y &&
            nextY + 2 >= w->y) {
            a->y = w->y - 2;
            a->vy = 0;
            a->grounded = 1;
        } else if (a->vy < 0 && oldY - 38 >= w->y + w->h &&
                   nextY - 38 <= w->y + w->h) {
            a->y = w->y + w->h + 38;
            a->vy = 0;
        }
    }

    if (a->x < 8) a->x = 8;
    if (a->x > WORLD_W - 8) a->x = WORLD_W - 8;
}

/* -------------------------------------------------- */
/* INICIALIZACAO                                       */
/* -------------------------------------------------- */

static void initWorld(void)
{
    int i;
    wallCount = 0;

    /* Zona 1: cavernas */
    addWall(180,176,150,16);
    addWall(390,145,130,16);
    addWall(620,180,170,16);
    addWall(850,155,140,16);
    addWall(1100,125,170,16);
    addWall(1370,172,160,16);
    addWall(1620,142,190,16);
    addWall(1900,178,140,16);
    addWall(2180,150,140,16);
    addWall(550,178,24,46);
    addWall(1020,170,24,54);
    addWall(1760,174,24,50);

    /* Zona 2: floresta esquecida */
    addWall(2500,174,180,16);
    addWall(2780,145,150,16);
    addWall(3060,120,180,16);
    addWall(3350,164,180,16);
    addWall(3640,132,180,16);
    addWall(3930,174,150,16);
    addWall(4180,145,160,16);

    /* Zona 3: fortaleza do Golem */
    addWall(4490,170,160,16);

    for (i = 0; i < ZOMBIE_MAX; i++) {
        zombies[i].x = 260.0f + i*360.0f;
        zombies[i].y = FLOOR_Y - 2;
        zombies[i].vx = zombies[i].vy = 0;
        zombies[i].hp = 50;
        zombies[i].anim = 0;
        zombies[i].attack = 0;
        zombies[i].facing = -1;
        zombies[i].grounded = 1;
        zombies[i].alive = 1;
        zombies[i].crippled = 0;
        zombies[i].hurt = 0;
    }

    /* Aranhas de grama na floresta */
    for (i = 0; i < SPIDER_MAX; i++) {
        spiders[i].x = 2500.0f + i*300.0f;
        spiders[i].y = FLOOR_Y - 2;
        spiders[i].vx = spiders[i].vy = 0;
        spiders[i].anim = 0;
        spiders[i].hp = 65;
        spiders[i].hurt = 0;
        spiders[i].facing = -1;
        spiders[i].grounded = 1;
        spiders[i].alive = 1;
        spiders[i].state = 0;
        spiders[i].jumpCooldown = 1.0f + i*0.25f;
    }

    memset(shots, 0, sizeof(shots));

    player.x = 90;
    player.y = FLOOR_Y - 2;
    player.vx = player.vy = 0;
    player.anim = player.attack = 0;
    player.hp = 100;
    player.facing = 1;
    player.grounded = 1;
    player.alive = 1;
    player.crippled = 0;
    player.hurt = 0;

    boss.x = bossSpawnX;
    boss.y = FLOOR_Y - 2;
    boss.vx = boss.vy = 0;
    boss.anim = 0;
    boss.hp = 500;
    boss.facing = -1;
    boss.grounded = 1;
    boss.alive = 1;
    boss.state = 0;
    boss.timer = 2.0f;
    boss.hurt = 0;

    cameraX = 0;
    worldTime = 0;
    magicCooldown = 0;
    swordHitDone = 0;
    gameOver = victory = 0;
}

/* -------------------------------------------------- */
/* PLAYER                                              */
/* -------------------------------------------------- */

static void drawPlayer(void)
{
    float x = player.x-cameraX;
    float y = player.y;
    float walk = sinf(player.anim*10.0f)*4.0f;
    float bob = player.grounded && fabsf(player.vx)>5
              ? fabsf(sinf(player.anim*10.0f))*1.5f : 0;
    float flame = sinf(worldTime*9.0f)*2.0f;

    y += bob;
    circle(x,y+1,10,BLACK);

    /* Corpo triangular, mantendo o design do player */
    tri(x-9,y-31,x+8,y-31,x-11,y-6,BLACK);
    tri(x+8,y-31,x+6,y-13,x-11,y-6,GRAY);
    tri(x-7,y-29,x+6,y-29,x,y-13,BLACK);
    tri(x-3,y-27,x+3,y-23,x-1,y-19,WHITE);

    line(x-7,y-27,x-13,y-17+walk*.3f,5,BLACK);
    line(x+7,y-27,x+12,y-18-walk*.3f,5,GRAY);

    /* Uma perna; a outra permanece ausente */
    line(x+2,y-13,x+5+walk,y-3,6,BLACK);
    line(x+5+walk,y-3,x+12+walk,y-2,4,GRAY);

    circle(x,y-38,8,BLACK);
    rect(x+player.facing*2,y-39,3,2,WHITE);

    /* Cabelo feito de circulos de fogo */
    circle(x-5+flame,y-45,4,WHITE);
    circle(x,y-49-flame,5,WHITE);
    circle(x+5-flame,y-45,4,WHITE);
    circle(x-1,y-43,3,GRAY);
    circle(x+8+flame,y-48,2,WHITE);

    /* Arco da espada */
    if (player.attack > 0) {
        int i;
        float progress = 1.0f-player.attack/0.48f;
        float radius = 16.0f+progress*20.0f;
        float start = -1.35f, end = 1.35f;
        float px = x+player.facing*cosf(start)*radius;
        float py = y-24+sinf(start)*radius;

        for (i=1; i<=12; i++) {
            float angle = start+(end-start)*i/12.0f;
            float nx = x+player.facing*cosf(angle)*radius;
            float ny = y-24+sinf(angle)*radius;
            line(px,py,nx,ny,3,WHITE);
            px=nx; py=ny;
        }
    }
}

/* -------------------------------------------------- */
/* MAGIA                                               */
/* -------------------------------------------------- */

static void castMagic(void)
{
    int i;
    if (magicCooldown > 0) return;

    for (i=0; i<MAGIC_MAX; i++) {
        if (!shots[i].active) {
            shots[i].x = player.x+player.facing*15;
            shots[i].y = player.y-25;
            shots[i].vx = player.facing*290;
            shots[i].vy = 0;
            shots[i].life = 1.5f;
            shots[i].active = 1;
            magicCooldown = 0.4f;
            break;
        }
    }
}

static void updateMagic(float dt)
{
    int i,j;
    if (magicCooldown > 0) magicCooldown -= dt;
    if (magicCooldown < 0) magicCooldown = 0;

    for (i=0; i<MAGIC_MAX; i++) {
        Magic *m=&shots[i];
        if (!m->active) continue;

        m->x += m->vx*dt;
        m->life -= dt;

        if (m->life<=0 || m->x<0 || m->x>WORLD_W) {
            m->active=0;
            continue;
        }

        for (j=0; j<ZOMBIE_MAX; j++) {
            Actor *z=&zombies[j];
            if (z->alive && fabsf(m->x-z->x)<17 &&
                fabsf(m->y-(z->y-20))<25) {
                z->hp-=25;
                z->hurt=0.15f;
                m->active=0;
                if (z->hp<=25) z->crippled=1;
                if (z->hp<=0) z->alive=0;
                break;
            }
        }

        for (j=0; j<SPIDER_MAX && m->active; j++) {
            GrassSpider *s=&spiders[j];
            if (s->alive && fabsf(m->x-s->x)<20 &&
                fabsf(m->y-(s->y-12))<22) {
                s->hp-=25;
                s->hurt=0.15f;
                m->active=0;
                if (s->hp<=0) s->alive=0;
            }
        }

        if (boss.alive && m->active &&
            fabsf(m->x-boss.x)<35 && fabsf(m->y-(boss.y-25))<35) {
            boss.hp-=20;
            boss.hurt=0.12f;
            m->active=0;
            if (boss.hp<=0) {
                boss.hp=0; boss.alive=0; victory=1;
            }
        }
    }
}

static void drawMagic(void)
{
    int i;
    for (i=0; i<MAGIC_MAX; i++) {
        Magic *m=&shots[i];
        float x,y;
        if (!m->active) continue;
        x=m->x-cameraX; y=m->y;
        line(x-m->vx*0.04f,y,x,y,4,WHITE);
        circle(x,y,5,WHITE);
        circle(x,y,2,BLACK);
    }
}

/* -------------------------------------------------- */
/* ZUMBIS                                              */
/* -------------------------------------------------- */

static void updateZombies(float dt)
{
    int i;
    for (i=0; i<ZOMBIE_MAX; i++) {
        Actor *z=&zombies[i];
        if (!z->alive) continue;
        if (z->hurt>0) z->hurt-=dt;
        z->anim+=dt;

        if (fabsf(player.x-z->x)<420) {
            z->facing=player.x>z->x?1:-1;
            z->vx=z->facing*(z->crippled?24:52);
            if (fabsf(player.x-z->x)<23 &&
                fabsf(player.y-z->y)<30) {
                player.hp-=10*dt;
                if (player.hp<0) player.hp=0;
            }
        } else z->vx=0;

        z->vy+=850*dt;
        collideActor(z,dt);
    }
}

static void drawZombie(Actor *z)
{
    float x=z->x-cameraX, y=z->y;
    float w=z->crippled?8:12;
    float step=sinf(z->anim*7)*4;
    if (!z->alive) return;

    tri(x-w,y-29,x+w,y-27,x+2,y-8,BLACK);
    circle(x,y-34,7,z->hurt>0?WHITE:GRAY);
    line(x-7,y-25,x-14,y-15+step,5,BLACK);
    line(x+7,y-25,x+13,y-17-step,5,GRAY);

    if (z->crippled) {
        line(x,y-10,x+10+step,y-2,5,BLACK);
        line(x-3,y-13,x-13,y-8,4,GRAY);
    } else {
        line(x-4,y-12,x-7+step,y-1,5,BLACK);
        line(x+4,y-12,x+7-step,y-1,5,GRAY);
    }
    rect(x+1,y-35,4,2,WHITE);
}

/* -------------------------------------------------- */
/* ARANHA DE GRAMA                                     */
/* -------------------------------------------------- */

static void updateSpiders(float dt)
{
    int i;
    for (i=0; i<SPIDER_MAX; i++) {
        GrassSpider *s=&spiders[i];
        float dist;

        if (!s->alive) continue;
        if (s->hurt>0) s->hurt-=dt;
        s->anim+=dt;
        s->jumpCooldown-=dt;

        dist=fabsf(player.x-s->x);
        s->facing=(player.x>s->x)?1:-1;

        if (dist<360) {
            if (s->jumpCooldown<=0 && s->grounded && dist>35) {
                s->vy=-260;
                s->vx=s->facing*105;
                s->grounded=0;
                s->jumpCooldown=1.35f;
                s->state=1;
            } else if (s->grounded) {
                s->vx=s->facing*38;
                s->state=0;
            }
        } else {
            s->vx=0;
            s->state=0;
        }

        s->vy+=760*dt;

        /* Adaptador de colisao: a aranha usa uma caixa menor,
           mas a rotina geral mantem o mesmo ponto dos pes. */
        {
            Actor a;
            a.x=s->x; a.y=s->y;
            a.vx=s->vx; a.vy=s->vy;
            a.grounded=s->grounded;
            collideActor(&a,dt);
            s->x=a.x; s->y=a.y;
            s->vx=a.vx; s->vy=a.vy;
            s->grounded=a.grounded;
        }

        if (dist<24 && fabsf(player.y-s->y)<32) {
            player.hp-=16*dt;
            if (player.hp<0) player.hp=0;
        }
    }
}

static void drawSpider(GrassSpider *s)
{
    float x=s->x-cameraX, y=s->y;
    float step=sinf(s->anim*12.0f)*4.0f;
    int i;

    if (!s->alive) return;

    /* Oito patas articuladas */
    for (i=0; i<4; i++) {
        float yy=y-9.0f+i*2.0f;
        line(x-5,yy,x-12-i*2,yy-5+step,3,BLACK);
        line(x-12-i*2,yy-5+step,x-15-i*2,yy+2,2,GRAY);
        line(x+5,yy,x+12+i*2,yy-5-step,3,BLACK);
        line(x+12+i*2,yy-5-step,x+15+i*2,yy+2,2,GRAY);
    }

    /* Corpo coberto por grama */
    circle(x,y-13,9,s->hurt>0?WHITE:GREEN);
    circle(x-4,y-19,4,LGREEN);
    circle(x+3,y-21,4,GREEN);
    tri(x-7,y-18,x-2,y-27,x+1,y-17,LGREEN);
    tri(x+1,y-19,x+6,y-28,x+8,y-17,LGREEN);

    /* Olhos */
    circle(x+s->facing*4,y-14,1.8f,WHITE);
    circle(x+s->facing*6,y-14,1.0f,BLACK);
}

/* -------------------------------------------------- */
/* GOLEM                                               */
/* -------------------------------------------------- */

static void updateBoss(float dt)
{
    float distance;
    if (!boss.alive || player.x<BOSS_START-500) return;

    boss.anim+=dt;
    boss.timer-=dt;
    if (boss.hurt>0) boss.hurt-=dt;
    distance=fabsf(player.x-boss.x);

    if (boss.state==0) {
        boss.vx=0;
        if (boss.timer<=0) {
            boss.state=1; boss.timer=0.7f;
            boss.facing=player.x>boss.x?1:-1;
        }
    } else if (boss.state==1) {
        boss.vx=0;
        if (boss.timer<=0) {
            boss.state=2;
            boss.vx=boss.facing*270;
            boss.timer=0.65f;
        }
    } else if (boss.state==2) {
        if (boss.timer<=0) {
            boss.state=3;
            boss.vy=-330;
            boss.timer=0.8f;
        }
    } else if (boss.state==3) {
        if (boss.grounded && boss.timer<0.6f) {
            boss.state=0; boss.timer=1.5f;
        }
    }

    if (boss.state!=2) boss.vx*=0.90f;
    boss.vy+=850*dt;
    boss.x+=boss.vx*dt;
    boss.y+=boss.vy*dt;
    boss.grounded=0;

    if (boss.vy>=0 && boss.y>=FLOOR_Y-2) {
        boss.y=FLOOR_Y-2;
        boss.vy=0;
        boss.grounded=1;
    }

    if (boss.x<ZONE_W) boss.x=ZONE_W;
    if (boss.x>WORLD_W-40) boss.x=WORLD_W-40;

    if (distance<48 && boss.state==2) {
        player.hp-=35*dt;
        if (player.hp<0) player.hp=0;
    }

    if (player.hp<=0) gameOver=1;
}

static void drawBoss(void)
{
    float x=boss.x-cameraX, y=boss.y;
    float bounce=sinf(boss.anim*8)*2;
    if (!boss.alive) return;

    rect(x-24,y-48+bounce,48,40,boss.hurt>0?WHITE:BLACK);
    rect(x-31,y-43+bounce,14,28,GRAY);
    rect(x+17,y-43+bounce,14,28,GRAY);
    rect(x-20,y-8,13,8,BLACK);
    rect(x+8,y-8,13,8,BLACK);
    circle(x-10,y-35+bounce,3,WHITE);
    circle(x+10,y-35+bounce,3,WHITE);
    line(x-5,y-28,x,y-20,2,GRAY);
    line(x+6,y-24,x+2,y-16,2,GRAY);

    rect(x-28,y-60,56,4,BLACK);
    rect(x-27,y-59,54*(boss.hp/500.0f),2,WHITE);
}

/* -------------------------------------------------- */
/* CENARIO                                             */
/* -------------------------------------------------- */

static void drawBackground(void)
{
    int i;
    rect(0,0,SW,SH,BLACK);

    for (i=0; i<8; i++) {
        float x=i*90-fmodf(cameraX*0.15f,90);
        tri(x-35,190,x+35,75+(i%3)*12,x+105,190,DARK);
    }

    /* Silhuetas verdes quando a camera esta na floresta */
    if (cameraX+SW>FOREST_START && cameraX<BOSS_START) {
        for (i=0; i<6; i++) {
            float tx=i*100-fmodf(cameraX*0.25f,100);
            tri(tx+10,190,tx+42,55+(i%3)*15,tx+75,190,GREEN);
            rect(tx+38,150,7,45,BLACK);
        }
    }

    circle(400,45,18,WHITE);
    circle(408,38,16,BLACK);
}

static void drawWorld(void)
{
    int i;
    rect(0,FLOOR_Y,SW,SH-FLOOR_Y,DARK);
    rect(0,FLOOR_Y,SW,4,GRAY);

    for (i=0; i<wallCount; i++) {
        float x=walls[i].x-cameraX, y=walls[i].y;
        if (x<-walls[i].w || x>SW) continue;
        rect(x,y,walls[i].w,walls[i].h,DARK);
        rect(x,y,walls[i].w,3,GRAY);
        line(x+4,y+5,x+walls[i].w*0.4f,y+walls[i].h-3,2,BLACK);
    }

    /* Portal da floresta e entrada da fortaleza */
    {
        float x=ZONE_W-cameraX;
        if (x>-30 && x<SW+30) {
            rect(x-15,FLOOR_Y-58,30,58,BLACK);
            rect(x-10,FLOOR_Y-53,20,53,PURPLE);
            rect(x-5,FLOOR_Y-48,10,48,WHITE);
        }
    }

    if (cameraX<FOREST_START)
        rect(12,12,4,4,WHITE);
    else if (cameraX<BOSS_START)
        rect(12,12,4,4,GREEN);
    else
        rect(12,12,4,4,PURPLE);
}

/* -------------------------------------------------- */
/* ATUALIZACAO                                         */
/* -------------------------------------------------- */

static void updatePlayer(float dt, SceCtrlData pad,
                         unsigned int pressed)
{
    float move=0, speed=145;

    if (pad.Buttons&PSP_CTRL_LEFT) move-=1;
    if (pad.Buttons&PSP_CTRL_RIGHT) move+=1;
    if (pad.Lx<105) move-=(105-pad.Lx)/105.0f;
    if (pad.Lx>150) move+=(pad.Lx-150)/105.0f;
    if (move<-1) move=-1;
    if (move>1) move=1;

    if (pad.Buttons&PSP_CTRL_RTRIGGER) speed=220;
    player.vx=move*speed;

    if (move>0.1f) player.facing=1;
    if (move<-0.1f) player.facing=-1;

    if ((pressed&PSP_CTRL_CROSS) && player.grounded) {
        player.vy=-340;
        player.grounded=0;
    }

    if ((pressed&PSP_CTRL_SQUARE) && player.attack<=0) {
        player.attack=0.48f;
        swordHitDone=0;
    }

    if (pressed&PSP_CTRL_TRIANGLE) castMagic();

    if (player.attack>0) {
        player.attack-=dt;
        if (player.attack<0) player.attack=0;
    }

    player.vy+=850*dt;
    collideActor(&player,dt);
    player.anim+=dt;

    if (player.x-cameraX>SW*0.65f)
        cameraX=player.x-SW*0.65f;
    if (cameraX<0) cameraX=0;
    if (cameraX>WORLD_W-SW) cameraX=WORLD_W-SW;
}

static void swordHit(void)
{
    int i;
    if (player.attack<=0.12f || player.attack>=0.25f || swordHitDone)
        return;

    swordHitDone=1;

    for (i=0; i<ZOMBIE_MAX; i++) {
        Actor *z=&zombies[i];
        if (!z->alive) continue;

        if (fabsf(z->x-player.x)<52 &&
            fabsf(z->y-player.y)<45 &&
            (z->x-player.x)*player.facing>-10) {
            z->hp-=30;
            z->hurt=0.12f;
            if (z->hp<=20) z->crippled=1;
            if (z->hp<=0) z->alive=0;
        }
    }

    for (i=0; i<SPIDER_MAX; i++) {
        GrassSpider *s=&spiders[i];
        if (!s->alive) continue;
        if (fabsf(s->x-player.x)<52 &&
            fabsf(s->y-player.y)<45 &&
            (s->x-player.x)*player.facing>-10) {
            s->hp-=30;
            s->hurt=0.12f;
            if (s->hp<=0) s->alive=0;
        }
    }

    if (boss.alive && fabsf(boss.x-player.x)<60 &&
        fabsf(boss.y-player.y)<60) {
        boss.hp-=15;
        boss.hurt=0.12f;
        if (boss.hp<=0) {
            boss.hp=0; boss.alive=0; victory=1;
        }
    }
}

static void drawHUD(void)
{
    int i;
    float hp=player.hp;
    if (hp<0) hp=0;
    if (hp>100) hp=100;

    rect(12,12,104,7,BLACK);
    rect(14,14,100*(hp/100.0f),3,WHITE);

    for (i=0; i<3; i++)
        rect(14+i*8,25,5,3,magicCooldown<=0?WHITE:GRAY);

    if (boss.alive && player.x>BOSS_START-500) {
        float bhp=boss.hp;
        if (bhp<0) bhp=0;
        if (bhp>500) bhp=500;
        rect(150,12,180,8,BLACK);
        rect(152,14,176*(bhp/500.0f),4,WHITE);
    }

    if (gameOver) {
        rect(150,115,180,35,BLACK);
        rect(170,130,140,3,WHITE);
    }
    if (victory) {
        rect(150,115,180,35,WHITE);
        rect(170,130,140,3,BLACK);
    }
}

/* -------------------------------------------------- */
/* GRAFICOS                                            */
/* -------------------------------------------------- */

static void graphicsInit(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT,list);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,STRIDE);
    sceGuDispBuffer(SW,SH,(void*)0x88000,STRIDE);
    sceGuDepthBuffer((void*)0x110000,STRIDE);
    sceGuOffset(2048-SW/2,2048-SH/2);
    sceGuViewport(2048,2048,SW,SH);
    sceGuScissor(0,0,SW,SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* -------------------------------------------------- */
/* MAIN                                                */
/* -------------------------------------------------- */

int main(void)
{
    SceCtrlData pad;
    unsigned int last=0, now, pressed;
    const float dt=1.0f/60.0f;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initWorld();
    graphicsInit();

    while (running) {
        sceCtrlReadBufferPositive(&pad,1);
        now=pad.Buttons;
        pressed=now&~last;

        if (pressed&PSP_CTRL_START) running=0;

        if (!gameOver && !victory) {
            updatePlayer(dt,pad,pressed);
            updateZombies(dt);
            updateSpiders(dt);
            updateMagic(dt);
            updateBoss(dt);
            swordHit();

            if (player.hp<=0) {
                player.hp=0;
                gameOver=1;
            }
        }

        worldTime+=dt;

        sceGuStart(GU_DIRECT,list);
        sceGuClearColor(BLACK);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        drawBackground();
        drawWorld();

        {
            int i;
            for (i=0; i<ZOMBIE_MAX; i++)
                drawZombie(&zombies[i]);
            for (i=0; i<SPIDER_MAX; i++)
                drawSpider(&spiders[i]);
        }

        drawBoss();
        drawPlayer();
        drawMagic();
        drawHUD();

        sceGuFinish();
        sceGuSync(0,0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        last=now;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
