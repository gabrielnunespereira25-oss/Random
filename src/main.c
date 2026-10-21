
/*
 * RANDOM - PSP / PPSSPP
 * Menu, personagem PNG, animacoes, ataque, magia e pausa.
 *
 * Controles:
 * Direcional: mover
 * X: pular
 * R + Direcional: correr
 * Quadrado: atacar
 * Circulo: magia
 * Start: pausar / continuar
 *
 * Requer PSPSDK com libpng e zlib.
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
#include <math.h>

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512
#define BUF_H 272

#define SPRITE_COUNT 18
#define TEX_MAX 512

#define WHITE  0xFFFFFFFF
#define BLACK  0xFF080B14
#define BLUE   0xFFFFB040
#define CYAN   0xFFFFE080
#define RED    0xFF3030E8
#define GREEN  0xFF40E890
#define GREY   0xFF808898

typedef struct {
    unsigned int *pixels;
    int width;
    int height;
    int loaded;
} Sprite;

static const char *spriteNames[SPRITE_COUNT] = {
    "idle_1.png",
    "idle_2.png",
    "walk_1.png",
    "walk_2.png",
    "walk_3.png",
    "walk_4.png",
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
    IDLE_1, IDLE_2,
    WALK_1, WALK_2, WALK_3, WALK_4,
    RUN_1, RUN_2, RUN_3, RUN_4,
    JUMP_1, JUMP_2, FALL, LANDING,
    ATTACK_1, ATTACK_2, HURT, MAGIC
};

static Sprite sprites[SPRITE_COUNT];

static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned int __attribute__((aligned(16))) framebuffer[BUF_W * BUF_H];

static int gameState = 0; /* 0 menu, 1 jogo, 2 pausa */
static int menuChoice = 0;
static int loadedCount = 0;

static float playerX = 100.0f;
static float playerY = 190.0f;
static float playerVY = 0.0f;
static float cameraX = 0.0f;

static int facing = 1;
static int grounded = 1;
static int attacking = 0;
static int magicActive = 0;
static int hurtTimer = 0;

static int animFrame = 0;
static int animTimer = 0;
static int gameFrame = 0;
static int hp = 5;

static SceCtrlData pad;
static unsigned int oldButtons = 0;

/* Fonte bitmap 5x7 para letras maiusculas, numeros e simbolos. */
static const unsigned char font[][7] = {
    {14,17,17,31,17,17,17}, /* A */
    {30,17,17,30,17,17,30}, /* B */
    {14,17,16,16,16,17,14}, /* C */
    {30,17,17,17,17,17,30}, /* D */
    {31,16,16,30,16,16,31}, /* E */
    {31,16,16,30,16,16,16}, /* F */
    {14,17,16,23,17,17,15}, /* G */
    {17,17,17,31,17,17,17}, /* H */
    {14,4,4,4,4,4,14},      /* I */
    {7,2,2,2,18,18,12},     /* J */
    {17,18,20,24,20,18,17}, /* K */
    {16,16,16,16,16,16,31}, /* L */
    {17,27,21,21,17,17,17}, /* M */
    {17,25,21,19,17,17,17}, /* N */
    {14,17,17,17,17,17,14}, /* O */
    {30,17,17,30,16,16,16}, /* P */
    {14,17,17,17,21,18,13}, /* Q */
    {30,17,17,30,20,18,17}, /* R */
    {15,16,16,14,1,1,30},   /* S */
    {31,4,4,4,4,4,4},       /* T */
    {17,17,17,17,17,17,14}, /* U */
    {17,17,17,17,17,10,4},  /* V */
    {17,17,17,21,21,21,10}, /* W */
    {17,17,10,4,10,17,17},  /* X */
    {17,17,10,4,4,4,4},     /* Y */
    {31,1,2,4,8,16,31},      /* Z */
    {14,17,19,21,25,17,14}, /* 0 */
    {4,12,4,4,4,4,14},      /* 1 */
    {14,17,1,2,4,8,31},     /* 2 */
    {30,1,1,14,1,1,30},     /* 3 */
    {2,6,10,18,31,2,2},     /* 4 */
    {31,16,16,30,1,1,30},   /* 5 */
    {14,16,16,30,17,17,14}, /* 6 */
    {31,1,2,4,8,8,8},       /* 7 */
    {14,17,17,14,17,17,14}, /* 8 */
    {14,17,17,15,1,1,14},   /* 9 */
    {0,0,0,0,0,0,0},        /* espaco */
    {0,4,4,0,4,4,0},        /* : */
    {0,0,0,0,0,4,8},        /* , */
    {0,0,0,31,0,0,0},       /* - */
    {0,0,0,0,0,12,12},      /* . */
    {4,4,4,4,4,0,4}         /* ! */
};

