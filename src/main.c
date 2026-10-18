
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define BUF_WIDTH 512
#define MAX_SHOTS 6
#define MAX_OBJECTS 12
#define WORLD_W 1500.0f
#define PLAYER_W 20.0f
#define PLAYER_H 34.0f

#define WHITE 0xFFFFFFFF
#define GRAY  0xFF888888
#define DARK  0xFF333333
#define BLACK 0xFF000000

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, w, h;
} Rect;

typedef struct {
    float x, y, vx, vy;
    int active;
    int target;
} Shot;

typedef struct {
    float x, y, w, h;
    int active;
    int room;
} MagicObject;

static unsigned int __attribute__((aligned(16)))
displayList[262144];

static int gameState = 0;
static int frameIndex = 0;
static int currentRoom = 0;

static float playerX = 35.0f;
static float playerY = 202.0f;
static float playerVY = 0.0f;
static float cameraX = 0.0f;

static int grounded = 0;
static int jumps = 0;
static int magicUnlocked = 0;
static int shotCooldown = 0;

static Shot shots[MAX_SHOTS];

static MagicObject objects[MAX_OBJECTS] = {
    {145,177,12,12,1,0},
    {235,132,12,12,1,0},
    {410,175,12,12,1,0},

    {555,157,12,12,1,1},
    {655,112,12,12,1,1},
    {765,168,12,12,1,1},
    {850,135,12,12,1,1},

    {1035,164,12,12,1,2},
    {1145,110,12,12,1,2},
    {1255,150,12,12,1,2},
    {1350,112,12,12,1,2},
    {1430,172,12,12,1,2}
};

static void initGraphics(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        BUF_WIDTH
    );

    sceGuDispBuffer(
        SW, SH,
        (void *)0x88000,
        BUF_WIDTH
    );

    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);
    sceGuScissor(0, 0, SW, SH);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

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

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)(frameIndex ? 0x88000 : 0),
        BUF_WIDTH
    );

    sceGuClearColor(BLACK);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

static void endFrame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();

    frameIndex ^= 1;
}

static void rectangle(
    float x, float y, float w, float h,
    unsigned int color
)
{
    Vertex *v = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex)
    );

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
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v
    );
}

static void worldRect(
    float x, float y, float w, float h,
    unsigned int color
)
{
    rectangle(
        x - cameraX,
        y,
        w, h,
        color
    );
}

/* Fonte bitmap compacta */
static const unsigned char font5x7[][5] = {
    {0,0,0,0,0},
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0,0x42,0x7F,0x40,0},       /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x41,0x41,0x7F,0x41,0x41}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}  /* Z */
};

static int charIndex(char c)
{
    if (c >= '0' && c <= '9')
        return 1 + c - '0';

    if (c >= 'A' && c <= 'Z')
        return 11 + c - 'A';

    return 0;
}

static void drawText(
    int x, int y,
    const char *text,
    unsigned int color
)
{
    while (*text) {
        char c = *text++;

        if (c == ' ') {
            x += 8;
            continue;
        }

        int index = charIndex(c);

        for (int col = 0; col < 5; col++) {
            unsigned char bits = font5x7[index][col];

            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) {
                    rectangle(
                        x + col * 2,
                        y + row * 2,
                        2, 2, color
                    );
                }
            }
        }

        x += 12;
    }
}

static void drawPlatform(float x, float y, float w)
{
    worldRect(x, y, w, 5, WHITE);
    worldRect(x, y + 5, w, 3, GRAY);

    for (float i = 8; i < w; i += 24)
        worldRect(x + i, y + 8, 2, 5, DARK);
}

static void drawCrystal(float x, float y)
{
    worldRect(x + 4, y, 5, 5, WHITE);
    worldRect(x + 2, y + 5, 9, 5, GRAY);
    worldRect(x, y + 10, 13, 5, WHITE);
    worldRect(x + 3, y + 15, 7, 4, GRAY);
}

static void drawSpike(float x, float y, int size)
{
    worldRect(x, y, size, 3, WHITE);
    worldRect(x + size / 4, y - 5, size / 2, 5, WHITE);
    worldRect(x + size / 3, y - 9, size / 3, 4, WHITE);
}

static void drawCracks(float x, float y)
{
    worldRect(x, y, 15, 2, GRAY);
    worldRect(x + 5, y + 2, 2, 7, GRAY);
    worldRect(x + 5, y + 9, 10, 2, GRAY);
    worldRect(x + 12, y + 11, 2, 6, GRAY);
}

static void drawRoomBackground(int room)
{
    for (int i = 0; i < 7; i++) {
        float x = room * 500 + i * 70;

        worldRect(x, 45 + (i % 3) * 12, 30, 3, DARK);
        worldRect(x + 17, 48 + (i % 3) * 12, 3, 10, GRAY);
    }

    drawCrystal(room * 500 + 95, 205);
    drawCrystal(room * 500 + 325, 193);

    drawCracks(room * 500 + 180, 80);
    drawCracks(room * 500 + 380, 100);
}

