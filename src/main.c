
/*
 * RANDOM - PSP / PSPSDK
 * Player com 16 sprites PNG.
 *
 * Controles:
 * Direcional: mover
 * R + Direcional: correr
 * X: salto duplo
 * Quadrado: ataque
 * Circulo: magia
 * Start: pausar
 *
 * Sprites:
 * assets/sprites/idle_1.png
 * assets/sprites/idle_2.png
 * assets/sprites/walk_1.png
 * assets/sprites/walk_2.png
 * assets/sprites/run_1.png ... run_4.png
 * assets/sprites/jump_1.png
 * assets/sprites/jump_2.png
 * assets/sprites/fall.png
 * assets/sprites/landing.png
 * assets/sprites/attack_1.png
 * assets/sprites/attack_2.png
 * assets/sprites/hurt.png
 * assets/sprites/magic.png
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
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512

#define GRAVITY       0.42f
#define JUMP_SPEED   -7.6f
#define WALK_SPEED    2.3f
#define RUN_SPEED     3.8f

#define PLAYER_W     76.0f
#define PLAYER_H     82.0f
#define GROUND_TOP  210.0f
#define PLAYER_GROUND (GROUND_TOP - PLAYER_H)

#define SPRITE_COUNT 16

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

static float playerX = 80.0f;
static float playerY = PLAYER_GROUND;
static float velocityY = 0.0f;

static float animationTime = 0.0f;
static float attackTime = 0.0f;
static float magicTime = 0.0f;
static float hurtTime = 0.0f;
static float landTime = 0.0f;

static int facing = 1;
static int grounded = 1;
static int jumpCount = 0;
static int paused = 0;
static int magicUnlocked = 1;

static int currentState = IDLE;
static int animationFrame = 0;
static unsigned int previousButtons = 0;
static int spritesLoaded = 0;

/* --------------------------------------------------
   TEXTURAS E PNG
   -------------------------------------------------- */

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

static void freeTexture(Texture *t)
{
    if (t->pixels)
        free(t->pixels);

    resetTexture(t);
}

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

    unsigned char *rgba = NULL;
    png_bytep *rows = NULL;
    unsigned int *pixels = NULL;

    if (setjmp(png_jmpbuf(png))) {
        if (pixels) free(pixels);
        if (rows) free(rows);
        if (rgba) free(rgba);

        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    png_init_io(png, file);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    int w = (int)png_get_image_width(png, info);
    int h = (int)png_get_image_height(png, info);

    int colorType = png_get_color_type(png, info);
    int bitDepth = png_get_bit_depth(png, info);

    if (w <= 0 || h <= 0 || w > 512 || h > 512) {
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

    if (png_get_channels(png, info) != 4 ||
        png_get_bit_depth(png, info) != 8) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    png_size_t rowBytes = png_get_rowbytes(png, info);

    if (rowBytes < (png_size_t)w * 4) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return 0;
    }

    int tw = nextPowerOfTwo(w);
    int th = nextPowerOfTwo(h);

    rgba = (unsigned char *)malloc((size_t)rowBytes * h);
    rows = (png_bytep *)malloc(sizeof(png_bytep) * h);

    if (!rgba || !rows) {
        free(rgba);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
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
        fclose(file);
        return 0;
    }

    memset(pixels, 0, (size_t)tw * th * 4);

    for (int y = 0; y < h; y++) {
        memcpy(
            (unsigned char *)pixels +
                (size_t)y * tw * 4,
            rgba + (size_t)y * rowBytes,
            (size_t)w * 4);
    }

    free(rgba);
    free(rows);

    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);

    t->pixels = pixels;
    t->w = w;
    t->h = h;
    t->tw = tw;
    t->th = th;

    sceKernelDcacheWritebackAll();

    return 1;
}

/* Caminhos alternativos para localizar os PNGs. */
static int loadSprite(int i)
{
    char path[192];

    resetTexture(&sprites[i]);

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
        "assets/sprites/%s", spriteNames[i]);

    if (loadPNG(path, &sprites[i]))
        return 1;

    snprintf(path, sizeof(path),
        "ms0:/PSP/GAME/RANDOM/%s", spriteNames[i]);

    if (loadPNG(path, &sprites[i]))
        return 1;

    return 0;
}

/* --------------------------------------------------
   DESENHO
   -------------------------------------------------- */

