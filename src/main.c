
/*
 * RANDOM - PSP / PSPSDK
 * Menu, fonte bitmap, sprites PNG e pausa.
 *
 * Estrutura:
 * RANDOM/
 *   EBOOT.PBP
 *   assets/sprites/
 *     idle_1.png, idle_2.png
 *     walk_1.png, walk_2.png, walk_3.png, walk_4.png
 *     run_1.png, run_2.png, run_3.png, run_4.png
 *     jump_1.png, jump_2.png
 *     fall.png, landing.png
 *     attack_1.png, attack_2.png
 *     hurt.png, magic.png
 *
 * Controles:
 * Direcional = mover
 * X = salto duplo
 * R + direcional = correr
 * Quadrado = atacar
 * Circulo = magia
 * Start = pausar
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <setjmp.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512

#define PLAYER_W 64
#define PLAYER_H 72

#define FLOOR_Y 224.0f
#define GRAVITY 0.42f
#define JUMP_VELOCITY -8.5f

#define SPRITE_COUNT 18

static unsigned int __attribute__((aligned(16)))
displayList[262144];

typedef struct {
    int width;
    int height;
    int texWidth;
    int texHeight;
    unsigned int *pixels;
    int loaded;
} Sprite;

static Sprite sprites[SPRITE_COUNT];

enum {
    SPR_IDLE1, SPR_IDLE2,
    SPR_WALK1, SPR_WALK2, SPR_WALK3, SPR_WALK4,
    SPR_RUN1, SPR_RUN2, SPR_RUN3, SPR_RUN4,
    SPR_JUMP1, SPR_JUMP2,
    SPR_FALL, SPR_LANDING,
    SPR_ATTACK1, SPR_ATTACK2,
    SPR_HURT, SPR_MAGIC
};

static const char *spriteNames[SPRITE_COUNT] = {
    "idle_1.png", "idle_2.png",
    "walk_1.png", "walk_2.png", "walk_3.png", "walk_4.png",
    "run_1.png", "run_2.png", "run_3.png", "run_4.png",
    "jump_1.png", "jump_2.png",
    "fall.png", "landing.png",
    "attack_1.png", "attack_2.png",
    "hurt.png", "magic.png"
};

enum {
    SCREEN_MENU,
    SCREEN_GAME,
    SCREEN_PAUSE
};

static int screenState = SCREEN_MENU;
static int menuSelection = 0;
static int pauseSelection = 0;

static float playerX = 70.0f;
static float playerY = FLOOR_Y - PLAYER_H;
static float playerVY = 0.0f;
static float cameraX = 0.0f;

static int grounded = 1;
static int jumpsRemaining = 2;
static int facingRight = 1;

static int attackTimer = 0;
static int magicTimer = 0;
static int hurtTimer = 0;
static int landingTimer = 0;

static int animationFrame = 0;
static int animationClock = 0;
static int currentSprite = SPR_IDLE1;

static int running = 0;
static int moving = 0;
static int previousButtons = 0;
static int gameRunning = 1;
static int loadedSprites = 0;

/* ---------------------------------------------------------
   CONTROLES
   --------------------------------------------------------- */

static unsigned int currentButtons(void)
{
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(&pad, 1);
    return pad.Buttons;
}

static int pressed(unsigned int buttons)
{
    unsigned int now = currentButtons();

    return ((now & buttons) != 0) &&
           ((previousButtons & buttons) == 0);
}

/* ---------------------------------------------------------
   PNG
   --------------------------------------------------------- */

static int nextPowerOfTwo(int value)
{
    int result = 1;

    while (result < value && result < 2048)
        result <<= 1;

    return result;
}

static void freeSprite(Sprite *s)
{
    if (s->pixels) {
        free(s->pixels);
        s->pixels = NULL;
    }

    s->loaded = 0;
    s->width = 0;
    s->height = 0;
    s->texWidth = 0;
    s->texHeight = 0;
}

