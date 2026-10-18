
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512

#define WORLD_W 2400.0f
#define PW 18.0f
#define PH 30.0f

#define PLAYER_MAX_HP 4
#define BOSS_MAX_HP 10

#define SHOT_COUNT 6
#define SPARK_COUNT 32
#define ENEMY_COUNT 6

#define BOSS_START 1920.0f
#define BOSS_END 2400.0f

#define BLUE       0xFFFF8040
#define LIGHT_BLUE 0xFFFFD080
#define WHITE      0xFFFFFFFF
#define SILVER     0xFFBFC5C9
#define DARK       0xFF17121D
#define ROCK       0xFF403443
#define ROCK_LIGHT 0xFF62516A
#define RED        0xFF3030D0
#define PURPLE     0xFFB060B0
#define GREEN      0xFF50C060
#define GOLD       0xFF40C0FF

typedef struct {
    float x, y, vx, vy;
    int active;
    int life;
} Shot;

typedef struct {
    float x, y, vx, vy;
    int active;
    int life;
    unsigned int color;
} Spark;

typedef struct {
    float x, y, vx, vy;
    int hp;
    int active;
    int grounded;
    int attackTimer;
    int damageCooldown;
} MiniBoss;

typedef struct {
    float x, y, vx, vy;
    int hp;
    int facing;
    int grounded;
    int jumps;
    int attackTimer;
    int damageCooldown;
    int magicUnlocked;
    int magicCooldown;
    int invulnerable;
} Player;

static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned int __attribute__((aligned(16))) framebuffer[512 * 272];

static Player player;
static MiniBoss boss;
static Shot shots[SHOT_COUNT];
static Spark sparks[SPARK_COUNT];

static float cameraX = 0.0f;
static int gameState = 0;
static int bossStarted = 0;
static int bossDefeated = 0;
static int frameCount = 0;
static int swordHitDone = 0;
static int bossHitFlash = 0;

static unsigned int oldButtons = 0;

static unsigned int pressed(unsigned int buttons, unsigned int key)
{
    return (buttons & key) && !(oldButtons & key);
}

static void rect(float x, float y, float w, float h, unsigned int color)
{
    if (w <= 0 || h <= 0) return;

    struct Vertex {
        unsigned short u, v;
        unsigned short color;
        float x, y, z;
    };

    struct Vertex *v = (struct Vertex *)sceGuGetMemory(sizeof(struct Vertex) * 2);

    v[0].u = v[0].v = 0;
    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].u = v[1].v = 0;
    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);
    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF,
        2,
        0,
        v
    );
}

static void circle(float x, float y, float radius, unsigned int color)
{
    for (int i = 0; i < 12; i++) {
        float a = (float)i * 6.2831853f / 12.0f;
        float px = x + cosf(a) * radius;
        float py = y + sinf(a) * radius;
        rect(px - 2, py - 2, 4, 4, color);
    }
}

static void spawnSparks(float x, float y, unsigned int color, int amount)
{
    for (int i = 0; i < amount; i++) {
        for (int j = 0; j < SPARK_COUNT; j++) {
            if (!sparks[j].active) {
                float angle = (float)(rand() % 628) / 100.0f;
                float speed = 0.7f + (float)(rand() % 30) / 10.0f;

                sparks[j].x = x;
                sparks[j].y = y;
                sparks[j].vx = cosf(angle) * speed;
                sparks[j].vy = sinf(angle) * speed - 0.8f;
                sparks[j].life = 14 + rand() % 15;
                sparks[j].active = 1;
                sparks[j].color = color;
                break;
            }
        }
    }
}

static void updateSparks(void)
{
    for (int i = 0; i < SPARK_COUNT; i++) {
        if (!sparks[i].active) continue;

        sparks[i].x += sparks[i].vx;
        sparks[i].y += sparks[i].vy;
        sparks[i].vy += 0.12f;
        sparks[i].life--;

        if (sparks[i].life <= 0)
            sparks[i].active = 0;
    }
}

