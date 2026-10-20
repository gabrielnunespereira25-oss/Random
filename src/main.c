
/*
 * RANDOM - PSP / PSPSDK
 * Sistema de sprites PNG corrigido
 *
 * Controles:
 * Direcional: mover
 * R + Direcional: correr
 * X: salto duplo
 * Quadrado: ataque
 * Circulo: magia
 * Start: pausar
 *
 * Requer PSPSDK, libpng e zlib.
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

#define SW 480
#define SH 272
#define BW 512

#define SPRITE_COUNT 16

#define PLAYER_W 76.0f
#define PLAYER_H 82.0f
#define GROUND_Y 210.0f
#define PLAYER_FLOOR (GROUND_Y - PLAYER_H)

#define GRAVITY 0.42f
#define JUMP_SPEED -7.6f
#define WALK_SPEED 2.3f
#define RUN_SPEED 3.8f

typedef struct {
    unsigned int *pixels;
    int w, h;
    int tw, th;
} Texture;

typedef struct {
    float u, v;
    float x, y, z;
} TexVertex;

typedef struct {
    float x, y, z;
} RectVertex;

static unsigned int __attribute__((aligned(16)))
displayList[262144];

static Texture sprites[SPRITE_COUNT];

static const char *spriteNames[SPRITE_COUNT] = {
    "idle_1.png",
    "idle_2.png",
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

static float px = 80.0f;
static float py = PLAYER_FLOOR;
static float vy = 0.0f;

static int facing = 1;
static int grounded = 1;
static int jumpCount = 0;
static int paused = 0;
static int loadedCount = 0;

static int state = IDLE;
static int animFrame = 0;

static float animTimer = 0.0f;
static float attackTimer = 0.0f;
static float magicTimer = 0.0f;
static float landTimer = 0.0f;

static unsigned int previousButtons = 0;

/* -------------------------------------------------
   TEXTURAS
   ------------------------------------------------- */

static int powerOfTwo(int n)
{
    int p = 1;
    while (p < n && p < 512)
        p <<= 1;
    return p;
}

static void clearTexture(Texture *t)
{
    t->pixels = NULL;
    t->w = t->h = 0;
    t->tw = t->th = 0;
}

static void releaseTexture(Texture *t)
{
    if (t->pixels)
        free(t->pixels);

    clearTexture(t);
}

static int loadPNG(const char *path, Texture *t)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;

    unsigned char sig[8];

    if (fread(sig, 1, 8, fp) != 8 ||
        png_sig_cmp(sig, 0, 8)) {
        fclose(fp);
        return 0;
    }

    png_structp png = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png) {
        fclose(fp);
        return 0;
    }

    png_infop info = png_create_info_struct(png);

    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return 0;
    }

    unsigned char *rgba = NULL;
    png_bytep *rows = NULL;
    unsigned int *pixels = NULL;

    /*
     * setjmp permite recuperar erros internos do libpng.
     */
    if (setjmp(png_jmpbuf(png))) {
        if (pixels) free(pixels);
        if (rows) free(rows);
        if (rgba) free(rgba);

        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    int w = (int)png_get_image_width(png, info);
    int h = (int)png_get_image_height(png, info);

    int colorType = png_get_color_type(png, info);
    int bitDepth = png_get_bit_depth(png, info);

    if (w < 1 || h < 1 || w > 512 || h > 512) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
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

    if (png_get_channels(png, info) != 4 ||
        png_get_bit_depth(png, info) != 8) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    png_size_t rowBytes = png_get_rowbytes(png, info);

    if (rowBytes < (png_size_t)w * 4) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    int tw = powerOfTwo(w);
    int th = powerOfTwo(h);

    rgba = (unsigned char *)malloc((size_t)rowBytes * h);
    rows = (png_bytep *)malloc(sizeof(png_bytep) * h);

    if (!rgba || !rows) {
        if (rgba) free(rgba);
        if (rows) free(rows);

        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    for (int y = 0; y < h; y++)
        rows[y] = rgba + (size_t)y * rowBytes;

    png_read_image(png, rows);
    png_read_end(png, NULL);

    pixels = (unsigned int *)memalign(
        16, (size_t)tw * th * 4);

    if (!pixels) {
        free(rgba);
        free(rows);

        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    memset(pixels, 0, (size_t)tw * th * 4);

    for (int y = 0; y < h; y++) {
        memcpy(
            (unsigned char *)pixels + (size_t)y * tw * 4,
            rgba + (size_t)y * rowBytes,
            (size_t)w * 4);
    }

    free(rgba);
    free(rows);

    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    t->pixels = pixels;
    t->w = w;
    t->h = h;
    t->tw = tw;
    t->th = th;

    sceKernelDcacheWritebackAll();

    return 1;
}

/* Procura os PNGs em diferentes pastas. */
static int loadSprite(int i)
{
    char path[256];

    if (i < 0 || i >= SPRITE_COUNT)
        return 0;

    clearTexture(&sprites[i]);

    snprintf(path, sizeof(path),
        "ms0:/PSP/GAME/RANDOM/assets/sprites/%s",
        spriteNames[i]);

    if (loadPNG(path, &sprites[i]))
        return 1;

    snprintf(path, sizeof(path),
        "ef0:/PSP/GAME/RANDOM/assets/sprites/%s",
        spriteNames[i]);

    if (loadPNG(path, &sprites[i]))
        return 1;

    snprintf(path, sizeof(path),
        "ms0:/PSP/GAME/RANDOM/%s",
        spriteNames[i]);

    if (loadPNG(path, &sprites[i]))
        return 1;

    snprintf(path, sizeof(path),
        "assets/sprites/%s",
        spriteNames[i]);

    if (loadPNG(path, &sprites[i]))
        return 1;

    return 0;
}

/* -------------------------------------------------
   DESENHO 2D
   ------------------------------------------------- */

static void rectangle(
    float x, float y, float w, float h,
    unsigned int color)
{
    if (w <= 0 || h <= 0)
        return;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);

    RectVertex *v = (RectVertex *)
        sceGuGetMemory(2 * sizeof(RectVertex));

    if (!v)
        return;

    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, 0, v);
}