static int loadPNG(const char *path, Sprite *out)
{
    FILE *file = fopen(path, "rb");

    if (!file)
        return 0;

    unsigned char signature[8];

    if (fread(signature, 1, 8, file) != 8 ||
        png_sig_cmp(signature, 0, 8)) {
        fclose(file);
        return 0;
    }

    png_structp png = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png) {
        fclose(file);
        return 0;
    }

    png_infop info = png_create_info_struct(png);

    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(file);
        return 0;
    }

    unsigned char *raw = NULL;
    png_bytep *rows = NULL;
    unsigned int *texture = NULL;

    if (setjmp(png_jmpbuf(png))) {
        free(raw);
        free(rows);
        free(texture);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    png_init_io(png, file);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);

    int colorType = png_get_color_type(png, info);
    int bitDepth = png_get_bit_depth(png, info);

    if (width == 0 || height == 0 ||
        width > 1024 || height > 1024) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    if (bitDepth == 16)
        png_set_strip_16(png);

    if (colorType == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    if (!(colorType & PNG_COLOR_MASK_ALPHA) &&
        !png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);

    png_set_interlace_handling(png);
    png_read_update_info(png, info);

    int rowBytes = (int)png_get_rowbytes(png, info);

    if (rowBytes < (int)width * 4) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    raw = (unsigned char *)malloc((size_t)rowBytes * height);
    rows = (png_bytep *)malloc(sizeof(png_bytep) * height);

    if (!raw || !rows) {
        free(raw);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    for (png_uint_32 y = 0; y < height; y++)
        rows[y] = raw + (size_t)y * rowBytes;

    png_read_image(png, rows);
    png_read_end(png, NULL);

    int tw = nextPowerOfTwo((int)width);
    int th = nextPowerOfTwo((int)height);

    texture = (unsigned int *)memalign(
        16, (size_t)tw * th * sizeof(unsigned int));

    if (!texture) {
        free(raw);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    memset(texture, 0, (size_t)tw * th * sizeof(unsigned int));

    for (png_uint_32 y = 0; y < height; y++) {
        unsigned char *src = rows[y];

        for (png_uint_32 x = 0; x < width; x++) {
            unsigned char r = src[x * 4 + 0];
            unsigned char g = src[x * 4 + 1];
            unsigned char b = src[x * 4 + 2];
            unsigned char a = src[x * 4 + 3];

            texture[y * tw + x] =
                ((unsigned int)a << 24) |
                ((unsigned int)b << 16) |
                ((unsigned int)g << 8) |
                (unsigned int)r;
        }
    }

    free(raw);
    free(rows);

    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);

    out->width = (int)width;
    out->height = (int)height;
    out->texWidth = tw;
    out->texHeight = th;
    out->pixels = texture;
    out->loaded = 1;

    return 1;
}

/* ---------------------------------------------------------
   CARREGAR SPRITES
   --------------------------------------------------------- */

static int loadAllSprites(void)
{
    const char *directories[] = {
        "ms0:/PSP/GAME/RANDOM/assets/sprites/",
        "ef0:/PSP/GAME/RANDOM/assets/sprites/",
        "assets/sprites/"
    };

    int total = 0;
    char path[256];

    for (int i = 0; i < SPRITE_COUNT; i++) {
        freeSprite(&sprites[i]);

        for (int d = 0; d < 3; d++) {
            snprintf(path, sizeof(path), "%s%s",
                     directories[d], spriteNames[i]);

            if (loadPNG(path, &sprites[i])) {
                total++;
                break;
            }
        }
    }

    return total;
}

/* ---------------------------------------------------------
   GRAFICOS PSP
   --------------------------------------------------------- */

static void initGraphics(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void *)0x88000, BUF_W);
    sceGuDepthBuffer((void *)0x110000, BUF_W);

    sceGuOffset(2048 - SCREEN_W / 2, 2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);

    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(
        GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    sceGuClearColor(0xFF100D1B);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* ---------------------------------------------------------
   RETANGULOS
   --------------------------------------------------------- */

typedef struct {
    float x, y, z;
} Vertex;

static void drawRect(
    float x, float y, float w, float h,
    unsigned int color)
{
    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);

    Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));

    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, NULL, v);

    sceGuEnable(GU_TEXTURE_2D);
}

/* ---------------------------------------------------------
   FONTE 5x7 ESTATICA
   --------------------------------------------------------- */

static const unsigned char font5x7[41][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14},      {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},   {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},     {31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},     {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},     {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
    {0,4,4,0,4,4,0},        {0,0,0,31,0,0,0},
    {0,0,0,0,0,12,12},      {16,8,4,2,4,8,16},
    {0,0,0,0,0,0,0}
};

