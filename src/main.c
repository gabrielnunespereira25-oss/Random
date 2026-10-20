
/*
 * RANDOM - PSP / PSPSDK
 * Versao corrigida: renderizacao, retangulos e carregamento PNG.
 *
 * Controles:
 * Direcional: mover
 * X: salto duplo
 * R + Direcional: correr
 * Quadrado: ataque de meia-lua
 * Circulo: magia
 * Start: pausar
 *
 * Requer PSPSDK, libpng e zlib.
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <setjmp.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512

#define GRAVITY 0.42f
#define JUMP_SPEED -7.6f
#define WALK_SPEED 2.3f
#define RUN_SPEED 3.8f

#define PLAYER_W 64
#define PLAYER_H 82
#define GROUND_Y 180.0f

#define SPRITE_COUNT 15

typedef struct {
    unsigned int *pixels;
    int w;
    int h;
    int tw;
    int th;
} Texture;

typedef struct {
    float u, v;
    float x, y, z;
} Vertex;

typedef struct {
    float x, y, z;
} RectVertex;

static unsigned int __attribute__((aligned(16)))
    displayList[262144];

static Texture sprites[SPRITE_COUNT];

static const char *spriteNames[SPRITE_COUNT] = {
    "idle_1.png",
    "walk_1.png",
    "walk_2.png",
    "run_1.png",
    "run_2.png",
    "run_3.png",
    "run_4.png",
    "jump_1.png",
    "jump_2.png",
    "fall.png",
    "landing.png",
    "attack_1.png",
    "attack_2.png",
    "hurt.png",
    "magic.png"
};

enum {
    IDLE,
    WALK,
    RUN,
    JUMP,
    FALL,
    LAND,
    ATTACK,
    HURT,
    MAGIC
};

static float playerX = 80.0f;
static float playerY = GROUND_Y;
static float velocityY = 0.0f;

static float animationTime = 0.0f;
static float attackTime = 0.0f;
static float magicTime = 0.0f;

static int facing = 1;
static int grounded = 1;
static int jumpCount = 0;
static int paused = 0;
static int magicUnlocked = 1;

static int currentState = IDLE;
static int animationFrame = 0;

static unsigned int previousButtons = 0;

/* ---------------------------------------------------------
   Utilitarios
   --------------------------------------------------------- */

static int nextPowerOfTwo(int n)
{
    int p = 1;

    while (p < n && p < 512)
        p <<= 1;

    return p;
}

static void resetTexture(Texture *t)
{
    t->pixels = NULL;
    t->w = 0;
    t->h = 0;
    t->tw = 0;
    t->th = 0;
}

/* ---------------------------------------------------------
   Carregamento PNG
   --------------------------------------------------------- */

