
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdlib.h>

#include "graphics.h"

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272
#define BUFFER_WIDTH 512

static unsigned int __attribute__((aligned(16))) display_list[262144];

void graphics_init(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, display_list);

    sceGuDrawBuffer(
        GU_PSM_8888,
        (void *)0,
        BUFFER_WIDTH
    );

    sceGuDispBuffer(
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        (void *)0x88000,
        BUFFER_WIDTH
    );

    sceGuDepthBuffer(
        (void *)0x110000,
        BUFFER_WIDTH
    );

    sceGuOffset(
        2048 - (SCREEN_WIDTH / 2),
        2048 - (SCREEN_HEIGHT / 2)
    );

    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuDepthRange(65535, 0);

    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);

    sceGuBlendFunc(
        GU_ADD,
        GU_SRC_ALPHA,
        GU_ONE_MINUS_SRC_ALPHA,
        0,
        0
    );

    sceGuEnable(GU_BLEND);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void graphics_shutdown(void)
{
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

void graphics_begin_frame(void)
{
    sceGuStart(GU_DIRECT, display_list);

    sceGuClearColor(0xFF202030);
    sceGuClearDepth(0);

    sceGuClear(
        GU_COLOR_BUFFER_BIT |
        GU_DEPTH_BUFFER_BIT
    );
}

void graphics_end_frame(void)
{
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void graphics_clear(unsigned int color)
{
    sceGuClearColor(color);

    sceGuClear(
        GU_COLOR_BUFFER_BIT
    );
}

void graphics_draw_rect(
    int x,
    int y,
    int width,
    int height,
    unsigned int color
)
{
    typedef struct {
        unsigned int color;
        short x;
        short y;
        short z;
    } Vertex;

    Vertex *vertices;

    vertices = (Vertex *)sceGuGetMemory(
        2 * sizeof(Vertex)
    );

    if (vertices == NULL) {
        return;
    }

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
