
/*
 * RANDOM - ATORES ADAPTADOS AO SISTEMA DE SALAS
 * PSP: 480x272
 * Jogador, mobs e miniboss desenhados em C.
 * Sem PNG, GIF ou sprites externos.
 *
 * Depende das variaveis do jogo:
 *   px, py, facing, cameraX
 *
 * Depende da funcao:
 *   wr(x, y, largura, altura, cor)
 *
 * IMPORTANTE:
 *   px e py representam a posicao do jogador no mundo.
 *   Esta versao nao substitui o movimento principal
 *   nem o sistema de colisoes do jogo.
 */

#include <math.h>

/* Cores locais */
#define RA_BLACK  0xFF080808
#define RA_DARK   0xFF202020
#define RA_STONE  0xFF454545
#define RA_GRAY   0xFF777777
#define RA_LIGHT  0xFFBBBBBB
#define RA_WHITE  0xFFFFFFFF

#define RA_ROOM_W 800
#define RA_BOSS_ROOM 4

/* Estas variaveis ja existem no codigo principal. */
extern float px;
extern float py;
extern float cameraX;
extern int facing;

/*
 * Se alguma destas variaveis tiver outro tipo
 * no teu codigo principal, ajusta a declaracao.
 */

/* Relogio visual das animacoes */
static int raAnimTick = 0;

/* =========================
   DESENHO NO MUNDO
   ========================= */

static void raRect(
    float worldX,
    float y,
    float w,
    float h,
    unsigned int color
)
{
    float sx;

    sx = worldX - cameraX;

    if (sx + w <= 0 || sx >= 480)
        return;

    wr(sx, y, w, h, color);
}

/* =========================
   JOGADOR ANIMADO
   ========================= */

/*
 * Desenha o jogador na posicao px/py.
 * Animacoes: parado, caminhada, salto e ataque.
 *
 * action:
 *   0 = parado
 *   1 = caminhada
 *   2 = salto/queda
 *   3 = ataque
 *
 * O jogo principal deve escolher a acao.
 */

static void raDrawPlayer(int action)
{
    float x;
    float legMove;
    float armMove;
    int step;

    x = px;
    step = (raAnimTick / 5) % 2;

    legMove = 0;
    armMove = 0;

    if (action == 1)
    {
        legMove = step ? 5 : -5;
        armMove = step ? -3 : 3;
    }
    else if (action == 2)
    {
        legMove = -2;
        armMove = -4;
    }
    else if (action == 3)
    {
        armMove = 7;
    }

    /* Cabeca */
    raRect(x + 8, py, 12, 12, RA_WHITE);

    /* Rosto */
    raRect(x + 10, py + 4, 2, 2, RA_BLACK);

    if (facing >= 0)
        raRect(x + 16, py + 4, 2, 2, RA_BLACK);
    else
        raRect(x + 8, py + 4, 2, 2, RA_BLACK);

    /* Corpo */
    raRect(x + 6, py + 12, 16, 17, RA_LIGHT);
    raRect(x + 9, py + 15, 10, 10, RA_GRAY);

    /* Bracos */
    raRect(x + 2, py + 14 + armMove, 5, 12, RA_WHITE);
    raRect(x + 21, py + 14 - armMove, 5, 12, RA_LIGHT);

    /* Pernas */
    raRect(x + 7, py + 28, 6, 12 + legMove, RA_LIGHT);
    raRect(x + 15, py + 28, 6, 12 - legMove, RA_WHITE);

    /* Botas */
    raRect(x + 5, py + 38 + legMove, 8, 3, RA_GRAY);
    raRect(x + 15, py + 38 - legMove, 8, 3, RA_GRAY);

    /* Lamina visual durante o ataque */
    if (action == 3)
    {
        if (facing >= 0)
        {
            raRect(x + 25, py + 13, 13, 3, RA_WHITE);
            raRect(x + 34, py + 9, 3, 10, RA_LIGHT);
        }
        else
        {
            raRect(x - 10, py + 13, 13, 3, RA_WHITE);
            raRect(x - 10, py + 9, 3, 10, RA_LIGHT);
        }
    }
}

/* =========================
   MOBS
   ========================= */

typedef struct
{
    float x;
    float y;
    float startX;
    float endX;
    float speed;
    int alive;
    int direction;
    int anim;
    int hp;
} RandomMob;

#define RA_MOB_COUNT 8

static RandomMob raMobs[RA_MOB_COUNT] =
{
    {  280, 220, 220, 520, 0.7f, 1,  1, 0, 3 },
    {  650, 220, 560, 760, 0.9f, 1, -1, 0, 3 },
    { 1050, 220, 900, 1350, 0.8f, 1,  1, 0, 3 },
    { 1450, 220, 1300, 1550, 1.0f, 1, -1, 0, 3 },
    { 1810, 220, 1650, 1950, 0.8f, 1,  1, 0, 3 },
    { 2200, 220, 2050, 2380, 1.1f, 1, -1, 0, 3 },
    { 2700, 220, 2500, 2950, 0.9f, 1,  1, 0, 3 },
    { 3100, 220, 3000, 3150, 0.7f, 1, -1, 0, 3 }
};

/* Movimento simples de patrulha */
static void raUpdateMobs(void)
{
    int i;
    RandomMob *m;

    for (i = 0; i < RA_MOB_COUNT; i++)
    {
        m = &raMobs[i];

        if (!m->alive)
            continue;

        m->x += m->speed * m->direction;

        if (m->x <= m->startX)
        {
            m->x = m->startX;
            m->direction = 1;
        }

        if (m->x >= m->endX)
        {
            m->x = m->endX;
            m->direction = -1;
        }

        m->anim++;
    }
}

