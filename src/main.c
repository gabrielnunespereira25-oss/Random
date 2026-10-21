#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspdebug.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "character.h"

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512
#define MENU_COUNT 3

typedef enum {
    STATE_MENU,
    STATE_GAME,
    STATE_PAUSE,
    STATE_CONTROLS
} GameState;

typedef struct {
    unsigned int color;
    short x, y, z;
} Vertex;

typedef struct {
    unsigned short u, v;
    short x, y, z;
} TexVertex;

typedef struct {
    unsigned int *pixels;
    int width, height;
    int tex_width, tex_height;
} Sprite;

static unsigned int __attribute__((aligned(16))) gu_list[262144];

static const char *menu[MENU_COUNT] = {
    "JOGAR",
    "CONTROLES",
    "SAIR"
};

static int selected = 0;
static int running = 1;
static GameState state = STATE_MENU;
static Character player;

static Sprite current_sprite = { NULL, 0, 0, 0, 0 };
static char loaded_path[256] = "";

static int next_power_of_two(int n)
{
    int p = 1;
    while (p < n && p < 1024)
        p <<= 1;
    return p;
}

static void free_sprite(void)
{
    if (current_sprite.pixels) {
        free(current_sprite.pixels);
    }

    memset(&current_sprite, 0, sizeof(current_sprite));
    loaded_path[0] = '\0';
}

static int load_png(const char *path, Sprite *out)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;

    png_structp png = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL
    );

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
    unsigned int *pixels = NULL;

    if (setjmp(png_jmpbuf(png))) {
        free(pixels);
        free(rows);
        free(raw);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    png_uint_32 width, height;
    int depth, color;

    png_get_IHDR(
        png, info, &width, &height,
        &depth, &color, NULL, NULL, NULL
    );

    if (!width || !height || width > 512 || height > 512) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    if (depth == 16)
        png_set_strip_16(png);

    if (color == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (color == PNG_COLOR_TYPE_GRAY && depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (color == PNG_COLOR_TYPE_GRAY ||
        color == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    if (!(color & PNG_COLOR_MASK_ALPHA))
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);

    png_read_update_info(png, info);

    size_t rowbytes = png_get_rowbytes(png, info);

    raw = malloc(rowbytes * height);
    rows = malloc(sizeof(png_bytep) * height);

    if (!raw || !rows) {
        free(raw);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    for (png_uint_32 y = 0; y < height; y++)
        rows[y] = raw + y * rowbytes;

    png_read_image(png, rows);

    int tw = next_power_of_two((int)width);
    int th = next_power_of_two((int)height);

    pixels = memalign(16, tw * th * sizeof(unsigned int));

    if (!pixels) {
        free(rows);
        free(raw);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    memset(pixels, 0, tw * th * sizeof(unsigned int));

    for (png_uint_32 y = 0; y < height; y++) {
        for (png_uint_32 x = 0; x < width; x++) {
            unsigned char *p = rows[y] + x * 4;

            /*
             * GU_PSM_8888 usa o formato de cor PSP.
             * Mantemos os canais RGBA convertidos para o
             * formato de pixel utilizado pelo GU.
             */
            pixels[y * tw + x] =
                ((unsigned int)p[3] << 24) |
                ((unsigned int)p[2] << 16) |
                ((unsigned int)p[1] << 8) |
                (unsigned int)p[0];
        }
    }

    free(rows);
    free(raw);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    out->pixels = pixels;
    out->width = (int)width;
    out->height = (int)height;
    out->tex_width = tw;
    out->tex_height = th;

    sceKernelDcacheWritebackInvalidateAll();

    return 1;
}

static void setup_gu(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, gu_list);

    sceGuDrawBuffer(
        GU_PSM_8888, (void *)0, BUF_W
    );

    sceGuDispBuffer(
        SCREEN_W, SCREEN_H,
        (void *)0x88000, BUF_W
    );

    sceGuDepthBuffer((void *)0x110000, BUF_W);

    sceGuOffset(
        2048 - SCREEN_W / 2,
        2048 - SCREEN_H / 2
    );

    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);

    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);

    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(
        GU_ADD, GU_SRC_ALPHA,
        GU_ONE_MINUS_SRC_ALPHA, 0, 0
    );

    sceGuClearColor(0xFF101018);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void begin_frame(unsigned int color)
{
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(color);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
}

static void end_frame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void draw_rect(
    int x, int y, int w, int h,
    unsigned int color
)
{
    Vertex *v = sceGuGetMemory(2 * sizeof(Vertex));

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDisable(GU_TEXTURE_2D);

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 |
        GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
        2, NULL, v
    );

    sceGuEnable(GU_TEXTURE_2D);
}

static void draw_sprite(Sprite *s, int x, int y)
{
    if (!s->pixels || s->width <= 0 || s->height <= 0)
        return;

    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);

    sceGuTexImage(
        0,
        s->tex_width,
        s->tex_height,
        s->tex_width,
        s->pixels
    );

    TexVertex *v = sceGuGetMemory(2 * sizeof(TexVertex));

    v[0].u = 0;
    v[0].v = 0;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].u = s->width;
    v[1].v = s->height;
    v[1].x = x + s->width;
    v[1].y = y + s->height;
    v[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_16BIT |
        GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
        2, NULL, v
    );
}

