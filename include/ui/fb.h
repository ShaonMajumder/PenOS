#ifndef UI_FB_H
#define UI_FB_H

#include <stdint.h>
#include "multiboot.h"

int fb_init(multiboot_info_t *mb);
int fb_present(void);
uint32_t fb_width(void);
uint32_t fb_height(void);

void fb_clear(uint32_t argb);
void fb_put_pixel(int x, int y, uint32_t argb);
void fb_fill_rect(int x, int y, int w, int h, uint32_t argb);
void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg);
void fb_console_set_visible(int visible);
void fb_begin_frame(void);
void fb_end_frame(void);

#endif