static void drawSparks(void)
{
    for (int i = 0; i < SPARK_COUNT; i++) {
        if (!sparks[i].active) continue;

        float size = sparks[i].life > 8 ? 3.0f : 2.0f;

        rect(
            sparks[i].x - cameraX,
            sparks[i].y,
            size,
            size,
            sparks[i].color
        );
    }
}

static void initGame(void)
{
    memset(&player, 0, sizeof(player));
    memset(&boss, 0, sizeof(boss));
    memset(shots, 0, sizeof(shots));
    memset(sparks, 0, sizeof(sparks));

    player.x = 70;
    player.y = 170;
    player.hp = PLAYER_MAX_HP;
    player.facing = 1;

    boss.x = BOSS_START + 270;
    boss.y = 190;
    boss.hp = BOSS_MAX_HP;
    boss.active = 1;

    cameraX = 0;
    bossStarted = 0;
    bossDefeated = 0;
    frameCount = 0;
    swordHitDone = 0;
    bossHitFlash = 0;
}

static float floorAt(float x)
{
    if (x < 1600.0f) return 232.0f;
    if (x < BOSS_START) return 232.0f;
    return 232.0f;
}

static void drawCaveRoom(float start, float end, int arena)
{
    unsigned int wall = arena ? 0xFF30273A : ROCK;
    unsigned int trim = arena ? 0xFF51405C : ROCK_LIGHT;

    rect(start - cameraX, 0, end - start, SH, DARK);

    for (int i = 0; i < 12; i++) {
        float rx = start + i * ((end - start) / 12.0f);
        float rw = (end - start) / 12.0f;

        rect(rx - cameraX, 0, rw, 12 + (i % 3) * 6, wall);

        float ceilingY = 18 + (i % 4) * 5;
        rect(rx - cameraX, ceilingY, rw * 0.55f, 3, trim);

        if (i % 2 == 0)
            rect(rx - cameraX + 8, 70 + (i % 3) * 12,
                 20, 4, wall);
    }

    rect(start - cameraX, 232, end - start, 40, wall);
    rect(start - cameraX, 232, end - start, 4, trim);

    for (int i = 0; i < 18; i++) {
        float rx = start + i * ((end - start) / 18.0f);
        rect(rx - cameraX, 244 + (i % 3) * 7,
             16, 3, DARK);
    }

    if (arena) {
        rect(start - cameraX + 10, 35, 4, 190, PURPLE);
        rect(end - cameraX - 14, 35, 4, 190, PURPLE);

        for (int i = 0; i < 5; i++) {
            rect(start - cameraX + 10, 45 + i * 35,
                 4, 12, LIGHT_BLUE);
            rect(end - cameraX - 14, 45 + i * 35,
                 4, 12, LIGHT_BLUE);
        }
    }
}

static void drawWorld(void)
{
    rect(0, 0, SW, SH, DARK);

    drawCaveRoom(0, 1600, 0);

    /* Corredor que liga a caverna à arena do miniboss. */
    rect(1600 - cameraX, 0, 320, SH, 0xFF211C2A);
    rect(1600 - cameraX, 232, 320, 40, ROCK);
    rect(1600 - cameraX, 232, 320, 4, ROCK_LIGHT);

    for (int i = 0; i < 8; i++) {
        float x = 1620 + i * 38;
        rect(x - cameraX, 40 + (i % 3) * 12,
             18, 4, ROCK_LIGHT);
    }

    /* Portal de entrada da arena. */
    rect(1888 - cameraX, 90, 8, 142, PURPLE);
    rect(1896 - cameraX, 100, 3, 122, LIGHT_BLUE);

    drawCaveRoom(BOSS_START, BOSS_END, 1);

    if (!bossDefeated) {
        rect(BOSS_END - cameraX - 28, 175, 16, 57, ROCK_LIGHT);
    } else {
        rect(BOSS_END - cameraX - 28, 175, 16, 57, GOLD);
        rect(BOSS_END - cameraX - 24, 180, 8, 47, LIGHT_BLUE);
    }

    /* Cristal mágico inicial. */
    if (!player.magicUnlocked) {
        float crystalY = 192 + sinf(frameCount * 0.08f) * 5.0f;
        float crystalX = 150 - cameraX;

        rect(crystalX - 5, crystalY - 8, 10, 16, BLUE);
        rect(crystalX - 2, crystalY - 5, 4, 10, LIGHT_BLUE);
    }
}