/* Personagem temporario se o PNG nao carregar. */
static void drawFallback(float x, float y, int flip)
{
    float w = PLAYER_W;
    float h = PLAYER_H;
    float cx = x + w * 0.5f;

    rectangle(cx - 12, y + 55, 10, 25, 0xFF29223B);
    rectangle(cx + 3,  y + 55, 10, 25, 0xFF29223B);
    rectangle(cx - 17, y + 25, 34, 34, 0xFF34304F);
    rectangle(cx - 17, y + 50, 34, 5, 0xFFFF6B35);
    rectangle(cx - 14, y + 5, 28, 23, 0xFFB87552);
    rectangle(cx - 15, y + 3, 30, 7, 0xFF17131F);

    if (flip)
        rectangle(cx - 9, y + 13, 5, 4, 0xFFFFFFFF);
    else
        rectangle(cx + 4, y + 13, 5, 4, 0xFFFFFFFF);
}

static void drawTexture(
    Texture *t, float x, float y,
    float w, float h, int flip)
{
    if (!t || !t->pixels || t->w <= 0 || t->h <= 0) {
        drawFallback(x, y, flip);
        return;
    }

    float sx = w / (float)t->w;
    float sy = h / (float)t->h;
    float scale = sx < sy ? sx : sy;

    float dw = t->w * scale;
    float dh = t->h * scale;

    x += (w - dw) * 0.5f;
    y += h - dh;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, t->tw, t->th, t->tw, t->pixels);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuColor(0xFFFFFFFF);

    TexVertex *v = (TexVertex *)
        sceGuGetMemory(2 * sizeof(TexVertex));

    if (!v) {
        sceGuDisable(GU_TEXTURE_2D);
        return;
    }

    v[0].u = flip ? (float)t->w : 0.0f;
    v[0].v = 0.0f;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].u = flip ? 0.0f : (float)t->w;
    v[1].v = (float)t->h;
    v[1].x = x + dw;
    v[1].y = y + dh;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_32BITF |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v);

    sceGuDisable(GU_TEXTURE_2D);
}

/* -------------------------------------------------
   GRAFICOS PSP
   ------------------------------------------------- */

static void initGraphics(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BW);
    sceGuDispBuffer(SW, SH, (void *)0x88000, BW);
    sceGuDepthBuffer((void *)0x110000, BW);

    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);

    sceGuScissor(0, 0, SW, SH);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);

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
    sceGuClear(GU_COLOR_BUFFER_BIT |
               GU_DEPTH_BUFFER_BIT);
}

static void endFrame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

/* -------------------------------------------------
   ANIMACAO
   ------------------------------------------------- */

static int chooseSprite(void)
{
    switch (state) {
        case IDLE:
            return animFrame % 2;

        case WALK:
            return 2 + animFrame % 2;

        case RUN:
            return 4 + animFrame % 4;

        case JUMP:
            return 8 + animFrame % 2;

        case FALL:
            return 10;

        case LAND:
            return 11;

        case ATTACK:
            return attackTimer > 0.12f ? 12 : 13;

        case HURT:
            return 14;

        case MAGIC:
            return 15;

        default:
            return 0;
    }
}

/* -------------------------------------------------
   CAVERNA DE TESTE
   ------------------------------------------------- */

static void drawCave(void)
{
    rectangle(0, 0, SW, SH, 0xFF17131F);

    rectangle(0, GROUND_Y, SW, SH - GROUND_Y,
              0xFF292436);

    rectangle(0, 235, SW, 37, 0xFF211D2C);
    rectangle(0, GROUND_Y, SW, 4, 0xFF56506B);

    rectangle(25, 160, 38, 50, 0xFF302B40);
    rectangle(320, 145, 55, 65, 0xFF302B40);
    rectangle(400, 175, 30, 35, 0xFF302B40);

    /* Barra de diagnostico: sprites carregados. */
    rectangle(8, 8, 100, 8, 0xFF383348);

    if (loadedCount > 0) {
        rectangle(8, 8,
            100.0f * loadedCount / SPRITE_COUNT,
            8, 0xFF55DDAA);
    }
}