static const unsigned char *glyph(char c)
{
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');

    if (c >= 'A' && c <= 'Z')
        return font5x7[c - 'A'];

    if (c >= '0' && c <= '9')
        return font5x7[26 + c - '0'];

    switch (c) {
        case ':': return font5x7[36];
        case '-': return font5x7[37];
        case '.': return font5x7[38];
        case '>': return font5x7[39];
        case ' ': return font5x7[40];
        default: return font5x7[40];
    }
}

static void drawText(
    int x, int y, const char *text,
    int scale, unsigned int color)
{
    int startX = x;

    if (scale < 1)
        scale = 1;

    while (*text) {
        char c = *text++;

        if (c == '\n') {
            y += 9 * scale;
            x = startX;
            continue;
        }

        const unsigned char *rows = glyph(c);

        for (int row = 0; row < 7; row++) {
            unsigned char bits = rows[row];

            for (int col = 0; col < 5; col++) {
                if (bits & (1 << (4 - col))) {
                    drawRect(
                        (float)(x + col * scale),
                        (float)(y + row * scale),
                        (float)scale, (float)scale, color);
                }
            }
        }

        x += 6 * scale;
    }
}

/* ---------------------------------------------------------
   SPRITES TEXTURIZADOS
   --------------------------------------------------------- */

typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} TextureVertex;

static void drawSprite(
    int spriteIndex, float x, float y,
    float w, float h, int flip)
{
    if (spriteIndex < 0 || spriteIndex >= SPRITE_COUNT)
        return;

    Sprite *s = &sprites[spriteIndex];

    if (!s->loaded || !s->pixels)
        return;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(0, s->texWidth, s->texHeight,
                  s->texWidth, s->pixels);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuColor(0xFFFFFFFF);

    TextureVertex *v =
        (TextureVertex *)sceGuGetMemory(2 * sizeof(TextureVertex));

    v[0].u = flip ? (float)s->width : 0.0f;
    v[0].v = 0.0f;
    v[0].color = 0xFFFFFFFF;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].u = flip ? 0.0f : (float)s->width;
    v[1].v = (float)s->height;
    v[1].color = 0xFFFFFFFF;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_32BITF |
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, NULL, v);
}

/* ---------------------------------------------------------
   CENARIO
   --------------------------------------------------------- */

static void drawBackground(void)
{
    drawRect(0, 0, SCREEN_W, SCREEN_H, 0xFF171321);

    drawRect(0, 0, SCREEN_W, 8, 0xFF29223A);
    drawRect(25, 48, 45, 7, 0xFF302641);
    drawRect(120, 72, 62, 8, 0xFF302641);
    drawRect(300, 40, 55, 8, 0xFF302641);
    drawRect(400, 92, 45, 7, 0xFF302641);

    drawRect(72, 126, 4, 18, 0xFFFFC6FF);
    drawRect(66, 132, 16, 5, 0xFFFF70ED);

    drawRect(360, 144, 4, 18, 0xFFFFC6FF);
    drawRect(354, 150, 16, 5, 0xFFFF70ED);

    drawRect(0, 200, 110, 12, 0xFF51405B);
    drawRect(145, 184, 82, 12, 0xFF51405B);
    drawRect(270, 164, 78, 12, 0xFF51405B);
    drawRect(385, 190, 95, 12, 0xFF51405B);

    drawRect(0, FLOOR_Y, SCREEN_W, SCREEN_H - FLOOR_Y,
             0xFF45374D);
    drawRect(0, FLOOR_Y, SCREEN_W, 4, 0xFF8D6F99);
}

static void drawPlayer(void)
{
    float screenX = playerX - cameraX;
    int sprite = currentSprite;

    if (hurtTimer > 0)
        sprite = SPR_HURT;
    else if (magicTimer > 0)
        sprite = SPR_MAGIC;
    else if (attackTimer > 0)
        sprite = attackTimer > 7 ? SPR_ATTACK1 : SPR_ATTACK2;
    else if (!grounded)
        sprite = playerVY < 0
            ? (playerVY < -4.0f ? SPR_JUMP1 : SPR_JUMP2)
            : SPR_FALL;
    else if (landingTimer > 0)
        sprite = SPR_LANDING;

    if (sprites[sprite].loaded) {
        drawSprite(sprite, screenX, playerY,
                   PLAYER_W, PLAYER_H, !facingRight);
    } else {
        drawRect(screenX + 18, playerY + 4, 28, 25, 0xFFFF80E8);
        drawRect(screenX + 14, playerY + 30, 36, 30, 0xFF8E64D9);
        drawRect(screenX + 12, playerY + 60, 15, 12, 0xFFFFFFFF);
        drawRect(screenX + 37, playerY + 60, 15, 12, 0xFFFFFFFF);
    }
}