static void drawPlayer(void)
{
    float x = player.x - cameraX;
    float y = player.y;

    if (player.invulnerable > 0 && (frameCount / 4) % 2 == 0)
        return;

    /* Pernas. */
    rect(x + 3, y + 21, 5, 9, 0xFF39324D);
    rect(x + 10, y + 21, 5, 9, 0xFF39324D);

    /* Corpo e cabeça. */
    rect(x + 2, y + 9, 14, 14, 0xFF25213D);
    rect(x + 4, y, 11, 11, 0xFFB7A7C6);

    /* Olhos. */
    rect(x + (player.facing > 0 ? 11 : 5), y + 4,
         3, 2, LIGHT_BLUE);

    /* Pequeno brilho mágico depois de pegar o cristal. */
    if (player.magicUnlocked) {
        rect(x - 2, y + 13, 3, 3, BLUE);
    }
}

static void drawSword(float x, float y, int facing)
{
    if (player.attackTimer <= 0)
        return;

    float progress = 1.0f - (float)player.attackTimer / 14.0f;

    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    float centerX = x + PW * 0.5f + facing * 8.0f;
    float centerY = y + PH * 0.45f;
    float radius = 25.0f;

    float startAngle = facing > 0 ? -1.45f : 4.59f;
    float endAngle   = facing > 0 ?  1.45f : 1.69f;

    /*
       A meia-lua é desenhada com pequenos segmentos.
       O trecho mais brilhante acompanha a animação do golpe.
    */
    for (int i = 0; i < 22; i++) {
        float t = (float)i / 21.0f;
        float angle = startAngle + (endAngle - startAngle) * t;

        float sx = centerX + cosf(angle) * radius;
        float sy = centerY + sinf(angle) * radius;

        float distance = fabsf(t - progress);
        float brightness = 1.0f - distance * 1.6f;

        if (brightness < 0.15f)
            brightness = 0.15f;

        float size = 2.0f + brightness * 3.0f;

        rect(sx - size * 0.5f, sy - size * 0.5f,
             size, size, LIGHT_BLUE);

        if (i % 2 == 0) {
            rect(sx - cameraX * 0.0f - 1,
                 sy - 1, 2, 2, WHITE);
        }
    }

    rect(centerX - 2, centerY - 2, 4, 4, BLUE);
}

static void drawBoss(void)
{
    if (!boss.active) return;

    float x = boss.x - cameraX;
    float y = boss.y;
    unsigned int body = bossHitFlash > 0 ? WHITE : 0xFF29243A;

    /* Pés e corpo. */
    rect(x + 5, y + 31, 10, 12, DARK);
    rect(x + 24, y + 31, 10, 12, DARK);
    rect(x + 2, y + 8, 35, 28, body);

    /* Cabeça e chifres. */
    rect(x + 7, y, 25, 15, 0xFF65536B);
    rect(x + 5, y - 5, 7, 9, ROCK_LIGHT);
    rect(x + 28, y - 5, 7, 9, ROCK_LIGHT);

    /* Olhos. */
    rect(x + 10, y + 5, 5, 4, RED);
    rect(x + 24, y + 5, 5, 4, RED);

    /* Braço e arma. */
    rect(x + 30, y + 16, 12, 8, 0xFF65536B);
    rect(x + 39, y + 12, 5, 22, SILVER);

    /* Barra de vida do miniboss. */
    rect(135, 14, 210, 9, 0xFF181818);
    rect(137, 16, 206, 5, DARK);

    float hpWidth = 206.0f * boss.hp / BOSS_MAX_HP;
    if (hpWidth < 0) hpWidth = 0;

    rect(137, 16, hpWidth, 5, RED);
}