static int glyphIndex(char c)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + c - '0';
    if (c == ' ') return 36;
    if (c == ':') return 37;
    if (c == ',') return 38;
    if (c == '-') return 39;
    if (c == '.') return 40;
    if (c == '!') return 41;
    return 36;
}

static void putPixel(int x, int y, unsigned int color)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H)
        return;
    framebuffer[y * BUF_W + x] = color;
}

static void drawRect(int x, int y, int w, int h,
                     unsigned int color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > SCREEN_W ? SCREEN_W : x + w;
    int y1 = y + h > SCREEN_H ? SCREEN_H : y + h;

    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            framebuffer[yy * BUF_W + xx] = color;
}

static void drawText(int x, int y, const char *text,
                     int scale, unsigned int color)
{
    int startX = x;

    while (*text) {
        if (*text == '\n') {
            x = startX;
            y += 9 * scale;
            text++;
            continue;
        }

        int g = glyphIndex(*text++);
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (font[g][row] & (1 << (4 - col)))
                    drawRect(x + col * scale,
                             y + row * scale,
                             scale, scale, color);
            }
        }
        x += 6 * scale;
    }
}

static void clearScreen(unsigned int color)
{
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < BUF_W; x++)
            framebuffer[y * BUF_W + x] = color;
}

static void freeSprite(Sprite *s)
{
    if (s->pixels) {
        free(s->pixels);
        s->pixels = NULL;
    }
    s->width = 0;
    s->height = 0;
    s->loaded = 0;
}