/* ---------------------------------------------------------
   ATUALIZACAO DO JOGADOR
   --------------------------------------------------------- */

static void startGame(void)
{
    playerX = 70.0f;
    playerY = FLOOR_Y - PLAYER_H;
    playerVY = 0.0f;
    cameraX = 0.0f;

    grounded = 1;
    jumpsRemaining = 2;
    facingRight = 1;

    attackTimer = 0;
    magicTimer = 0;
    hurtTimer = 0;
    landingTimer = 0;

    animationFrame = 0;
    animationClock = 0;
    currentSprite = SPR_IDLE1;

    screenState = SCREEN_GAME;
}

static void updatePlayer(void)
{
    unsigned int buttons = currentButtons();

    moving = 0;
    running = 0;

    if (buttons & PSP_CTRL_LEFT) {
        playerX -= (buttons & PSP_CTRL_RTRIGGER) ? 3.8f : 2.0f;
        facingRight = 0;
        moving = 1;
        running = (buttons & PSP_CTRL_RTRIGGER) != 0;
    }

    if (buttons & PSP_CTRL_RIGHT) {
        playerX += (buttons & PSP_CTRL_RTRIGGER) ? 3.8f : 2.0f;
        facingRight = 1;
        moving = 1;
        running = (buttons & PSP_CTRL_RTRIGGER) != 0;
    }

    if (playerX < 0)
        playerX = 0;

    if (pressed(PSP_CTRL_CROSS) && jumpsRemaining > 0) {
        playerVY = JUMP_VELOCITY;
        grounded = 0;
        jumpsRemaining--;
        landingTimer = 0;
    }

    if (pressed(PSP_CTRL_SQUARE))
        attackTimer = 14;

    if (pressed(PSP_CTRL_CIRCLE))
        magicTimer = 22;

    if (attackTimer > 0) attackTimer--;
    if (magicTimer > 0) magicTimer--;
    if (hurtTimer > 0) hurtTimer--;
    if (landingTimer > 0) landingTimer--;

    if (!grounded) {
        playerVY += GRAVITY;
        playerY += playerVY;

        if (playerY + PLAYER_H >= FLOOR_Y) {
            playerY = FLOOR_Y - PLAYER_H;
            playerVY = 0.0f;
            grounded = 1;
            jumpsRemaining = 2;
            landingTimer = 8;
        }
    }

    if (playerX > 2500)
        playerX = 2500;

    cameraX = playerX - SCREEN_W * 0.4f;
    if (cameraX < 0)
        cameraX = 0;

    if (grounded && moving) {
        animationClock++;

        if (animationClock >= (running ? 5 : 9)) {
            animationClock = 0;
            animationFrame++;
        }

        currentSprite = running
            ? SPR_RUN1 + animationFrame % 4
            : SPR_WALK1 + animationFrame % 4;
    } else if (grounded) {
        animationClock++;

        if (animationClock >= 30) {
            animationClock = 0;
            animationFrame++;
        }

        currentSprite = (animationFrame % 2)
            ? SPR_IDLE1 : SPR_IDLE2;
    }
}

/* ---------------------------------------------------------
   MENU
   --------------------------------------------------------- */

static void drawMenu(void)
{
    char countText[8];

    drawRect(0, 0, SCREEN_W, SCREEN_H, 0xFF100D1B);
    drawRect(44, 42, 392, 3, 0xFF644B86);
    drawRect(44, 206, 392, 3, 0xFF644B86);

    drawText(140, 65, "RANDOM", 4, 0xFFFF80E8);
    drawText(122, 115, "PSP ADVENTURE", 1, 0xFFFFFFFF);

    drawText(150, 155,
        menuSelection == 0 ? "> NOVO JOGO" : "  NOVO JOGO",
        1, menuSelection == 0 ? 0xFFFF80E8 : 0xFFFFFFFF);

    drawText(150, 177,
        menuSelection == 1 ? "> SAIR" : "  SAIR",
        1, menuSelection == 1 ? 0xFFFF80E8 : 0xFFFFFFFF);

    drawText(130, 230, "DIRECIONAL - ESCOLHER", 1, 0xFFB8A9C9);
    drawText(170, 244, "X - CONFIRMAR", 1, 0xFFB8A9C9);

    drawText(10, 10, "PNG:", 1, 0xFFFFFFFF);
    snprintf(countText, sizeof(countText), "%d", loadedSprites);
    drawText(40, 10, countText, 1, 0xFFFF80E8);
}