static void drawEffects(void)
{
    if (state == ATTACK) {
        float x = facing > 0 ? px + PLAYER_W - 4 : px - 22;

        rectangle(x, py + 30, 26, 4, 0xFF4D8DFF);
        rectangle(x + 4, py + 24, 18, 3, 0xFFB7D8FF);
    }

    if (state == MAGIC) {
        float x = facing > 0 ? px + PLAYER_W - 2 : px - 18;

        rectangle(x, py + 25, 15, 15, 0xFFD7AAFF);
        rectangle(x + 4, py + 29, 7, 7, 0xFF65BFFF);
    }
}

/* -------------------------------------------------
   CONTROLES E FISICA
   ------------------------------------------------- */

static void updatePlayer(unsigned int buttons, float dt)
{
    int moving = 0;
    int running = (buttons & PSP_CTRL_RTRIGGER) != 0;

    if (buttons & PSP_CTRL_LEFT) {
        px -= running ? RUN_SPEED : WALK_SPEED;
        facing = -1;
        moving = 1;
    }

    if (buttons & PSP_CTRL_RIGHT) {
        px += running ? RUN_SPEED : WALK_SPEED;
        facing = 1;
        moving = 1;
    }

    if ((buttons & PSP_CTRL_CROSS) &&
        !(previousButtons & PSP_CTRL_CROSS) &&
        jumpCount < 2) {

        vy = JUMP_SPEED;
        jumpCount++;
        grounded = 0;
        state = JUMP;
        animFrame = 0;
        animTimer = 0;
    }

    if ((buttons & PSP_CTRL_SQUARE) &&
        !(previousButtons & PSP_CTRL_SQUARE)) {

        attackTimer = 0.24f;
        state = ATTACK;
        animFrame = 0;
    }

    if ((buttons & PSP_CTRL_CIRCLE) &&
        !(previousButtons & PSP_CTRL_CIRCLE)) {

        magicTimer = 0.35f;
        state = MAGIC;
        animFrame = 0;
    }

    if (!grounded) {
        vy += GRAVITY;
        py += vy;

        if (py >= PLAYER_FLOOR) {
            py = PLAYER_FLOOR;
            vy = 0;
            grounded = 1;
            jumpCount = 0;
            landTimer = 0.12f;
        }
    }

    if (attackTimer > 0) {
        attackTimer -= dt;
        if (attackTimer < 0)
            attackTimer = 0;
    }

    if (magicTimer > 0) {
        magicTimer -= dt;
        if (magicTimer < 0)
            magicTimer = 0;
    }

    if (landTimer > 0) {
        landTimer -= dt;
        if (landTimer < 0)
            landTimer = 0;
    }

    if (attackTimer > 0) {
        state = ATTACK;
    }
    else if (magicTimer > 0) {
        state = MAGIC;
    }
    else if (!grounded) {
        state = vy < 0 ? JUMP : FALL;
    }
    else if (landTimer > 0) {
        state = LAND;
    }
    else if (moving) {
        state = running ? RUN : WALK;
    }
    else {
        state = IDLE;
    }

    animTimer += dt;

    if (animTimer >= 0.12f) {
        animTimer = 0;
        animFrame++;
    }

    if (px < 0)
        px = 0;

    if (px > SW - PLAYER_W)
        px = SW - PLAYER_W;
}

/* -------------------------------------------------
   MAIN
   ------------------------------------------------- */

int main(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    for (int i = 0; i < SPRITE_COUNT; i++)
        clearTexture(&sprites[i]);

    initGraphics();

    loadedCount = 0;

    for (int i = 0; i < SPRITE_COUNT; i++) {
        if (loadSprite(i))
            loadedCount++;
    }

    while (1) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int buttons = pad.Buttons;
        const float dt = 1.0f / 60.0f;

        if ((buttons & PSP_CTRL_START) &&
            !(previousButtons & PSP_CTRL_START)) {
            paused = !paused;
        }

        if (!paused)
            updatePlayer(buttons, dt);

        beginFrame();

        drawCave();

        int spriteIndex = chooseSprite();

        drawTexture(
            &sprites[spriteIndex],
            px, py,
            PLAYER_W, PLAYER_H,
            facing < 0);

        drawEffects();

        if (paused) {
            rectangle(120, 90, 240, 80, 0xEE090812);
            rectangle(165, 125, 150, 4, 0xFFD9D2FF);
        }

        endFrame();

        previousButtons = buttons;
    }

    for (int i = 0; i < SPRITE_COUNT; i++)
        releaseTexture(&sprites[i]);

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