static int loadPNG(const char *path, Texture *t)
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
        PNG_LIBPNG_VER_STRING,
        NULL,
        NULL,
        NULL
    );

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

    unsigned char *rgba = NULL;
    png_bytep *rows = NULL;
    unsigned int *pixels = NULL;

    if (setjmp(png_jmpbuf(png))) {
        free(rgba);
        free(rows);
        free(pixels);

        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);

        return 0;
    }

    png_init_io(png, file);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    int width = (int)png_get_image_width(png, info);
    int height = (int)png_get_image_height(png, info);

    int colorType = png_get_color_type(png, info);
    int bitDepth = png_get_bit_depth(png, info);

    if (width <= 0 || height <= 0 ||
        width > 512 || height > 512) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    if (bitDepth == 16)
        png_set_strip_16(png);

    if (colorType == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (colorType == PNG_COLOR_TYPE_GRAY &&
        bitDepth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    if (!(colorType & PNG_COLOR_MASK_ALPHA) &&
        !png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_add_alpha(
            png,
            0xFF,
            PNG_FILLER_AFTER
        );
    }

    png_read_update_info(png, info);

    if (png_get_channels(png, info) != 4 ||
        png_get_bit_depth(png, info) != 8) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    png_size_t rowBytes = png_get_rowbytes(png, info);

    if (rowBytes < (png_size_t)width * 4) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    int textureW = nextPowerOfTwo(width);
    int textureH = nextPowerOfTwo(height);

    rgba = (unsigned char *)malloc(
        (size_t)width * height * 4
    );

    rows = (png_bytep *)malloc(
        sizeof(png_bytep) * height
    );

    if (!rgba || !rows) {
        free(rgba);
        free(rows);

        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    for (int y = 0; y < height; y++) {
        rows[y] = rgba + (size_t)y * width * 4;
    }

    png_read_image(png, rows);
    png_read_end(png, NULL);

    pixels = (unsigned int *)memalign(
        16,
        (size_t)textureW * textureH * 4
    );

    if (!pixels) {
        free(rgba);
        free(rows);

        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    memset(
        pixels,
        0,
        (size_t)textureW * textureH * 4
    );

    /*
     * Copia cada linha para a textura com largura
     * em potencia de dois, exigida pelo PSP GU.
     */
    for (int y = 0; y < height; y++) {
        memcpy(
            ((unsigned char *)pixels) +
                (size_t)y * textureW * 4,
            rgba + (size_t)y * width * 4,
            (size_t)width * 4
        );
    }

    t->pixels = pixels;
    t->w = width;
    t->h = height;
    t->tw = textureW;
    t->th = textureH;

    free(rgba);
    free(rows);

    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);

    return 1;
}

static int loadSprite(int index)
{
    char path[192];

    resetTexture(&sprites[index]);

    /*
     * O caminho relativo depende do diretorio atual
     * usado para iniciar o homebrew.
     */
    snprintf(
        path,
        sizeof(path),
        "assets/sprites/%s",
        spriteNames[index]
    );

    if (loadPNG(path, &sprites[index]))
        return 1;

    snprintf(
        path,
        sizeof(path),
        "ms0:/PSP/GAME/RANDOM/assets/sprites/%s",
        spriteNames[index]
    );

    if (loadPNG(path, &sprites[index]))
        return 1;

    snprintf(
        path,
        sizeof(path),
        "ms0:/PSP/GAME/Random/assets/sprites/%s",
        spriteNames[index]
    );

    return loadPNG(path, &sprites[index]);
}

/* ---------------------------------------------------------
   Desenho de texturas
   --------------------------------------------------------- */

static void drawTexture(
    Texture *texture,
    float x,
    float y,
    float width,
    float height,
    int flip)
{
    if (!texture || !texture->pixels)
        return;

    sceGuEnable(GU_TEXTURE_2D);

    sceGuTexMode(GU_PSM_8888, 0, 0, 0);

    sceGuTexImage(
        0,
        texture->tw,
        texture->th,
        texture->tw,
        texture->pixels
    );

    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuColor(0xFFFFFFFF);

    Vertex *v = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex)
    );

    if (!v) {
        sceGuDisable(GU_TEXTURE_2D);
        return;
    }

    v[0].u = flip ? (float)texture->w : 0.0f;
    v[0].v = 0.0f;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].u = flip ? 0.0f : (float)texture->w;
    v[1].v = (float)texture->h;
    v[1].x = x + width;
    v[1].y = y + height;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_32BITF |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2,
        0,
        v
    );

    sceGuDisable(GU_TEXTURE_2D);
}

/* ---------------------------------------------------------
   Desenho de retangulos corrigido
   --------------------------------------------------------- */

static void drawRect(
    float x,
    float y,
    float width,
    float height,
    unsigned int color)
{
    if (width <= 0 || height <= 0)
        return;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);

    RectVertex *v = (RectVertex *)sceGuGetMemory(
        2 * sizeof(RectVertex)
    );

    if (!v)
        return;

    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].x = x + width;
    v[1].y = y + height;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2,
        0,
        v
    );
}

/* ---------------------------------------------------------
   Inicializacao grafica
   --------------------------------------------------------- */

static void initializeGraphics(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        BUF_W
    );

    sceGuDispBuffer(
        SCREEN_W,
        SCREEN_H,
        (void *)0x88000,
        BUF_W
    );

    sceGuDepthBuffer(
        (void *)0x110000,
        BUF_W
    );

    sceGuOffset(
        2048 - SCREEN_W / 2,
        2048 - SCREEN_H / 2
    );

    sceGuViewport(
        2048,
        2048,
        SCREEN_W,
        SCREEN_H
    );

    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_SMOOTH);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void beginFrame(void)
{
    sceGuStart(GU_DIRECT, displayList);

    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_DEPTH_TEST);

    sceGuClearColor(0xFF17131F);
    sceGuClearDepth(0);

    sceGuClear(
        GU_COLOR_BUFFER_BIT |
        GU_DEPTH_BUFFER_BIT
    );
}

static void endFrame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

/* ---------------------------------------------------------
   Animacoes
   --------------------------------------------------------- */

static int chooseSprite(void)
{
    switch (currentState) {
    case WALK:
        return 1 + (animationFrame & 1);

    case RUN:
        return 3 + (animationFrame & 3);

    case JUMP:
        return 7 + (animationFrame & 1);

    case FALL:
        return 9;

    case LAND:
        return 10;

    case ATTACK:
        return attackTime > 0.12f ? 11 : 12;

    case HURT:
        return 13;

    case MAGIC:
        return 14;

    default:
        return 0;
    }
}

/* ---------------------------------------------------------
   Cenario provisiorio
   --------------------------------------------------------- */

static void drawCave(void)
{
    drawRect(
        0, 210,
        SCREEN_W, 62,
        0xFF292436
    );

    drawRect(
        0, 235,
        SCREEN_W, 37,
        0xFF211D2C
    );

    drawRect(
        0, 208,
        SCREEN_W, 4,
        0xFF56506B
    );

    drawRect(
        25, 160,
        38, 48,
        0xFF302B40
    );

    drawRect(
        320, 145,
        55, 63,
        0xFF302B40
    );

    drawRect(
        400, 175,
        30, 33,
        0xFF302B40
    );
}