static void drawRoom0(void)
{
    drawRoomBackground(0);

    drawPlatform(0, 236, 95);
    drawPlatform(110, 236, 70);
    drawPlatform(195, 236, 90);
    drawPlatform(300, 236, 200);

    drawPlatform(70, 194, 42);
    drawPlatform(142, 165, 48);
    drawPlatform(225, 190, 45);
    drawPlatform(340, 155, 50);
    drawPlatform(420, 195, 45);

    drawSpike(185, 236, 18);
    drawSpike(287, 236, 14);

    worldRect(0, 0, 120, 35, WHITE);
    worldRect(0, 35, 95, 8, GRAY);
    worldRect(135, 0, 85, 20, WHITE);
    worldRect(150, 20, 50, 6, GRAY);
    worldRect(260, 0, 95, 28, WHITE);
    worldRect(290, 28, 65, 6, GRAY);
    worldRect(390, 0, 110, 22, WHITE);
}

static void drawRoom1(void)
{
    drawRoomBackground(1);

    drawPlatform(500, 236, 65);
    drawPlatform(590, 220, 55);
    drawPlatform(665, 236, 70);
    drawPlatform(755, 207, 45);
    drawPlatform(820, 236, 85);
    drawPlatform(920, 220, 80);

    drawPlatform(535, 180, 42);
    drawPlatform(625, 145, 55);
    drawPlatform(720, 170, 35);
    drawPlatform(795, 130, 48);
    drawPlatform(885, 165, 40);
    drawPlatform(950, 180, 45);

    drawSpike(570, 236, 20);
    drawSpike(740, 236, 20);
    drawSpike(905, 236, 16);

    worldRect(510, 0, 75, 24, WHITE);
    worldRect(530, 24, 35, 9, GRAY);
    worldRect(620, 0, 100, 17, WHITE);
    worldRect(650, 17, 35, 12, GRAY);
    worldRect(770, 0, 90, 28, WHITE);
    worldRect(790, 28, 30, 10, GRAY);
    worldRect(900, 0, 100, 18, WHITE);
}

static void drawRoom2(void)
{
    drawRoomBackground(2);

    drawPlatform(1000, 236, 80);
    drawPlatform(1100, 215, 55);
    drawPlatform(1170, 190, 45);
    drawPlatform(1230, 236, 70);
    drawPlatform(1320, 210, 65);
    drawPlatform(1400, 236, 100);

    drawPlatform(1040, 170, 45);
    drawPlatform(1130, 145, 40);
    drawPlatform(1210, 115, 55);
    drawPlatform(1300, 160, 45);
    drawPlatform(1380, 130, 45);

    drawSpike(1080, 236, 18);
    drawSpike(1210, 236, 18);
    drawSpike(1305, 236, 14);

    worldRect(1460, 70, 40, 166, WHITE);
    worldRect(1460, 70, 40, 8, GRAY);

    worldRect(1420, 165, 34, 5, WHITE);
    worldRect(1420, 170, 5, 32, GRAY);
    worldRect(1449, 170, 5, 32, GRAY);

    worldRect(1430, 178, 18, 18, WHITE);
    worldRect(1435, 183, 8, 8, BLACK);

    drawText(1380 - (int)cameraX, 95, "EXIT", WHITE);
}

static void drawCave(void)
{
    drawRoom0();
    drawRoom1();
    drawRoom2();

    worldRect(490, 190, 20, 46, BLACK);
    worldRect(990, 190, 20, 46, BLACK);
}

static void drawPlayer(void)
{
    float x = playerX - cameraX;
    float y = playerY;

    rectangle(x + 5, y, 10, 10, WHITE);
    rectangle(x + 3, y + 10, 14, 13, GRAY);

    rectangle(x, y + 12, 4, 12, WHITE);
    rectangle(x + 16, y + 12, 4, 12, WHITE);

    rectangle(x + 4, y + 23, 5, 11, WHITE);
    rectangle(x + 12, y + 23, 5, 11, WHITE);

    rectangle(x + 7, y + 4, 2, 2, BLACK);
    rectangle(x + 12, y + 4, 2, 2, BLACK);
}

static void drawObjects(void)
{
    for (int i = 0; i < MAX_OBJECTS; i++) {
        MagicObject *o = &objects[i];

        if (!o->active)
            continue;

        worldRect(o->x - 3, o->y - 3, o->w + 6, 2, GRAY);
        worldRect(o->x - 3, o->y + o->h + 1, o->w + 6, 2, GRAY);
        worldRect(o->x - 3, o->y - 3, 2, o->h + 6, GRAY);
        worldRect(o->x + o->w + 1, o->y - 3, 2, o->h + 6, GRAY);

        worldRect(o->x, o->y, o->w, o->h, WHITE);
        worldRect(o->x + 4, o->y + 4, 4, 4, BLACK);
    }
}

