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
#include <malloc.h>

#include "character.h"

PSP_MODULE_INFO("RANDOM", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define MENU_COUNT 3

typedef struct {
    unsigned int *pixels;
    int width;
    int height;
    int tex_width;
    int tex_height;
} Sprite;

typedef struct {
    unsigned short u, v;
    short x, y, z;
} TexVertex;

static Sprite player_sprite;
static char loaded_path[256] = "";
static int sprite_load_error = 0;

static Character player;

static int menu_selection = 0;
static int game_state = 0;
static int running = 1;

static const char *menu_items[MENU_COUNT] = {
    "JOGAR",
    "CONTROLES",
    "SAIR"
};

static unsigned int __attribute__((aligned(16))) list[262144];

/* -------------------------------------------------- */
/* PNG helpers                                        */
/* -------------------------------------------------- */

static int next_power_of_two(int value)
{
    int result = 1;

    while (result < value && result < 512) {
        result <<= 1;
    }

    return result;
}

static void free_sprite(Sprite *sprite)
{
    if (sprite->pixels != NULL) {
        free(sprite->pixels);
    }

    memset(sprite, 0, sizeof(Sprite));
}

static int load_png(const char *filename, Sprite *sprite)
{
    FILE *file;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep *rows = NULL;
    png_bytep row = NULL;

    int width;
    int height;
    int bit_depth;
    int color_type;
    int channels;
    int y;
    int x;

    png_uint_32 rowbytes;

    unsigned int *pixels = NULL;
    int tex_width;
    int tex_height;

    file = fopen(filename, "rb");

    if (file == NULL) {
        return 0;
    }

    {
        unsigned char signature[8];

        if (fread(signature, 1, 8, file) != 8 ||
            png_sig_cmp(signature, 0, 8)) {
            fclose(file);
            return 0;
        }
    }

    png_ptr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL
    );

    if (png_ptr == NULL) {
        fclose(file);
        return 0;
    }

    info_ptr = png_create_info_struct(png_ptr);

    if (info_ptr == NULL) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(file);
        return 0;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        if (rows != NULL) {
            free(rows);
        }

        if (row != NULL) {
            free(row);
        }

        if (pixels != NULL) {
            free(pixels);
        }

        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(file);
        return 0;
    }

    png_init_io(png_ptr, file);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    width = (int)png_get_image_width(png_ptr, info_ptr);
    height = (int)png_get_image_height(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);

    if (width <= 0 || height <= 0 ||
        width > 256 || height > 256) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(file);
        return 0;
    }

    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }

    if (!(color_type & PNG_COLOR_MASK_ALPHA) &&
        !png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }

    png_read_update_info(png_ptr, info_ptr);

    channels = png_get_channels(png_ptr, info_ptr);
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    if (channels != 4 || rowbytes < (png_uint_32)(width * 4)) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(file);
        return 0;
    }

    row = (png_bytep)malloc(rowbytes);

    if (row == NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(file);
        return 0;
    }

    tex_width = next_power_of_two(width);
    tex_height = next_power_of_two(height);

    pixels = (unsigned int *)memalign(
        16, tex_width * tex_height * sizeof(unsigned int)
    );

    if (pixels == NULL) {
        free(row);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(file);
        return 0;
    }

    memset(pixels, 0, tex_width * tex_height * sizeof(unsigned int));

    for (y = 0; y < height; y++) {
        png_read_row(png_ptr, row, NULL);

        for (x = 0; x < width; x++) {
            unsigned char r = row[x * 4 + 0];
            unsigned char g = row[x * 4 + 1];
            unsigned char b = row[x * 4 + 2];
            unsigned char a = row[x * 4 + 3];

            pixels[y * tex_width + x] =
                ((unsigned int)a << 24) |
                ((unsigned int)b << 16) |
                ((unsigned int)g << 8) |
                (unsigned int)r;
        }
    }

    png_read_end(png_ptr, info_ptr);

    free(row);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(file);

    sceKernelDcacheWritebackInvalidateRange(
        pixels,
        tex_width * tex_height * sizeof(unsigned int)
    );

    sprite->pixels = pixels;
    sprite->width = width;
    sprite->height = height;
    sprite->tex_width = tex_width;
    sprite->tex_height = tex_height;

    return 1;
}

/* -------------------------------------------------- */
/* Sprite loading                                     */
/* -------------------------------------------------- */

