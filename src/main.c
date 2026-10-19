
/*
 * RANDOM - ESSENCIA ESCURA
 * PSP / PSPSDK
 *
 * Zona 1: Cavernas da Essencia
 * Zona 2: Fortaleza do Golem
 *
 * Controles:
 * Analógico / direcional: mover
 * X: pular
 * Quadrado: espada
 * Triângulo: magia
 * R: correr
 * START: sair
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

#define ZONE_W 2400.0f
#define WORLD_W (ZONE_W * 2.0f)
#define FLOOR_Y 224.0f

#define BLACK  0xFF08080C
#define DARK   0xFF191923
#define GRAY   0xFF777783
#define WHITE  0xFFF4F4FF
#define PURPLE 0xFF713B91
#define RED    0xFF3535D5

#define WALL_MAX 100
#define ZOMBIE_MAX 12
#define MAGIC_MAX 10

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, w, h;
    int solid;
} Wall;

typedef struct {
    float x, y, vx, vy;
    float anim, attack, hp;
    int facing, grounded, alive;
    int crippled;
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

static unsigned int __attribute__((aligned(16)))
list[262144];

static Player_unused_placeholder; /* removido abaixo */