static int nearestObject(void)
{
    int nearest = -1;
    float best = 999999.0f;

    float px = playerX + PLAYER_W / 2;
    float py = playerY + PLAYER_H / 2;

    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active)
            continue;

        float dx = objects[i].x + objects[i].w / 2 - px;
        float dy = objects[i].y + objects[i].h / 2 - py;
        float d = dx * dx + dy * dy;

        if (d < best) {
            best = d;
            nearest = i;
        }
    }

    return nearest;
}

static void fireMagic(void)
{
    int target = nearestObject();

    if (target < 0)
        return;

    for (int i = 0; i < MAX_SHOTS; i++) {
        if (!shots[i].active) {
            shots[i].x = playerX + PLAYER_W / 2;
            shots[i].y = playerY + PLAYER_H / 2;
            shots[i].vx = 4.0f;
            shots[i].vy = 0.0f;
            shots[i].target = target;
            shots[i].active = 1;
            shotCooldown = 15;
            return;
        }
    }
}

static void updateShots(void)
{
    for (int i = 0; i < MAX_SHOTS; i++) {
        Shot *s = &shots[i];

        if (!s->active)
            continue;

        if (s->target < 0 ||
            s->target >= MAX_OBJECTS ||
            !objects[s->target].active) {
            s->active = 0;
            continue;
        }

        MagicObject *o = &objects[s->target];

        float dx = o->x + o->w / 2 - s->x;
        float dy = o->y + o->h / 2 - s->y;
        float distance = sqrtf(dx * dx + dy * dy);

        if (distance < 9.0f) {
            o->active = 0;
            s->active = 0;
            continue;
        }

        if (distance > 0.01f) {
            float desiredVX = dx / distance * 5.0f;
            float desiredVY = dy / distance * 5.0f;

            s->vx += (desiredVX - s->vx) * 0.22f;
            s->vy += (desiredVY - s->vy) * 0.22f;
        }

        s->x += s->vx;
        s->y += s->vy;

        if (s->x < 0 || s->x > WORLD_W ||
            s->y < 0 || s->y > SH) {
            s->active = 0;
        }
    }
}

static void drawShots(void)
{
    for (int i = 0; i < MAX_SHOTS; i++) {
        if (!shots[i].active)
            continue;

        float x = shots[i].x - cameraX;
        float y = shots[i].y;

        rectangle(x - 5, y - 2, 10, 4, GRAY);
        rectangle(x - 2, y - 4, 4, 8, WHITE);
        rectangle(x - shots[i].vx * 2,
                  y - shots[i].vy * 2,
                  3, 3, GRAY);
    }
}

static void drawMenu(void)
{
    rectangle(0, 0, SW, SH, BLACK);

    rectangle(65, 48, 350, 4, WHITE);
    rectangle(65, 52, 4, 48, GRAY);
    rectangle(411, 52, 4, 48, GRAY);

    drawText(174, 68, "RANDOM", WHITE);
    drawText(143, 125, "PRESS START", WHITE);
    drawText(120, 160, "3 CONNECTED ROOMS", GRAY);

    rectangle(120, 215, 240, 3, WHITE);
}

static void drawIntro(void)
{
    rectangle(0, 0, SW, SH, BLACK);

    drawText(174, 55, "RANDOM", WHITE);
    drawText(120, 95, "EXPLORE THE CAVE", WHITE);
    drawText(90, 130, "X JUMP", GRAY);
    drawText(90, 150, "O MAGIC", GRAY);
    drawText(90, 180, "START MENU", GRAY);
    drawText(100, 225, "PRESS START", WHITE);
}

static void drawGame(void)
{
    beginFrame();

    drawCave();
    drawObjects();
    drawShots();
    drawPlayer();

    drawText(8, 8,
        magicUnlocked ? "MAGIC READY" : "FIND MAGIC",
        WHITE);

    if (currentRoom == 0)
        drawText(190, 8, "START", GRAY);
    else if (currentRoom == 1)
        drawText(190, 8, "CAVE", GRAY);
    else
        drawText(190, 8, "ZONE 1", GRAY);

    endFrame();
}

static int overlaps(
    float ax, float ay, float aw, float ah,
    float bx, float by, float bw, float bh
)
{
    return ax < bx + bw &&
           ax + aw > bx &&
           ay < by + bh &&
           ay + ah > by;
}