static int load_sprite_path(const char *path)
{
    Sprite new_sprite;

    memset(&new_sprite, 0, sizeof(new_sprite));

    if (strcmp(path, loaded_path) == 0 &&
        player_sprite.pixels != NULL) {
        return 1;
    }

    free_sprite(&player_sprite);
    loaded_path[0] = '\0';

    if (!load_png(path, &new_sprite)) {
        sprite_load_error = 1;
        return 0;
    }

    player_sprite = new_sprite;

    strncpy(loaded_path, path, sizeof(loaded_path) - 1);
    loaded_path[sizeof(loaded_path) - 1] = '\0';

    sprite_load_error = 0;

    return 1;
}

static void refresh_sprite(void)
{
    const char *path = character_get_sprite(&player);

    if (path == NULL || path[0] == '\0') {
        sprite_load_error = 1;
        return;
    }

    if (strcmp(path, loaded_path) != 0) {
        load_sprite_path(path);
    }
}

/* -------------------------------------------------- */
/* GU rendering                                       */
/* -------------------------------------------------- */

static void setup_gu(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        512
    );

    sceGuDispBuffer(
        SCREEN_W,
        SCREEN_H,
        (void *)0x88000,
        512
    );

    sceGuDepthBuffer(
        (void *)0x110000,
        512
    );

    sceGuOffset(2048 - (SCREEN_W / 2), 2048 - (SCREEN_H / 2));
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);

    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuAlphaFunc(GU_GREATER, 0, 0xFF);
    sceGuEnable(GU_ALPHA_TEST);

    sceGuBlendFunc(
        GU_ADD,
        GU_SRC_ALPHA,
        GU_ONE_MINUS_SRC_ALPHA,
        0,
        0
    );

    sceGuEnable(GU_BLEND);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void begin_frame(void)
{
    sceGuStart(GU_DIRECT, list);

    sceGuClearColor(0xFF202030);
    sceGuClearDepth(0);
    sceGuClear(
        GU_COLOR_BUFFER_BIT |
        GU_DEPTH_BUFFER_BIT
    );
}

static void end_frame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void draw_rect(
    int x,
    int y,
    int width,
    int height,
    unsigned int color
)
{
    typedef struct {
        unsigned int color;
        short x, y, z;
    } ColorVertex;

    ColorVertex *vertices;

    vertices = (ColorVertex *)sceGuGetMemory(
        2 * sizeof(ColorVertex)
    );

    vertices[0].color = color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0;

    vertices[1].color = color;
    vertices[1].x = x + width;
    vertices[1].y = y + height;
    vertices[1].z = 0;

    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 |
        GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
        2,
        NULL,
        vertices
    );
}

static void draw_sprite(int x, int y)
{
    TexVertex *vertices;

    if (player_sprite.pixels == NULL) {
        return;
    }

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(
        0,
        player_sprite.tex_width,
        player_sprite.tex_height,
        player_sprite.tex_width,
        player_sprite.pixels
    );

    sceGuTexFunc(
        GU_TFX_MODULATE,
        GU_TCC_RGBA
    );

    sceGuTexFilter(
        GU_LINEAR,
        GU_LINEAR
    );

    sceGuTexWrap(
        GU_CLAMP,
        GU_CLAMP
    );

    sceGuEnable(GU_BLEND);

    vertices = (TexVertex *)sceGuGetMemory(
        2 * sizeof(TexVertex)
    );

    vertices[0].u = 0;
    vertices[0].v = 0;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0;

    vertices[1].u = player_sprite.width;
    vertices[1].v = player_sprite.height;
    vertices[1].x = x + player_sprite.width;
    vertices[1].y = y + player_sprite.height;
    vertices[1].z = 0;

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_16BIT |
        GU_VERTEX_16BIT |
        GU_TRANSFORM_2D,
        2,
        NULL,
        vertices
    );

    sceGuDisable(GU_TEXTURE_2D);
}

/* -------------------------------------------------- */
/* Menu and game                                      */
/* -------------------------------------------------- */

static void draw_menu(void)
{
    int i;

    begin_frame();

    draw_rect(0, 0, SCREEN_W, SCREEN_H, 0xFF202030);
    draw_rect(70, 45, 340, 180, 0xFF303048);

    end_frame();

    pspDebugScreenSetXY(17, 4);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("RANDOM PSP");

    for (i = 0; i < MENU_COUNT; i++) {
        pspDebugScreenSetXY(18, 9 + i * 2);

        if (i == menu_selection) {
            pspDebugScreenSetTextColor(0xFF00FFFF);
            pspDebugScreenPrintf("> %s", menu_items[i]);
        } else {
            pspDebugScreenSetTextColor(0xFFFFFFFF);
            pspDebugScreenPrintf("  %s", menu_items[i]);
        }
    }

    pspDebugScreenSetXY(5, 30);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("Use D-Pad and X to select");
}

