#ifndef UI_FRAMEBUFFER_H
#define UI_FRAMEBUFFER_H

#include <stdint.h>
#include "multiboot.h"

typedef struct framebuffer_info {
    uint8_t *addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
} framebuffer_info_t;

void framebuffer_init(multiboot_info_t *mb_info);
int framebuffer_available(void);
const framebuffer_info_t *framebuffer_query(void);

void framebuffer_clear(uint32_t color);
void framebuffer_draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void framebuffer_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void framebuffer_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void framebuffer_draw_text(uint32_t x, uint32_t y, const char *text, uint32_t fg, uint32_t bg);
void framebuffer_draw_text_scaled(uint32_t x, uint32_t y, const char *text, uint32_t fg, uint32_t bg, uint32_t scale);
void framebuffer_blit_sprite(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint32_t *pixels, int skip_zero);
void framebuffer_render_splash(const char *version);
void framebuffer_present(void);

void framebuffer_console_configure(uint32_t cols, uint32_t rows);
int framebuffer_console_attach(void);
void framebuffer_console_set_visible(int visible);
int framebuffer_console_visible(void);
void framebuffer_console_bounds(uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h);
void framebuffer_console_draw_cell(uint32_t col, uint32_t row, char c, uint8_t attr);
void framebuffer_console_full_refresh(const uint16_t *buffer, uint32_t cols, uint32_t rows);

#endif