static void castMagic(void)
{
    if (!player.magicUnlocked || player.magicCooldown > 0)
        return;

    for (int i = 0; i < SHOT_COUNT; i++) {
        if (!shots[i].active) {
            shots[i].x = player.x + PW * 0.5f;
            shots[i].y = player.y + PH * 0.45f;
            shots[i].vx = player.facing * 4.0f;
            shots[i].vy = -0.4f;
            shots[i].life = 120;
            shots[i].active = 1;
            player.magicCooldown = 25;
            break;
        }
    }
}

static void updateShots(void)
{
    for (int i = 0; i < SHOT_COUNT; i++) {
        Shot *s = &shots[i];

        if (!s->active) continue;

        s->life--;
        if (s->life <= 0) {
            s->active = 0;
            continue;
        }

        if (boss.active && bossStarted) {
            float dx = boss.x + 18 - s->x;
            float dy = boss.y + 20 - s->y;
            float length = sqrtf(dx * dx + dy * dy);

            if (length > 1.0f) {
                s->vx += (dx / length) * 0.13f;
                s->vy += (dy / length) * 0.13f;
            }

            float speed = sqrtf(s->vx * s->vx + s->vy * s->vy);
            if (speed > 5.0f) {
                s->vx *= 5.0f / speed;
                s->vy *= 5.0f / speed;
            }
        }

        s->x += s->vx;
        s->y += s->vy;

        if (boss.active && bossStarted && boss.damageCooldown <= 0) {
            if (s->x > boss.x && s->x < boss.x + 42 &&
                s->y > boss.y && s->y < boss.y + 44) {
                boss.hp--;
                boss.damageCooldown = 12;
                bossHitFlash = 4;

                spawnSparks(s->x, s->y, SILVER, 8);
                spawnSparks(s->x, s->y, WHITE, 4);

                s->active = 0;

                if (boss.hp <= 0) {
                    boss.hp = 0;
                    boss.active = 0;
                    bossDefeated = 1;
                }
            }
        }

        if (s->x < 0 || s->x > WORLD_W)
            s->active = 0;
    }
}

static void drawShots(void)
{
    for (int i = 0; i < SHOT_COUNT; i++) {
        if (!shots[i].active) continue;

        float x = shots[i].x - cameraX;
        float y = shots[i].y;

        circle(x, y, 5, BLUE);
        rect(x - 2, y - 2, 4, 4, LIGHT_BLUE);
    }
}

static void updateBoss(void)
{
    if (!boss.active || !bossStarted) return;

    if (boss.damageCooldown > 0)
        boss.damageCooldown--;

    if (bossHitFlash > 0)
        bossHitFlash--;

    float dx = player.x - boss.x;

    if (fabsf(dx) > 38) {
        boss.vx = dx > 0 ? 1.0f : -1.0f;
    } else {
        boss.vx = 0;

        if (boss.attackTimer <= 0) {
            boss.attackTimer = 55;
            boss.vy = -5.2f;
        }
    }

    boss.x += boss.vx;
    boss.y += boss.vy;

    boss.vy += 0.24f;

    if (boss.y >= 189) {
        boss.y = 189;
        boss.vy = 0;
        boss.grounded = 1;
    } else {
        boss.grounded = 0;
    }

    if (boss.attackTimer > 0)
        boss.attackTimer--;

    if (player.damageCooldown <= 0 &&
        player.invulnerable <= 0 &&
        player.x + PW > boss.x &&
        player.x < boss.x + 42 &&
        player.y + PH > boss.y &&
        player.y < boss.y + 44) {

        player.hp--;
        player.damageCooldown = 60;
        player.invulnerable = 60;
        player.vy = -4.5f;
        player.vx = player.x < boss.x ? -2.5f : 2.5f;

        spawnSparks(player.x + PW * 0.5f,
                    player.y + PH * 0.5f,
                    BLUE, 12);

        if (player.hp <= 0) {
            player.hp = 0;
            gameState = 3;
        }
    }
}