static void drawRect(
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

/* Usado somente se um PNG não carregar. */
static void drawFallback(
    float x, float y, float w, float h, int flip)
{
    float cx = x + w * 0.5f;

    drawRect(cx-w*.20f,y+h*.67f,w*.16f,h*.27f,0xFF29223B);
    drawRect(cx+w*.04f,y+h*.67f,w*.16f,h*.27f,0xFF29223B);

    drawRect(cx-w*.24f,y+h*.32f,w*.46f,h*.39f,0xFF34304F);
    drawRect(cx-w*.23f,y+h*.56f,w*.46f,h*.06f,0xFFFF6B35);

    drawRect(
        flip ? cx-w*.34f : cx+w*.17f,
        y+h*.39f,w*.17f,h*.25f,0xFF34304F);

    drawRect(cx-w*.22f,y+h*.07f,w*.44f,h*.28f,0xFFB87552);
    drawRect(cx-w*.24f,y+h*.06f,w*.48f,h*.09f,0xFF17131F);

    drawRect(
        flip ? cx-w*.16f : cx+w*.08f,
        y+h*.18f,w*.07f,h*.04f,0xFFFFFFFF);
}

static void drawTexture(
    Texture *t, float x, float y,
    float w, float h, int flip)
{
    if (!t || !t->pixels || t->w <= 0 || t->h <= 0) {
        drawFallback(x, y, w, h, flip);
        return;
    }

    float aspect = (float)t->w / (float)t->h;
    float dw = h * aspect;
    float dh = h;

    if (dw > w) {
        dw = w;
        dh = w / aspect;
    }

    x += (w - dw) * 0.5f;
    y += h - dh;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);

    sceGuTexImage(
        0, t->tw, t->th, t->tw, t->pixels);

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

    v[0].u = flip ? (float)t->w : 0;
    v[0].v = 0;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].u = flip ? 0 : (float)t->w;
    v[1].v = (float)t->h;
    v[1].x = x + dw;
    v[1].y = y + dh;
    v[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_32BITF |
        GU_VERTEX_32BITF |
        GU_TRANSFORM_2D,
        2, 0, v);

    sceGuDisable(GU_TEXTURE_2D);
}

/* --------------------------------------------------
   VIDEO
   -------------------------------------------------- */

static void initializeGraphics(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, displayList);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void *)0x88000, BUF_W);
    sceGuDepthBuffer((void *)0x110000, BUF_W);

    sceGuOffset(2048-SCREEN_W/2, 2048-SCREEN_H/2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);

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
        GU_DEPTH_BUFFER_BIT);
}

static void endFrame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

/* --------------------------------------------------
   ANIMACOES
   -------------------------------------------------- */

static int chooseSprite(void)
{
    switch (currentState) {
        case IDLE:
            return animationFrame % 2;

        case WALK:
            return 2 + (animationFrame % 2);

        case RUN:
            return 4 + (animationFrame % 4);

        case JUMP:
            return 8 + (animationFrame % 2);

        case FALL:
            return 10;

        case LAND:
            return 11;

        case ATTACK:
            return attackTime > 0.12f ? 12 : 13;

        case HURT:
            return 14;

        case MAGIC:
            return 15;

        default:
            return 0;
    }
}

/* --------------------------------------------------
   CAVERNA DE TESTE
   -------------------------------------------------- */

static void drawCave(void)
{
    /* Fundo */
    drawRect(0, 0, SCREEN_W, SCREEN_H, 0xFF17131F);

    /* Plataformas e chao */
    drawRect(0, GROUND_TOP, SCREEN_W,
        SCREEN_H-GROUND_TOP, 0xFF292436);

    drawRect(0, 235, SCREEN_W, 37, 0xFF211D2C);
    drawRect(0, GROUND_TOP, SCREEN_W, 4, 0xFF56506B);

    /* Pedras */
    drawRect(25, 160, 38, 50, 0xFF302B40);
    drawRect(320, 145, 55, 65, 0xFF302B40);
    drawRect(400, 175, 30, 35, 0xFF302B40);

    /* Indicador de PNGs carregados */
    drawRect(8, 8, 100, 8, 0xFF383348);

    if (spritesLoaded > 0) {
        drawRect(8, 8,
            100.0f * spritesLoaded / SPRITE_COUNT,
            8, 0xFF55DDAA);
    }
}

/* Efeitos da magia e do ataque desenhados em C. */
static void drawEffects(void)
{
    if (currentState == ATTACK) {
        float sx = facing > 0
            ? playerX + PLAYER_W - 4
            : playerX - 22;

        drawRect(sx, playerY + 30, 26, 4, 0xFF4D8DFF);
        drawRect(sx + 4, playerY + 24, 18, 3, 0xFFB7D8FF);
    }

    if (currentState == MAGIC) {
        float sx = facing > 0
            ? playerX + PLAYER_W - 2
            : playerX - 18;

        drawRect(sx, playerY + 25, 15, 15, 0xFFD7AAFF);
        drawRect(sx + 4, playerY + 29, 7, 7, 0xFF65BFFF);
    }
}