static void draw_controls(void)
{
    begin_frame();
    draw_rect(0, 0, SCREEN_W, SCREEN_H, 0xFF202030);
    end_frame();

    pspDebugScreenSetXY(5, 4);
    pspDebugScreenSetTextColor(0xFFFFFFFF);
    pspDebugScreenPrintf("CONTROLES");

    pspDebugScreenSetXY(5, 8);
    pspDebugScreenPrintf("D-Pad: mover");

    pspDebugScreenSetXY(5, 10);
    pspDebugScreenPrintf("X: selecionar / pular");

    pspDebugScreenSetXY(5, 12);
    pspDebugScreenPrintf("O: voltar");

    pspDebugScreenSetXY(5, 16);
    pspDebugScreenPrintf("Pressione O para voltar");
}

static void draw_game(void)
{
    begin_frame();

    draw_rect(0, 0, SCREEN_W, SCREEN_H, 0xFF76B9E8);
    draw_rect(0, 220, SCREEN_W, 52, 0xFF399447);

    refresh_sprite();

    if (!sprite_load_error && player_sprite.pixels != NULL) {
        draw_sprite((int)player.x, (int)player.y);
    }

    end_frame();

    if (sprite_load_error) {
        pspDebugScreenSetXY(1, 1);
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        pspDebugScreenPrintf("PNG nao encontrado!");
        pspDebugScreenSetXY(1, 2);
        pspDebugScreenPrintf("Verifique assets/sprites/");
    }
}

/* -------------------------------------------------- */
/* Input                                               */
/* -------------------------------------------------- */

static void update_menu(unsigned int buttons)
{
    if (buttons & PSP_CTRL_UP) {
        if (menu_selection > 0) {
            menu_selection--;
        }
    }

    if (buttons & PSP_CTRL_DOWN) {
        if (menu_selection < MENU_COUNT - 1) {
            menu_selection++;
        }
    }

    if (buttons & PSP_CTRL_CROSS) {
        if (menu_selection == 0) {
            character_init(&player);
            game_state = 1;
        } else if (menu_selection == 1) {
            game_state = 2;
        } else if (menu_selection == 2) {
            running = 0;
        }
    }
}

static void update_game(unsigned int buttons)
{
    if (buttons & PSP_CTRL_START) {
        game_state = 3;
        return;
    }

    if (buttons & PSP_CTRL_CIRCLE) {
        game_state = 0;
        return;
    }

    character_update(&player, buttons);
}

static void update_pause(unsigned int buttons)
{
    if (buttons & PSP_CTRL_START) {
        game_state = 1;
    }

    if (buttons & PSP_CTRL_CIRCLE) {
        game_state = 0;
    }
}

/* -------------------------------------------------- */
/* Main                                                */
/* -------------------------------------------------- */

int main(int argc, char *argv[])
{
    SceCtrlData pad;
    unsigned int previous_buttons = 0;

    (void)argc;
    (void)argv;

    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    character_init(&player);
    setup_gu();

    while (running) {
        unsigned int buttons;
        unsigned int pressed;

        sceCtrlReadBufferPositive(&pad, 1);

        buttons = pad.Buttons;
        pressed = buttons & ~previous_buttons;
        previous_buttons = buttons;

        switch (game_state) {
            case 0:
                if (pressed & (PSP_CTRL_UP | PSP_CTRL_DOWN |
                               PSP_CTRL_CROSS)) {
                    update_menu(pressed);
                }

                draw_menu();
                break;

            case 1:
                update_game(pressed);
                draw_game();
                break;

            case 2:
                if (pressed & PSP_CTRL_CIRCLE) {
                    game_state = 0;
                }

                draw_controls();
                break;

            case 3:
                update_pause(pressed);

                begin_frame();
                draw_rect(0, 0, SCREEN_W, SCREEN_H, 0xFF202030);
                end_frame();

                pspDebugScreenSetXY(15, 8);
                pspDebugScreenSetTextColor(0xFFFFFFFF);
                pspDebugScreenPrintf("PAUSADO");

                pspDebugScreenSetXY(7, 12);
                pspDebugScreenPrintf("START: continuar");

                pspDebugScreenSetXY(7, 14);
                pspDebugScreenPrintf("O: menu");

                break;
        }

        sceDisplayWaitVblankStart();
    }

    free_sprite(&player_sprite);

    sceGuTerm();
    sceKernelExitGame();

    return 0;
}
