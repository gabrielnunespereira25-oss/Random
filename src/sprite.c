
#include <pspkernel.h>
#include <pspgu.h>
#include <png.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "sprite.h"

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
} SpriteVertex;

static Sprite current_sprite;

static int power_of_two(int value)
{
    int result = 1;

    while (result < value) {
        result <<= 1;
    }

    return result;
}

static void free_current_sprite(void)
{
    if (current_sprite.pixels != NULL) {
        free(current_sprite.pixels);
    }

    memset(&current_sprite, 0, sizeof(current_sprite));
}

void sprite_init(void)
{
    memset(&current_sprite, 0, sizeof(current_sprite));
}

int sprite_load(const char *path)
{
    FILE *file;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep row = NULL;
    unsigned int *pixels = NULL;

    int width;
    int height;
    int bit_depth;
    int color_type;
    int tex_width;
    int tex_height;
    int x;
    int y;

    png_uint_32 rowbytes;

    if (path == NULL) {
        return 0;
    }

    file = fopen(path, "rb");

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
        free(row);
        free(pixels);

        png_destroy_read_struct(
            &png_ptr, &info_ptr, NULL
        );

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
        png_destroy_read_struct(
            &png_ptr, &info_ptr, NULL
        );

        fclose(file);
        return 0;
    }

    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY &&
        bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }

    if (png_get_valid(
        png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }

    if (!(color_type & PNG_COLOR_MASK_ALPHA) &&
        !png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_add_alpha(
            png_ptr, 0xFF, PNG_FILLER_AFTER
        );
    }

    png_read_update_info(png_ptr, info_ptr);

    rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    if (png_get_channels(png_ptr, info_ptr) != 4 ||
        rowbytes < (png_uint_32)(width * 4)) {
        png_destroy_read_struct(
            &png_ptr, &info_ptr, NULL
        );

        fclose(file);
        return 0;
    }

    row = (png_bytep)malloc(rowbytes);

    if (row == NULL) {
        png_destroy_read_struct(
            &png_ptr, &info_ptr, NULL
        );

        fclose(file);
        return 0;
    }

    tex_width = power_of_two(width);
    tex_height = power_of_two(height);

    pixels = (unsigned int *)memalign(
        16,
        tex_width * tex_height * sizeof(unsigned int)
    );

    if (pixels == NULL) {
        free(row);

        png_destroy_read_struct(
            &png_ptr, &info_ptr, NULL
        );

        fclose(file);
        return 0;
    }

    memset(
        pixels, 0,
        tex_width * tex_height * sizeof(unsigned int)
    );

    free_current_sprite();

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

    png_destroy_read_struct(
        &png_ptr, &info_ptr, NULL
    );

    fclose(file);

    sceKernelDcacheWritebackInvalidateRange(
        pixels,
        tex_width * tex_height * sizeof(unsigned int)
    );

    current_sprite.pixels = pixels;
    current_sprite.width = width;
    current_sprite.height = height;
    current_sprite.tex_width = tex_width;
    current_sprite.tex_height = tex_height;

    return 1;
}

void sprite_draw(const char *path, int x, int y)
{
    SpriteVertex *vertices;

    if (path == NULL) {
        return;
    }

    /*
     * Carrega o PNG solicitado.
     * Para animações, o próximo arquivo será carregado
     * quando o caminho mudar.
     */
    if (current_sprite.pixels == NULL ||
        sprite_load(path) == 0) {
        return;
    }

    sceGuEnable(GU_TEXTURE_2D);

    sceGuTexMode(GU_PSM_8888, 0, 0, 0);

    sceGuTexImage(
        0,
        current_sprite.tex_width,
        current_sprite.tex_height,
        current_sprite.tex_width,
        current_sprite.pixels
    );

    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);

    vertices = (SpriteVertex *)sceGuGetMemory(
        2 * sizeof(SpriteVertex)
    );

    if (vertices == NULL) {
        sceGuDisable(GU_TEXTURE_2D);
        return;
    }

    vertices[0].u = 0;
    vertices[0].v = 0;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0;

    vertices[1].u = current_sprite.width;
    vertices[1].v = current_sprite.height;
    vertices[1].x = x + current_sprite.width;
    vertices[1].y = y + current_sprite.height;
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

void sprite_cleanup(void)
{
    free_current_sprite();
}