static void updateGame(SceCtrlData pad, unsigned int pressed)
{
    float vx = 0;

    if (pad.Buttons & PSP_CTRL_LEFT)
        vx = -2.5f;

    if (pad.Buttons & PSP_CTRL_RIGHT)
        vx = 2.5f;

    playerX += vx;

    if (playerX < 0)
        playerX = 0;

    if (playerX > WORLD_W - PLAYER_W)
        playerX = WORLD_W - PLAYER_W;

    if ((pressed & PSP_CTRL_CROSS) && grounded) {
        playerVY = -7.5f;
        grounded = 0;
        jumps = 1;
    } else if ((pressed & PSP_CTRL_CROSS) && jumps < 2) {
        playerVY = -7.0f;
        grounded = 0;
        jumps++;
    }

    playerVY += 0.32f;

    if (playerVY > 7.0f)
        playerVY = 7.0f;

    float oldBottom = playerY + PLAYER_H;
    playerY += playerVY;
    grounded = 0;

    Rect floors[] = {
        {0,236,95,36},
        {110,236,70,36},
        {195,236,305,36},
        {500,236,65,36},
        {665,236,70,36},
        {820,236,85,36},
        {1000,236,80,36},
        {1230,236,70,36},
        {1400,236,100,36}
    };

    Rect platforms[] = {
        {70,194,42,6},{142,165,48,6},
        {225,190,45,6},{340,155,50,6},
        {420,195,45,6},{535,180,42,6},
        {625,145,55,6},{720,170,35,6},
        {795,130,48,6},{885,165,40,6},
        {950,180,45,6},{1040,170,45,6},
        {1130,145,40,6},{1210,115,55,6},
        {1300,160,45,6},{1380,130,45,6}
    };

    for (int i = 0; i < (int)(sizeof(floors) / sizeof(floors[0])); i++) {
        Rect f = floors[i];

        if (playerVY >= 0 &&
            oldBottom <= f.y + 2 &&
            playerY + PLAYER_H >= f.y &&
            playerX + PLAYER_W > f.x &&
            playerX < f.x + f.w) {
            playerY = f.y - PLAYER_H;
            playerVY = 0;
            grounded = 1;
            jumps = 0;
        }
    }

    for (int i = 0; i < (int)(sizeof(platforms) / sizeof(platforms[0])); i++) {
        Rect p = platforms[i];

        if (playerVY >= 0 &&
            oldBottom <= p.y + 2 &&
            playerY + PLAYER_H >= p.y &&
            playerX + PLAYER_W > p.x &&
            playerX < p.x + p.w) {
            playerY = p.y - PLAYER_H;
            playerVY = 0;
            grounded = 1;
            jumps = 0;
        }
    }

    if (!magicUnlocked &&
        overlaps(playerX, playerY, PLAYER_W, PLAYER_H,
                 130, 165, 35, 30)) {
        magicUnlocked = 1;
    }

    if (shotCooldown > 0)
        shotCooldown--;

    if (magicUnlocked &&
        (pressed & PSP_CTRL_CIRCLE) &&
        shotCooldown == 0) {
        fireMagic();
    }

    updateShots();

    if (playerY > SH + 100) {
        playerX = 35;
        playerY = 202;
        playerVY = 0;
    }

    cameraX = playerX - 210;

    if (cameraX < 0)
        cameraX = 0;

    if (cameraX > WORLD_W - SW)
        cameraX = WORLD_W - SW;

    if (playerX < 500)
        currentRoom = 0;
    else if (playerX < 1000)
        currentRoom = 1;
    else
        currentRoom = 2;

    if (playerX > 1440) {
        playerX = 35;
        playerY = 202;
        playerVY = 0;
        magicUnlocked = 0;

        for (int i = 0; i < MAX_OBJECTS; i++)
            objects[i].active = 1;

        for (int i = 0; i < MAX_SHOTS; i++)
            shots[i].active = 0;
    }
}


int main(void)
{
    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initGraphics();
    memset(shots, 0, sizeof(shots));

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        /* Menu: aceita X, O ou START diretamente.
           Isso evita depender da detecção de borda. */
        if (gameState == 0) {
            beginFrame();
            drawMenu();
            endFrame();

            if (pad.Buttons &
                (PSP_CTRL_CROSS |
                 PSP_CTRL_CIRCLE |
                 PSP_CTRL_START)) {
                gameState = 1;
            }
        }
        else if (gameState == 1) {
            beginFrame();
            drawIntro();
            endFrame();

            if (pad.Buttons &
                (PSP_CTRL_CROSS |
                 PSP_CTRL_CIRCLE |
                 PSP_CTRL_START)) {
                gameState = 2;
            }
        }
        else {
            if (pad.Buttons & PSP_CTRL_START) {
                gameState = 0;
                sceKernelDelayThread(200000);
                continue;
            }

            /* Compatibilidade com X/O mesmo sem detecção de borda. */
            unsigned int pressed = pad.Buttons;

            updateGame(pad, pressed);
            drawGame();
        }

        sceKernelDelayThread(16667);
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}

