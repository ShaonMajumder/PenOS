#include "ui/fb.h"
#include "ui/framebuffer.h"
#include <stddef.h>

static const framebuffer_info_t *fb_info = NULL;
static int fb_ready = 0;

int fb_init(multiboot_info_t *mb)
{
    framebuffer_init(mb);
    if (!framebuffer_available()) {
        fb_ready = 0;
        return -1;
    }
    framebuffer_console_configure(80, 25);
    fb_info = framebuffer_query();
    fb_ready = fb_info != NULL;
    return fb_ready ? 0 : -1;
}

int fb_present(void)
{
    return fb_ready;
}

uint32_t fb_width(void)
{
    return fb_ready && fb_info ? fb_info->width : 0;
}

uint32_t fb_height(void)
{
    return fb_ready && fb_info ? fb_info->height : 0;
}

void fb_clear(uint32_t argb)
{
    if (!fb_ready) {
        return;
    }
    framebuffer_clear(argb);
}

void fb_put_pixel(int x, int y, uint32_t argb)
{
    if (!fb_ready) {
        return;
    }
    framebuffer_draw_pixel((uint32_t)x, (uint32_t)y, argb);
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t argb)
{
    if (!fb_ready) {
        return;
    }
    framebuffer_draw_rect((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, argb);
}

void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg)
{
    if (!fb_ready) {
        return;
    }
    framebuffer_draw_char((uint32_t)x, (uint32_t)y, c, fg, bg);
}

void fb_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
    if (!fb_ready) {
        return;
    }
    framebuffer_draw_text((uint32_t)x, (uint32_t)y, s, fg, bg);
}

void fb_console_set_visible(int visible)
{
    framebuffer_console_set_visible(visible);
}