static void updatePlayer(SceCtrlData *pad)
{
    unsigned int buttons = pad->Buttons;

    if (player.damageCooldown > 0)
        player.damageCooldown--;

    if (player.invulnerable > 0)
        player.invulnerable--;

    if (player.magicCooldown > 0)
        player.magicCooldown--;

    if (player.attackTimer > 0)
        player.attackTimer--;

    player.vx = 0;

    if (buttons & PSP_CTRL_LEFT) {
        player.vx = -2.4f;
        player.facing = -1;
    }

    if (buttons & PSP_CTRL_RIGHT) {
        player.vx = 2.4f;
        player.facing = 1;
    }

    if (pressed(buttons, PSP_CTRL_CROSS)) {
        if (player.grounded) {
            player.vy = -8.2f;
            player.grounded = 0;
            player.jumps = 1;
        } else if (player.jumps < 2) {
            player.vy = -7.4f;
            player.jumps = 2;
            spawnSparks(player.x + PW * 0.5f,
                        player.y + PH,
                        LIGHT_BLUE, 5);
        }
    }

    /* Corte do pulo variável ao soltar X. */
    if (!(buttons & PSP_CTRL_CROSS) && player.vy < -3.0f)
        player.vy *= 0.86f;

    if (pressed(buttons, PSP_CTRL_SQUARE)) {
        player.attackTimer = 14;
        swordHitDone = 0;
    }

    if (pressed(buttons, PSP_CTRL_CIRCLE))
        castMagic();

    player.vy += 0.27f;
    if (player.vy > 7.0f)
        player.vy = 7.0f;

    player.x += player.vx;
    player.y += player.vy;

    if (player.x < 0)
        player.x = 0;

    if (player.x > WORLD_W - PW)
        player.x = WORLD_W - PW;

    float floorY = floorAt(player.x);

    if (player.y + PH >= floorY) {
        player.y = floorY - PH;
        player.vy = 0;
        player.grounded = 1;
        player.jumps = 0;
    } else {
        player.grounded = 0;
    }

    /* Pegar o cristal de magia. */
    if (!player.magicUnlocked &&
        player.x + PW > 135 &&
        player.x < 165 &&
        player.y + PH > 165) {
        player.magicUnlocked = 1;
        spawnSparks(150, 190, BLUE, 18);
    }

    /*
       A arena só começa quando o jogador atravessa a entrada.
       Depois da entrada, o miniboss fica na sala separada.
    */
    if (player.x >= BOSS_START && !bossStarted && !bossDefeated) {
        bossStarted = 1;
        player.x = BOSS_START + 8;
    }

    /*
       Enquanto o miniboss estiver vivo, o jogador não sai da arena.
       A entrada e a saída são separadas do corredor.
    */
    if (bossStarted && !bossDefeated) {
        if (player.x < BOSS_START + 2)
            player.x = BOSS_START + 2;

        if (player.x > BOSS_END - PW - 4)
            player.x = BOSS_END - PW - 4;
    }

    /* Golpe de espada contra o miniboss. */
    if (player.attackTimer > 0 &&
        !swordHitDone &&
        boss.active &&
        bossStarted &&
        boss.damageCooldown <= 0) {

        float swordX = player.x + PW * 0.5f + player.facing * 24.0f;
        float swordY = player.y + PH * 0.45f;

        if (fabsf(swordX - (boss.x + 20)) < 34 &&
            fabsf(swordY - (boss.y + 20)) < 28) {

            boss.hp--;
            boss.damageCooldown = 12;
            bossHitFlash = 4;
            swordHitDone = 1;

            spawnSparks(boss.x + 20, boss.y + 20, SILVER, 9);
            spawnSparks(boss.x + 20, boss.y + 20, WHITE, 4);

            if (boss.hp <= 0) {
                boss.hp = 0;
                boss.active = 0;
                bossDefeated = 1;
            }
        }
    }

    /* Saída liberada após derrotar o miniboss. */
    if (bossDefeated && player.x >= WORLD_W - 50) {
        gameState = 2;
    }

    /*
       A câmera acompanha o jogador, mas permanece dentro do mundo.
    */
    cameraX = player.x - SW * 0.42f;

    if (cameraX < 0)
        cameraX = 0;

    if (cameraX > WORLD_W - SW)
        cameraX = WORLD_W - SW;
}