static void refresh_sprite(void)
{
    const char *path = character_get_sprite(&player);

    if (!path)
        return;

    if (current_sprite.pixels &&
        strcmp(path, loaded_path) == 0)
        return;

    free_sprite();

    if (load_png(path, &current_sprite)) {
        strncpy(loaded_path, path, sizeof(loaded_path) - 1);
        loaded_path[sizeof(loaded_path) - 1] = '\0';
    }
}

static void draw_menu(void)
{
    /*
     * O menu textual usa a tela de depuração.
     * Não desenhamos GU neste estado para evitar
     * misturar dois sistemas de renderização.
     */
    pspDebugScreenClear();
    pspDebugScreenSetXY(2, 2);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("===== RANDOM PSP =====\n\n");

    for (int i = 0; i < MENU_COUNT; i++) {
        pspDebugScreenSetTextColor(
            i == selected ? 0xFF00FFFF : 0xFFFFFFFF
        );

        pspDebugScreenPrintf(
            "%s %s\n",
            i == selected ? ">" : " ",
            menu[i]
        );
    }

    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("\nSetas: navegar\nX: selecionar\n");
}

static void draw_game(void)
{
    begin_frame(0xFF243044);

    draw_rect(0, 230, SCREEN_W, 42, 0xFF345C38);

    refresh_sprite();

    if (current_sprite.pixels) {
        draw_sprite(
            &current_sprite,
            (int)player.x,
            (int)player.y - current_sprite.height
        );
    } else {
        draw_rect(
            (int)player.x,
            (int)player.y - 32,
            24, 32, 0xFFFFA000
        );
    }

    end_frame();
}

static void draw_pause(void)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(2, 2);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("JOGO PAUSADO\n\n");
    pspDebugScreenPrintf("X: continuar\n");
    pspDebugScreenPrintf("O: voltar ao menu\n");
}

static void draw_controls(void)
{
    pspDebugScreenClear();
    pspDebugScreenSetXY(2, 2);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("CONTROLES\n\n");
    pspDebugScreenPrintf("Setas: mover\n");
    pspDebugScreenPrintf("X: pular\n");
    pspDebugScreenPrintf("Quadrado: atacar\n");
    pspDebugScreenPrintf("Circulo: magia\n");
    pspDebugScreenPrintf("START: pausar\n");
    pspDebugScreenPrintf("O: voltar\n");
}

int main(void)
{
    SceCtrlData pad;
    unsigned int old_buttons = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    pspDebugScreenInit();
    setup_gu();
    character_init(&player);

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int pressed = pad.Buttons & ~old_buttons;
        old_buttons = pad.Buttons;

        if (state == STATE_MENU) {
            if (pressed & PSP_CTRL_UP)
                selected = (selected + MENU_COUNT - 1) % MENU_COUNT;

            if (pressed & PSP_CTRL_DOWN)
                selected = (selected + 1) % MENU_COUNT;

            if (pressed & PSP_CTRL_CROSS) {
                if (selected == 0) {
                    character_init(&player);
                    free_sprite();
                    state = STATE_GAME;
                } else if (selected == 1) {
                    state = STATE_CONTROLS;
                } else {
                    running = 0;
                }
            }

            draw_menu();
        }
        else if (state == STATE_GAME) {
            if (pressed & PSP_CTRL_START) {
                state = STATE_PAUSE;
                draw_pause();
            } else if (pressed & PSP_CTRL_CIRCLE) {
                state = STATE_MENU;
            } else {
                character_update(&player, pad.Buttons);
                draw_game();
            }
        }
        else if (state == STATE_PAUSE) {
            if (pressed & PSP_CTRL_CROSS)
                state = STATE_GAME;

            if (pressed & PSP_CTRL_CIRCLE)
                state = STATE_MENU;

            if (state == STATE_PAUSE)
                draw_pause();
        }
        else if (state == STATE_CONTROLS) {
            draw_controls();

            if (pressed & PSP_CTRL_CIRCLE)
                state = STATE_MENU;
        }

        sceKernelDelayThread(16000);
    }

    free_sprite();
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