/* --------------------------------------------------
   JOGADOR E CONTROLES
   -------------------------------------------------- */

static void updatePlayer(unsigned int buttons, float dt)
{
    int moving = 0;
    int running = (buttons & PSP_CTRL_RTRIGGER) != 0;

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

    /* Salto duplo */
    if ((buttons & PSP_CTRL_CROSS) &&
        !(previousButtons & PSP_CTRL_CROSS) &&
        jumpCount < 2) {

        velocityY = JUMP_SPEED;
        jumpCount++;
        grounded = 0;
        currentState = JUMP;
        animationTime = 0;
    }

    /* Ataque */
    if ((buttons & PSP_CTRL_SQUARE) &&
        !(previousButtons & PSP_CTRL_SQUARE)) {

        currentState = ATTACK;
        attackTime = 0.24f;
        animationFrame = 0;
    }

    /* Magia */
    if ((buttons & PSP_CTRL_CIRCLE) &&
        !(previousButtons & PSP_CTRL_CIRCLE) &&
        magicUnlocked) {

        currentState = MAGIC;
        magicTime = 0.35f;
        animationFrame = 0;
    }

    /* Movimento vertical */
    if (!grounded) {
        velocityY += GRAVITY;
        playerY += velocityY;

        if (playerY >= PLAYER_GROUND) {
            playerY = PLAYER_GROUND;
            velocityY = 0;
            grounded = 1;
            jumpCount = 0;

            currentState = LAND;
            landTime = 0.12f;
            animationTime = 0;
        }
    }

    /* Temporizador do ataque */
    if (attackTime > 0) {
        attackTime -= dt;

        if (attackTime < 0)
            attackTime = 0;
    }

    /* Temporizador da magia */
    if (magicTime > 0) {
        magicTime -= dt;

        if (magicTime < 0)
            magicTime = 0;
    }

    /* Temporizador da aterrissagem */
    if (landTime > 0) {
        landTime -= dt;

        if (landTime < 0)
            landTime = 0;
    }

    /* Prioridade das animacoes */
    if (attackTime > 0) {
        currentState = ATTACK;
    }
    else if (magicTime > 0) {
        currentState = MAGIC;
    }
    else if (!grounded) {
        currentState = velocityY < 0 ? JUMP : FALL;
    }
    else if (landTime > 0) {
        currentState = LAND;
    }
    else if (moving) {
        currentState = running ? RUN : WALK;
    }
    else {
        currentState = IDLE;
    }

    /* Avanco dos quadros de animacao */
    animationTime += dt;

    if (animationTime >= 0.12f) {
        animationTime = 0;
        animationFrame++;
    }

    /* Limites da tela */
    if (playerX < 0)
        playerX = 0;

    if (playerX > SCREEN_W - PLAYER_W)
        playerX = SCREEN_W - PLAYER_W;
}

/* --------------------------------------------------
   INICIO
   -------------------------------------------------- */

int main(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    for (int i = 0; i < SPRITE_COUNT; i++)
        resetTexture(&sprites[i]);

    initializeGraphics();

    /* Carrega todos os PNGs */
    spritesLoaded = 0;

    for (int i = 0; i < SPRITE_COUNT; i++) {
        if (loadSprite(i))
            spritesLoaded++;
    }

    SceCtrlData pad;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int buttons = pad.Buttons;
        const float dt = 1.0f / 60.0f;

        /* Pausa */
        if ((buttons & PSP_CTRL_START) &&
            !(previousButtons & PSP_CTRL_START)) {
            paused = !paused;
        }

        if (!paused)
            updatePlayer(buttons, dt);

        beginFrame();

        drawCave();

        /* Desenha o sprite correspondente ao estado atual */
        int spriteIndex = chooseSprite();

        drawTexture(
            &sprites[spriteIndex],
            playerX,
            playerY,
            PLAYER_W,
            PLAYER_H,
            facing < 0);

        drawEffects();

        if (paused) {
            drawRect(120, 90, 240, 80, 0xEE090812);
            drawRect(165, 125, 150, 4, 0xFFD9D2FF);
        }

        endFrame();

        previousButtons = buttons;
    }

    for (int i = 0; i < SPRITE_COUNT; i++)
        freeTexture(&sprites[i]);

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