/* Libpng: converte PNG para RGBA 8888. */
static int loadPNG(const char *path, Sprite *out)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;

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

    unsigned char *raw = NULL;
    png_bytep *rows = NULL;

    if (setjmp(png_jmpbuf(png))) {
        free(rows);
        free(raw);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    png_uint_32 w, h;
    int depth, colorType, interlace, compression, filter;

    png_get_IHDR(png, info, &w, &h, &depth, &colorType,
                 &interlace, &compression, &filter);

    if (w == 0 || h == 0 || w > TEX_MAX || h > TEX_MAX) {
        png_error(png, "PNG dimensions unsupported");
    }

    if (depth == 16)
        png_set_strip_16(png);

    if (colorType == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (colorType == PNG_COLOR_TYPE_GRAY && depth < 8)
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

    png_size_t rowBytes = png_get_rowbytes(png, info);
    raw = (unsigned char *)malloc(rowBytes * h);
    rows = (png_bytep *)malloc(sizeof(png_bytep) * h);

    if (!raw || !rows)
        png_error(png, "Out of memory");

    for (png_uint_32 y = 0; y < h; y++)
        rows[y] = raw + y * rowBytes;

    png_read_image(png, rows);
    png_read_end(png, NULL);

    unsigned int *pixels = (unsigned int *)
        memalign(16, w * h * sizeof(unsigned int));

    if (!pixels)
        png_error(png, "Out of memory for texture");

    for (png_uint_32 y = 0; y < h; y++) {
        for (png_uint_32 x = 0; x < w; x++) {
            unsigned char *p = rows[y] + x * 4;
            unsigned int r = p[0];
            unsigned int g = p[1];
            unsigned int b = p[2];
            unsigned int a = p[3];

            pixels[y * w + x] =
                (a << 24) | (b << 16) | (g << 8) | r;
        }
    }

    free(rows);
    free(raw);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    out->pixels = pixels;
    out->width = (int)w;
    out->height = (int)h;
    out->loaded = 1;
    return 1;
}

static int loadAllSprites(void)
{
    const char *dirs[] = {
        "ms0:/PSP/GAME/RANDOM/assets/sprites/",
        "ef0:/PSP/GAME/RANDOM/assets/sprites/",
        "./assets/sprites/",
        "assets/sprites/"
    };

    int total = 0;
    char path[256];

    for (int i = 0; i < SPRITE_COUNT; i++) {
        freeSprite(&sprites[i]);

        for (int d = 0; d < 4; d++) {
            snprintf(path, sizeof(path), "%s%s",
                     dirs[d], spriteNames[i]);

            if (loadPNG(path, &sprites[i])) {
                total++;
                break;
            }
        }
    }

    return total;
}

static void initGraphics(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888,
                    (void *)0, BUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H,
                    (void *)0x88000, BUF_W);
    sceGuDepthBuffer((void *)0x110000, BUF_W);
    sceGuOffset(2048 - SCREEN_W / 2,
                2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void present(void)
{
    sceGuStart(GU_DIRECT, list);

    sceGuCopyImage(GU_PSM_8888, 0, 0, SCREEN_W, SCREEN_H,
                   BUF_W, framebuffer, 0, 0,
                   BUF_W, (void *)0);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

/*
 * Desenha sprite RGBA usando o framebuffer por CPU.
 * Isso evita dependencias de alinhamento/textura GU
 * durante a fase inicial de testes.
 */
static void drawSprite(int index, int x, int y,
                       int scale, int flipX)
{
    if (index < 0 || index >= SPRITE_COUNT)
        return;

    Sprite *s = &sprites[index];
    if (!s->loaded || !s->pixels)
        return;

    for (int sy = 0; sy < s->height; sy++) {
        for (int sx = 0; sx < s->width; sx++) {
            int srcX = flipX ? s->width - 1 - sx : sx;
            unsigned int c = s->pixels[sy * s->width + srcX];
            unsigned int a = (c >> 24) & 255;

            if (a < 16) continue;

            unsigned int r = c & 255;
            unsigned int g = (c >> 8) & 255;
            unsigned int b = (c >> 16) & 255;

            for (int yy = 0; yy < scale; yy++) {
                for (int xx = 0; xx < scale; xx++) {
                    int dx = x + sx * scale + xx;
                    int dy = y + sy * scale + yy;

                    if (dx < 0 || dx >= SCREEN_W ||
                        dy < 0 || dy >= SCREEN_H)
                        continue;

                    if (a == 255) {
                        framebuffer[dy * BUF_W + dx] =
                            0xFF000000 | (b << 16) |
                            (g << 8) | r;
                    } else {
                        unsigned int old =
                            framebuffer[dy * BUF_W + dx];

                        unsigned int or = old & 255;
                        unsigned int og = (old >> 8) & 255;
                        unsigned int ob = (old >> 16) & 255;

                        r = (r * a + or * (255 - a)) / 255;
                        g = (g * a + og * (255 - a)) / 255;
                        b = (b * a + ob * (255 - a)) / 255;

                        framebuffer[dy * BUF_W + dx] =
                            0xFF000000 | (b << 16) |
                            (g << 8) | r;
                    }
                }
            }
        }
    }
}

static int currentSprite(void)
{
    if (hurtTimer > 0) return HURT;
    if (attacking) return (animFrame & 1) ? ATTACK_2 : ATTACK_1;
    if (magicActive) return MAGIC;

    if (!grounded) {
        if (playerVY < 0)
            return JUMP_1;
        return FALL;
    }

    if (pad.Buttons & PSP_CTRL_R) {
        int f = (gameFrame / 7) % 4;
        return RUN_1 + f;
    }

    if (pad.Buttons & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
        int f = (gameFrame / 9) % 4;
        return WALK_1 + f;
    }

    return ((gameFrame / 40) % 2) ? IDLE_2 : IDLE_1;
}

static void resetGame(void)
{
    playerX = 100.0f;
    playerY = 190.0f;
    playerVY = 0;
    cameraX = 0;
    hp = 5;
    grounded = 1;
    attacking = 0;
    magicActive = 0;
    hurtTimer = 0;
    animFrame = 0;
    gameFrame = 0;
}

static void updateGame(void)
{
    float speed = (pad.Buttons & PSP_CTRL_R) ? 3.0f : 1.7f;

    if (pad.Buttons & PSP_CTRL_LEFT) {
        playerX -= speed;
        facing = -1;
    }

    if (pad.Buttons & PSP_CTRL_RIGHT) {
        playerX += speed;
        facing = 1;
    }

    if (playerX < 0) playerX = 0;
    if (playerX > 2200) playerX = 2200;

    if ((pad.Buttons & PSP_CTRL_CROSS) &&
        !(oldButtons & PSP_CTRL_CROSS) && grounded) {
        playerVY = -7.8f;
        grounded = 0;
    }

    if ((pad.Buttons & PSP_CTRL_SQUARE) &&
        !(oldButtons & PSP_CTRL_SQUARE)) {
        attacking = 1;
        animFrame = 0;
    }

    if ((pad.Buttons & PSP_CTRL_CIRCLE) &&
        !(oldButtons & PSP_CTRL_CIRCLE)) {
        magicActive = 1;
    }

    if (attacking) {
        animTimer++;
        if (animTimer > 8) {
            animTimer = 0;
            animFrame++;
            if (animFrame >= 3) {
                attacking = 0;
                animFrame = 0;
            }
        }
    }

    if (magicActive) {
        animTimer++;
        if (animTimer > 16) {
            magicActive = 0;
            animTimer = 0;
        }
    }

    if (hurtTimer > 0) hurtTimer--;

    if (!grounded) {
        playerVY += 0.35f;
        playerY += playerVY;

        if (playerY >= 190.0f) {
            playerY = 190.0f;
            playerVY = 0;
            grounded = 1;
        }
    }

    cameraX = playerX - 190.0f;
    if (cameraX < 0) cameraX = 0;
    if (cameraX > 1720) cameraX = 1720;

    gameFrame++;
}

static void drawBackground(void)
{
    clearScreen(0xFF17101B);

    /* Ceu da caverna */
    drawRect(0, 0, SCREEN_W, 200, 0xFF21172B);

    /* Cristais decorativos */
    for (int i = 0; i < 8; i++) {
        int x = (i * 83 - (int)cameraX / 3) % 520;
        if (x < 0) x += 520;

        drawRect(x, 38 + (i % 3) * 24, 3, 18,
                 (i % 2) ? 0xFFCE8B42 : 0xFFFFB05A);
        drawRect(x - 3, 44 + (i % 3) * 24, 9, 3,
                 0xFF8C5B39);
    }

    /* Plataformas e piso */
    drawRect(0, 222, SCREEN_W, 50, 0xFF292735);
    drawRect(0, 222, SCREEN_W, 5, 0xFF53506A);

    for (int i = 0; i < SCREEN_W; i += 32) {
        drawRect(i, 238 + (i % 3) * 5, 12, 2,
                 0xFF444052);
    }

    /* Plataforma elevada */
    drawRect(300 - ((int)cameraX % 300) / 3,
             178, 72, 8, 0xFF53506A);
}

static void drawHUD(void)
{
    drawText(12, 10, "RANDOM", 2, WHITE);
    drawText(12, 33, "HP", 1, CYAN);

    for (int i = 0; i < 5; i++) {
        drawRect(34 + i * 15, 32, 11, 8,
                 i < hp ? RED : GREY);
    }

    drawText(350, 12, "PNG:", 1, WHITE);

    char status[16];
    snprintf(status, sizeof(status), "%d/18", loadedCount);
    drawText(386, 12, status, 1,
             loadedCount == SPRITE_COUNT ? GREEN : RED);
}

static void drawGame(void)
{
    drawBackground();

    int px = (int)(playerX - cameraX);
    int py = (int)playerY;

    int sprite = currentSprite();
    if (sprites[sprite].loaded) {
        drawSprite(sprite, px, py - sprites[sprite].height,
                   1, facing < 0);
    } else {
        /* Personagem de reserva se os PNGs nao carregarem. */
        drawRect(px + 8, py - 30, 16, 24, 0xFF4A9CFF);
        drawRect(px + 10, py - 40, 12, 12, 0xFFD4A77B);
        drawRect(px + 8, py - 8, 6, 8, 0xFF303050);
        drawRect(px + 18, py - 8, 6, 8, 0xFF303050);
    }

    if (magicActive) {
        int mx = px + (facing > 0 ? 28 : -12);
        drawRect(mx, py - 30, 12, 12, CYAN);
        drawRect(mx + 3, py - 34, 6, 20, BLUE);
    }

    if (attacking) {
        int ax = px + (facing > 0 ? 28 : -18);
        drawRect(ax, py - 28, 18, 4, WHITE);
        drawRect(ax + 4, py - 34, 10, 3, CYAN);
    }

    drawHUD();
}

static void drawMenu(void)
{
    clearScreen(0xFF100D1A);

    drawRect(0, 0, SCREEN_W, 190, 0xFF1C1730);
    drawRect(0, 190, SCREEN_W, 82, 0xFF30263D);

    drawText(142, 42, "RANDOM", 4, CYAN);
    drawText(130, 80, "FORGOTTEN CAVE", 1, WHITE);

    drawRect(150, 126 + menuChoice * 30, 180, 24,
             0xFF55416D);

    drawText(190, 134, "JOGAR", 2, WHITE);
    drawText(190, 164, "SAIR", 2, WHITE);

    drawText(12, 224, "PNG:", 1, WHITE);

    char status[24];
    snprintf(status, sizeof(status), "%d/18", loadedCount);
    drawText(48, 224, status, 1,
             loadedCount == SPRITE_COUNT ? GREEN : RED);

    if (loadedCount < SPRITE_COUNT)
        drawText(12, 242, "VERIFIQUE ASSETS/SPRITES", 1, CYAN);
    else
        drawText(12, 242, "SPRITES CARREGADOS", 1, GREEN);
}

static void drawPause(void)
{
    drawGame();

    /* Painel de pausa */
    drawRect(105, 60, 270, 145, 0xFF10101F);
    drawRect(105, 60, 270, 3, CYAN);

    drawText(194, 78, "PAUSADO", 2, WHITE);
    drawText(143, 118, "START: CONTINUAR", 1, CYAN);
    drawText(143, 140, "X: VOLTAR AO MENU", 1, WHITE);
}

int main(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initGraphics();
    loadedCount = loadAllSprites();

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (gameState == 0) {
            if ((pad.Buttons & PSP_CTRL_UP) &&
                !(oldButtons & PSP_CTRL_UP)) {
                menuChoice = 0;
            }

            if ((pad.Buttons & PSP_CTRL_DOWN) &&
                !(oldButtons & PSP_CTRL_DOWN)) {
                menuChoice = 1;
            }

            if ((pad.Buttons & PSP_CTRL_CROSS) &&
                !(oldButtons & PSP_CTRL_CROSS)) {
                if (menuChoice == 0) {
                    resetGame();
                    gameState = 1;
                } else {
                    break;
                }
            }

            drawMenu();
        } else if (gameState == 1) {
            if ((pad.Buttons & PSP_CTRL_START) &&
                !(oldButtons & PSP_CTRL_START)) {
                gameState = 2;
            } else {
                updateGame();
            }

            drawGame();
        } else {
            if ((pad.Buttons & PSP_CTRL_START) &&
                !(oldButtons & PSP_CTRL_START)) {
                gameState = 1;
            }

            if ((pad.Buttons & PSP_CTRL_CROSS) &&
                !(oldButtons & PSP_CTRL_CROSS)) {
                gameState = 0;
            }

            drawPause();
        }

        present();
        oldButtons = pad.Buttons;
    }

    for (int i = 0; i < SPRITE_COUNT; i++)
        freeSprite(&sprites[i]);

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
