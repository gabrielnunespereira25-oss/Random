
/*
 * RANDOM - PSP / PSPSDK
 * Base inicial com 15 sprites PNG.
 *
 * Controles:
 * Direcional: mover
 * X: salto duplo
 * R + Direcional: correr
 * Quadrado: ataque de meia-lua
 * Círculo: magia
 * Start: pausar
 *
 * Requer libpng e zlib no ambiente PSPSDK.
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

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

typedef struct {
    unsigned int *pixels;
    int w, h, tw, th;
} Texture;

typedef struct {
    float u, v, x, y, z;
} Vertex;

static unsigned int __attribute__((aligned(16))) displayList[262144];
static Texture sprites[15];

static const char *spriteNames[15] = {
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
    IDLE, WALK, RUN, JUMP, FALL,
    LAND, ATTACK, HURT, MAGIC
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

static int nextPowerOfTwo(int n)
{
    int p = 1;
    while (p < n && p < 512)
        p <<= 1;
    return p;
}

/* Carrega PNG com libpng. */
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

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    png_init_io(png, file);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
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

    png_read_update_info(png, info);

    int textureW = nextPowerOfTwo(width);
    int textureH = nextPowerOfTwo(height);

    unsigned char *rgba =
        (unsigned char *)malloc(width * height * 4);

    png_bytep *rows =
        (png_bytep *)malloc(sizeof(png_bytep) * height);

    if (!rgba || !rows) {
        free(rgba);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    for (int y = 0; y < height; y++)
        rows[y] = rgba + y * width * 4;

    png_read_image(png, rows);
    png_read_end(png, NULL);

    t->pixels = (unsigned int *)memalign(
        16, textureW * textureH * 4);

    if (!t->pixels) {
        free(rgba);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    memset(t->pixels, 0, textureW * textureH * 4);

    for (int y = 0; y < height; y++) {
        memcpy(
            ((unsigned char *)t->pixels) + y * textureW * 4,
            rgba + y * width * 4,
            width * 4);
    }

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

static void drawTexture(
    Texture *texture,
    float x, float y,
    float width, float height,
    int flip)
{
    if (!texture->pixels)
        return;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);

    sceGuTexImage(
        0,
        texture->tw,
        texture->th,
        texture->tw,
        texture->pixels);

    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuColor(0xFFFFFFFF);

    Vertex *v = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex));

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
        2, 0, v);

    sceGuDisable(GU_TEXTURE_2D);
}

static void drawRect(
    float x, float y,
    float width, float height,
    unsigned int color)
{
    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);

    Vertex *v = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex));

    v[0].u = 0;
    v[0].v = 0;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].u = 0;
    v[1].v = 0;
    v[1].x = x + width;
    v[1].y = y + height;
    v[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, 0, v);
}

static void initializeGraphics(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(
        GU_PSM_8888, (void *)0, BUF_W);

    sceGuDispBuffer(
        SCREEN_W, SCREEN_H,
        (void *)0x88000, BUF_W);

    sceGuDepthBuffer(
        (void *)0x110000, BUF_W);

    sceGuOffset(
        2048 - SCREEN_W / 2,
        2048 - SCREEN_H / 2);

    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void beginFrame(void)
{
    sceGuStart(GU_DIRECT, displayList);
    sceGuClearColor(0xFF17131F);
    sceGuClearDepth(0);
    sceGuClear(
        GU_COLOR_BUFFER_BIT |
        GU_DEPTH_BUFFER_BIT);
}

static void endFrame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

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

int main(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initializeGraphics();

    for (int i = 0; i < 15; i++) {
        char path[160];

        snprintf(
            path, sizeof(path),
            "ms0:/PSP/GAME/RANDOM/assets/sprites/%s",
            spriteNames[i]);

        loadPNG(path, &sprites[i]);
    }

    SceCtrlData pad;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int buttons = pad.Buttons;
        float deltaTime = 1.0f / 60.0f;

        if ((buttons & PSP_CTRL_START) &&
            !(previousButtons & PSP_CTRL_START)) {
            paused = !paused;
        }

        if (!paused) {
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

                if (velocityY > 0)
                    currentState = FALL;

                if (playerY >= GROUND_Y) {
                    playerY = GROUND_Y;
                    velocityY = 0;
                    grounded = 1;
                    jumpCount = 0;
                    currentState = LAND;
                    animationTime = 0;
                }
            }

            if (attackTime > 0) {
                attackTime -= deltaTime;
                currentState = ATTACK;
            } else if (magicTime > 0) {
                magicTime -= deltaTime;
                currentState = MAGIC;
            } else if (!grounded) {
                currentState =
                    velocityY < 0 ? JUMP : FALL;
            } else if (currentState == LAND &&
                       animationTime < 0.12f) {
                /* Mantém a pose de aterrissagem brevemente. */
            } else {
                currentState = moving
                    ? (running ? RUN : WALK)
                    : IDLE;
            }

            animationTime += deltaTime;

            if (animationTime > 0.12f) {
                animationTime = 0;
                animationFrame++;
            }

            if (playerX < 0)
                playerX = 0;

            if (playerX > SCREEN_W - PLAYER_W)
                playerX = SCREEN_W - PLAYER_W;
        }

        beginFrame();

        /* Cenário provisório de caverna. */
        drawRect(0, 210, SCREEN_W, 62, 0xFF292436);
        drawRect(0, 235, SCREEN_W, 37, 0xFF211D2C);
        drawRect(0, 208, SCREEN_W, 4, 0xFF56506B);

        drawRect(25, 160, 38, 48, 0xFF302B40);
        drawRect(320, 145, 55, 63, 0xFF302B40);
        drawRect(400, 175, 30, 33, 0xFF302B40);

        drawTexture(
            &sprites[chooseSprite()],
            playerX, playerY,
            PLAYER_W, PLAYER_H,
            facing < 0);

        /* Efeito visual provisório do ataque. */
        if (currentState == ATTACK) {
            drawRect(
                facing > 0 ? playerX + 48 : playerX - 24,
                playerY + 30,
                26, 4, 0xFF4D8DFF);

            drawRect(
                facing > 0 ? playerX + 50 : playerX - 20,
                playerY + 24,
                18, 3, 0xFFB7D8FF);
        }

        /* Efeito visual provisório da magia. */
        if (currentState == MAGIC) {
            drawRect(
                facing > 0 ? playerX + 57 : playerX - 18,
                playerY + 25,
                15, 15, 0xFFD7AAFF);

            drawRect(
                facing > 0 ? playerX + 61 : playerX - 14,
                playerY + 29,
                7, 7, 0xFF65BFFF);
        }

        if (paused) {
            drawRect(140, 95, 200, 70, 0xEE090812);
            drawRect(165, 125, 150, 4, 0xFFD9D2FF);
        }

        endFrame();
        previousButtons = buttons;
    }

    sceGuTerm();
    return 0;
}