static void drawAttackEffect(void)
{
    if (currentState != ATTACK)
        return;

    drawRect(
        facing > 0 ? playerX + 48 : playerX - 24,
        playerY + 30,
        26,
        4,
        0xFF4D8DFF
    );

    drawRect(
        facing > 0 ? playerX + 50 : playerX - 20,
        playerY + 24,
        18,
        3,
        0xFFB7D8FF
    );
}

static void drawMagicEffect(void)
{
    if (currentState != MAGIC)
        return;

    drawRect(
        facing > 0 ? playerX + 57 : playerX - 18,
        playerY + 25,
        15,
        15,
        0xFFD7AAFF
    );

    drawRect(
        facing > 0 ? playerX + 61 : playerX - 14,
        playerY + 29,
        7,
        7,
        0xFF65BFFF
    );
}

/* ---------------------------------------------------------
   Atualizacao do jogador
   --------------------------------------------------------- */

static void updatePlayer(unsigned int buttons, float dt)
{
    int moving = 0;

    int running =
        (buttons & PSP_CTRL_RTRIGGER) != 0;

    if (buttons & PSP_CTRL_LEFT) {
        playerX -= running ? RUN_SPEED : WALK_SPEED;
        facing = -1;
        moving = 1;
    }

    if (buttons & PSP_CTRL_RIGHT) {
        playerX += running ? RUN_SPEED : WALK_SPEED;
        facing = 1;
        moving = 1;
    }

    if ((buttons & PSP_CTRL_CROSS) &&
        !(previousButtons & PSP_CTRL_CROSS) &&
        jumpCount < 2) {

        velocityY = JUMP_SPEED;
        jumpCount++;
        grounded = 0;

        currentState = JUMP;
    }

    if ((buttons & PSP_CTRL_SQUARE) &&
        !(previousButtons & PSP_CTRL_SQUARE)) {

        currentState = ATTACK;
        attackTime = 0.24f;
    }

    if ((buttons & PSP_CTRL_CIRCLE) &&
        !(previousButtons & PSP_CTRL_CIRCLE) &&
        magicUnlocked) {

        currentState = MAGIC;
        magicTime = 0.35f;
    }

    if (!grounded) {
        velocityY += GRAVITY;
        playerY += velocityY;

        if (playerY >= GROUND_Y) {
            playerY = GROUND_Y;
            velocityY = 0.0f;

            grounded = 1;
            jumpCount = 0;

            currentState = LAND;
            animationTime = 0.0f;
        }
    }

    if (attackTime > 0.0f) {
        attackTime -= dt;

        if (attackTime < 0.0f)
            attackTime = 0.0f;

        currentState = ATTACK;
    } else if (magicTime > 0.0f) {
        magicTime -= dt;

        if (magicTime < 0.0f)
            magicTime = 0.0f;

        currentState = MAGIC;
    } else if (!grounded) {
        currentState =
            velocityY < 0.0f ? JUMP : FALL;
    } else if (currentState == LAND &&
               animationTime < 0.12f) {
        /* Mantem a pose de aterrissagem brevemente. */
    } else {
        currentState = moving
            ? (running ? RUN : WALK)
            : IDLE;
    }

    animationTime += dt;

    if (animationTime >= 0.12f) {
        animationTime = 0.0f;
        animationFrame++;
    }

    if (playerX < 0.0f)
        playerX = 0.0f;

    if (playerX > SCREEN_W - PLAYER_W)
        playerX = SCREEN_W - PLAYER_W;
}

/* ---------------------------------------------------------
   Entrada principal
   --------------------------------------------------------- */

int main(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    for (int i = 0; i < SPRITE_COUNT; i++)
        resetTexture(&sprites[i]);

    initializeGraphics();

    for (int i = 0; i < SPRITE_COUNT; i++)
        loadSprite(i);

    SceCtrlData pad;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int buttons = pad.Buttons;
        const float deltaTime = 1.0f / 60.0f;

        if ((buttons & PSP_CTRL_START) &&
            !(previousButtons & PSP_CTRL_START)) {
            paused = !paused;
        }

        if (!paused)
            updatePlayer(buttons, deltaTime);

        beginFrame();

        drawCave();

        drawTexture(
            &sprites[chooseSprite()],
            playerX,
            playerY,
            PLAYER_W,
            PLAYER_H,
            facing < 0
        );

        drawAttackEffect();
        drawMagicEffect();

        if (paused) {
            drawRect(
                140, 95,
                200, 70,
                0xEE090812
            );

            drawRect(
                165, 125,
                150, 4,
                0xFFD9D2FF
            );
        }

        endFrame();

        previousButtons = buttons;
    }

    sceGuTerm();

    for (int i = 0; i < SPRITE_COUNT; i++) {
        if (sprites[i].pixels) {
            free(sprites[i].pixels);
            sprites[i].pixels = NULL;
        }
    }

    sceKernelExitGame();
    return 0;
}
