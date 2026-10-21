
#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <pspdisplay.h>

void graphics_init(void);
void graphics_begin(void);
void graphics_end(void);
void graphics_clear(unsigned int color);
void graphics_draw_rect(int x, int y, int width, int height, unsigned int color);

void graphics_begin_frame(void);
void graphics_end_frame(void);
void graphics_shutdown(void);

#endif