/* Desenho de um mob com animacao */
static void raDrawMob(RandomMob *m)
{
    int bounce;

    if (!m->alive)
        return;

    bounce = (m->anim / 6) % 2;

    /* Corpo */
    raRect(m->x, m->y + bounce, 25, 23, RA_STONE);
    raRect(m->x + 4, m->y - 4 + bounce, 17, 7, RA_GRAY);

    /* Olhos */
    raRect(m->x + 5, m->y + 5 + bounce, 4, 4, RA_WHITE);
    raRect(m->x + 16, m->y + 5 + bounce, 4, 4, RA_WHITE);
    raRect(m->x + 6, m->y + 6 + bounce, 2, 2, RA_BLACK);
    raRect(m->x + 17, m->y + 6 + bounce, 2, 2, RA_BLACK);

    /* Pes */
    raRect(m->x + 2, m->y + 21 + bounce, 8, 5, RA_DARK);
    raRect(m->x + 15, m->y + 21 - bounce, 8, 5, RA_DARK);
}

static void raDrawMobs(void)
{
    int i;

    for (i = 0; i < RA_MOB_COUNT; i++)
        raDrawMob(&raMobs[i]);
}

/* =========================
   MINIBOSS - SALA 4
   ========================= */

typedef struct
{
    float x;
    float y;
    int hp;
    int maxHp;
    int alive;
    int direction;
    int anim;
    int attackTimer;
} RandomMiniBoss;

static RandomMiniBoss raBoss =
{
    3500, 195,
    30, 30,
    1, -1,
    0, 0
};

/*
 * O miniboss fica na sala 4:
 * X = 3200 ate 3999.
 *
 * A movimentacao e limitada a arena.
 */

static void raUpdateBoss(void)
{
    if (!raBoss.alive)
        return;

    raBoss.anim++;
    raBoss.attackTimer++;

    raBoss.x += 0.45f * raBoss.direction;

    if (raBoss.x < 3300)
    {
        raBoss.x = 3300;
        raBoss.direction = 1;
    }

    if (raBoss.x > 3850)
    {
        raBoss.x = 3850;
        raBoss.direction = -1;
    }

    if (raBoss.attackTimer > 180)
        raBoss.attackTimer = 0;
}

static void raDrawBoss(void)
{
    int bob;
    int barWidth;

    if (!raBoss.alive)
        return;

    bob = (raBoss.anim / 12) % 2;

    /* Corpo grande */
    raRect(raBoss.x, raBoss.y + bob, 64, 43, RA_DARK);
    raRect(raBoss.x + 7, raBoss.y - 10 + bob, 50, 15, RA_STONE);

    /* Chifres */
    raRect(raBoss.x + 7, raBoss.y - 18 + bob, 8, 12, RA_LIGHT);
    raRect(raBoss.x + 48, raBoss.y - 18 + bob, 8, 12, RA_LIGHT);

    /* Olhos */
    raRect(raBoss.x + 13, raBoss.y + 5 + bob, 10, 7, RA_WHITE);
    raRect(raBoss.x + 40, raBoss.y + 5 + bob, 10, 7, RA_WHITE);
    raRect(raBoss.x + 17, raBoss.y + 7 + bob, 4, 4, RA_BLACK);
    raRect(raBoss.x + 43, raBoss.y + 7 + bob, 4, 4, RA_BLACK);

    /* Bracos */
    raRect(raBoss.x - 10, raBoss.y + 9 + bob, 12, 27, RA_GRAY);
    raRect(raBoss.x + 62, raBoss.y + 9 + bob, 12, 27, RA_GRAY);

    /* Pernas */
    raRect(raBoss.x + 8, raBoss.y + 39 + bob, 17, 12, RA_STONE);
    raRect(raBoss.x + 39, raBoss.y + 39 - bob, 17, 12, RA_STONE);

    /* Barra de vida */
    raRect(raBoss.x, raBoss.y - 29, 64, 5, RA_DARK);

    barWidth = (raBoss.hp * 60) / raBoss.maxHp;

    if (barWidth > 0)
        raRect(raBoss.x + 2, raBoss.y - 28,
               (float)barWidth, 3, RA_WHITE);
}

/*
 * Chamar esta funcao quando o ataque do jogador
 * for ativado no codigo principal.
 *
 * Usa uma verificacao simples de alcance.
 */
static void raAttackTargets(void)
{
    int i;
    float distance;

    for (i = 0; i < RA_MOB_COUNT; i++)
    {
        if (!raMobs[i].alive)
            continue;

        distance = raMobs[i].x - px;

        if (distance < 0)
            distance = -distance;

        if (distance < 55 &&
            raMobs[i].y + 26 >= py &&
            raMobs[i].y <= py + 43)
        {
            raMobs[i].hp--;

            if (raMobs[i].hp <= 0)
                raMobs[i].alive = 0;
        }
    }

    if (raBoss.alive)
    {
        distance = raBoss.x - px;

        if (distance < 0)
            distance = -distance;

        if (distance < 85 &&
            raBoss.y + 50 >= py &&
            raBoss.y <= py + 43)
        {
            raBoss.hp--;

            if (raBoss.hp <= 0)
                raBoss.alive = 0;
        }
    }
}

/* =========================
   FUNCOES DE INTEGRACAO
   ========================= */

/* Chamar uma vez por frame durante o jogo */
static void randomActorsUpdate(void)
{
    raAnimTick++;

    raUpdateMobs();
    raUpdateBoss();
}

/*
 * Desenhar depois do cenario.
 *
 * playerAction:
 *   0 parado
 *   1 andando
 *   2 no ar
 *   3 atacando
 */
static void randomActorsDraw(int playerAction)
{
    raDrawMobs();
    raDrawBoss();
    raDrawPlayer(playerAction);
}