/* ---------------------------------------------------------
   PAUSA
   --------------------------------------------------------- */

static void drawPause(void)
{
    drawRect(0, 0, SCREEN_W, SCREEN_H, 0xD0100D1B);
    drawRect(90, 54, 300, 164, 0xFF292139);
    drawRect(90, 54, 300, 4, 0xFFFF80E8);

    drawText(190, 75, "PAUSADO", 2, 0xFFFF80E8);

    drawText(125, 120,
        pauseSelection == 0 ? "> CONTINUAR" : "  CONTINUAR",
        1, pauseSelection == 0 ? 0xFFFF80E8 : 0xFFFFFFFF);

    drawText(125, 143,
        pauseSelection == 1 ? "> MENU PRINCIPAL" : "  MENU PRINCIPAL",
        1, pauseSelection == 1 ? 0xFFFF80E8 : 0xFFFFFFFF);

    drawText(125, 166,
        pauseSelection == 2 ? "> SAIR" : "  SAIR",
        1, pauseSelection == 2 ? 0xFFFF80E8 : 0xFFFFFFFF);

    drawText(140, 192, "X - CONFIRMAR", 1, 0xFFB8A9C9);
}

/* ---------------------------------------------------------
   RENDERIZACAO
   --------------------------------------------------------- */

static void render(void)
{
    sceGuStart(GU_DIRECT, displayList);

    sceGuClearColor(0xFF100D1B);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    if (screenState == SCREEN_MENU) {
        drawMenu();
    } else {
        drawBackground();
        drawPlayer();

        drawRect(8, 8, 180, 22, 0xC0100D1B);
        drawText(16, 15, "RANDOM - TESTE", 1, 0xFFFFFFFF);

        if (screenState == SCREEN_PAUSE)
            drawPause();
    }

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

/* ---------------------------------------------------------
   MAIN
   --------------------------------------------------------- */

int main(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initGraphics();

    loadedSprites = loadAllSprites();

    while (gameRunning) {
        unsigned int buttons = currentButtons();

        if (screenState == SCREEN_MENU) {
            if (pressed(PSP_CTRL_UP) || pressed(PSP_CTRL_DOWN))
                menuSelection = 1 - menuSelection;

            if (pressed(PSP_CTRL_CROSS)) {
                if (menuSelection == 0)
                    startGame();
                else
                    gameRunning = 0;
            }

            if (pressed(PSP_CTRL_START))
                startGame();
        }
        else if (screenState == SCREEN_GAME) {
            if (pressed(PSP_CTRL_START)) {
                screenState = SCREEN_PAUSE;
                pauseSelection = 0;
            } else {
                updatePlayer();
            }
        }
        else if (screenState == SCREEN_PAUSE) {
            if (pressed(PSP_CTRL_UP)) {
                pauseSelection--;
                if (pauseSelection < 0)
                    pauseSelection = 2;
            }

            if (pressed(PSP_CTRL_DOWN)) {
                pauseSelection++;
                if (pauseSelection > 2)
                    pauseSelection = 0;
            }

            if (pressed(PSP_CTRL_CIRCLE))
                screenState = SCREEN_GAME;

            if (pressed(PSP_CTRL_CROSS)) {
                if (pauseSelection == 0)
                    screenState = SCREEN_GAME;
                else if (pauseSelection == 1)
                    screenState = SCREEN_MENU;
                else
                    gameRunning = 0;
            }

            if (pressed(PSP_CTRL_START))
                screenState = SCREEN_GAME;
        }

        render();

        previousButtons = buttons;
        sceKernelDelayThread(16000);
    }

    for (int i = 0; i < SPRITE_COUNT; i++)
        freeSprite(&sprites[i]);

    sceGuDisplay(GU_FALSE);
    sceGuTerm();

    sceKernelExitGame();
    return 0;
}
