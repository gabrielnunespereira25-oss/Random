
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspdebug.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "character.h"

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512

#define FRAMEBUFFER0 ((void *)0x4400000)
#define FRAMEBUFFER1 ((void *)0x4400000 + 512 * 272 * 4)

static unsigned int __attribute__((aligned(16)))
display_list[262144];

typedef struct {
    unsigned short u, v;
    short x, y, z;
} Vertex;

typedef struct {
    unsigned int *pixels;
    int width;
    int height;
    int tex_width;
    int tex_height;
} Sprite;

static Sprite player_sprite = {0};

static int next_power_of_two(int n)
{
    int p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

static void free_sprite(Sprite *s)
{
    if (s->pixels)
        free(s->pixels);

    memset(s, 0, sizeof(*s));
}

/* Carrega um PNG RGBA para uma textura do PSP. */
static int load_png(const char *path, Sprite *s)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;

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

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        free_sprite(s);
        return 0;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    png_uint_32 w, h;
    int depth, color;

    png_get_IHDR(png, info, &w, &h, &depth,
                 &color, NULL, NULL, NULL);

    if (w == 0 || h == 0 || w > 1024 || h > 1024) {
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

    if (!(color & PNG_COLOR_MASK_ALPHA) &&
        !png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);

    png_read_update_info(png, info);

    int tw = next_power_of_two((int)w);
    int th = next_power_of_two((int)h);

    s->pixels = memalign(16, tw * th * 4);

    if (!s->pixels) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }

    memset(s->pixels, 0, tw * th * 4);

    png_bytep row = malloc(png_get_rowbytes(png, info));

    if (!row) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        free_sprite(s);
        return 0;
    }

    for (unsigned int y = 0; y < h; y++) {
        png_read_row(png, row, NULL);

        memcpy((unsigned char *)s->pixels +
                   y * tw * 4,
               row, w * 4);
    }

    free(row);

    png_read_end(png, NULL);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    s->width = (int)w;
    s->height = (int)h;
    s->tex_width = tw;
    s->tex_height = th;

    return 1;
}

static void draw_sprite(Sprite *s, int x, int y)
{
    if (!s->pixels)
        return;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, s->tex_width, s->tex_height,
                  s->tex_width, s->pixels);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuColor(0xFFFFFFFF);

    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 2);

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

    sceGuDrawArray(GU_SPRITES,
        GU_TEXTURE_16BIT | GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
        2, NULL, v);

    sceGuDisable(GU_TEXTURE_2D);
}

static void draw_rect(int x, int y, int w, int h,
                      unsigned int color)
{
    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);

    Vertex *v = (Vertex *)sceGuGetMemory(sizeof(Vertex) * 2);

    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;

    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;

    sceGuDrawArray(GU_SPRITES,
        GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        2, NULL, v);
}

static void setup_gu(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, display_list);

    sceGuDrawBuffer(GU_PSM_8888,
                    (void *)0, BUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H,
                    FRAMEBUFFER1, BUF_W);
    sceGuDepthBuffer((void *)0x110000, BUF_W);

    sceGuOffset(2048 - SCREEN_W / 2,
                2048 - SCREEN_H / 2);

    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);

    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);

    sceGuClearColor(0xFF252535);
    sceGuClear(GU_COLOR_BUFFER_BIT |
               GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

int main(void)
{
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    setup_gu();

    Character player;
    character_init(&player);

    int menu = 1;
    int running = 1;
    unsigned int previous = 0;

    while (running) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        unsigned int buttons = pad.Buttons;

        if (menu) {
            if ((buttons & PSP_CTRL_CROSS) &&
                !(previous & PSP_CTRL_CROSS)) {
                menu = 0;
            }

            if ((buttons & PSP_CTRL_CIRCLE) &&
                !(previous & PSP_CTRL_CIRCLE)) {
                running = 0;
            }
        } else {
            character_update(&player, buttons);

            if ((buttons & PSP_CTRL_SELECT) &&
                !(previous & PSP_CTRL_SELECT)) {
                menu = 1;
            }
        }

        previous = buttons;

        sceGuStart(GU_DIRECT, display_list);

        sceGuClearColor(0xFF252535);
        sceGuClear(GU_COLOR_BUFFER_BIT |
                   GU_DEPTH_BUFFER_BIT);

        /* Chão provisório */
        draw_rect(0, 252, SCREEN_W, 20, 0xFF426344);

        if (menu) {
            draw_rect(70, 65, 340, 125, 0xFF393958);
            draw_rect(90, 95, 300, 5, 0xFF70D6FF);
            draw_rect(130, 135, 220, 5, 0xFFFFFFFF);
        } else {
            const char *path =
                character_get_sprite(&player);

            if (path) {
                Sprite current = {0};

                if (load_png(path, &current)) {
                    draw_sprite(&current,
                        (int)player.x,
                        (int)player.y - current.height + 32);
                    free_sprite(&current);
                }
            }

            if (player.paused) {
                draw_rect(100, 80, 280, 100,
                          0xDD151525);
                draw_rect(150, 120, 180, 5,
                          0xFFFFFFFF);
            }
        }

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