static void drawHUD(void)
{
    for (int i = 0; i < PLAYER_MAX_HP; i++) {
        unsigned int color = i < player.hp ? RED : 0xFF38313D;
        rect(12 + i * 18, 12, 13, 9, color);
    }

    if (player.magicUnlocked) {
        rect(12, 28, 9, 9, BLUE);
        rect(14, 30, 5, 5, LIGHT_BLUE);
    }
}

static void drawMenu(void)
{
    rect(0, 0, SW, SH, DARK);

    rect(80, 58, 320, 5, PURPLE);
    rect(100, 68, 280, 3, BLUE);

    rect(190, 80, 100, 12, LIGHT_BLUE);
    rect(200, 94, 80, 5, WHITE);

    rect(150, 125, 180, 3, ROCK_LIGHT);
    rect(175, 140, 130, 3, ROCK_LIGHT);

    rect(176, 174, 128, 25, 0xFF282138);
    rect(182, 180, 116, 13, BLUE);

    rect(190, 220, 100, 3, PURPLE);
}

static void drawEnding(void)
{
    rect(0, 0, SW, SH, DARK);

    for (int i = 0; i < 22; i++) {
        float x = (i * 37 + frameCount * (i % 3 + 1)) % SW;
        float y = (i * 19 + frameCount / 2) % SH;
        rect(x, y, 3, 3, i % 2 ? LIGHT_BLUE : PURPLE);
    }

    rect(100, 90, 280, 4, GOLD);
    rect(120, 106, 240, 3, LIGHT_BLUE);
    rect(160, 135, 160, 3, GOLD);
}

int main(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();

    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, STRIDE);
    sceGuDispBuffer(SW, SH, (void *)0x88000, STRIDE);
    sceGuDepthBuffer((void *)0x110000, STRIDE);
    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SW, SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    srand(12345);
    initGame();

    while (1) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        frameCount++;

        if (pressed(pad.Buttons, PSP_CTRL_START)) {
            if (gameState == 0) {
                initGame();
                gameState = 1;
            } else if (gameState == 3) {
                initGame();
                gameState = 0;
            }
        }

        if (gameState == 1) {
            updatePlayer(&pad);
            updateShots();
            updateBoss();
            updateSparks();
        }

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(DARK);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        if (gameState == 0) {
            drawMenu();
        } else if (gameState == 1) {
            drawWorld();
            drawShots();
            drawBoss();
            drawPlayer();

            drawSword(
                player.x - cameraX,
                player.y,
                player.facing
            );

            drawSparks();
            drawHUD();
        } else if (gameState == 2) {
            drawEnding();
        } else if (gameState == 3) {
            rect(0, 0, SW, SH, DARK);
            rect(100, 90, 280, 4, RED);
            rect(130, 115, 220, 4, PURPLE);
            rect(160, 150, 160, 4, ROCK_LIGHT);
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
