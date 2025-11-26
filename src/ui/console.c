#include "ui/console.h"
#include "arch/x86/io.h"
#include "ui/framebuffer.h"
#include <stddef.h>
#include <stdint.h>

#define CONSOLE_COLS 80
#define CONSOLE_ROWS 25

static uint16_t *video_memory = (uint16_t *)0xB8000;
static uint16_t shadow_buffer[CONSOLE_COLS * CONSOLE_ROWS];
static size_t cursor_x = 0;
static size_t cursor_y = 0;
static uint8_t color = 0x0F;
static int use_framebuffer_console = 0;

static inline uint16_t make_entry(char c, uint8_t attr)
{
    return ((uint16_t)attr << 8) | (uint8_t)c;
}

static void sync_fb_console(void)
{
    if (use_framebuffer_console) {
        framebuffer_console_full_refresh(shadow_buffer, CONSOLE_COLS, CONSOLE_ROWS);
    }
}

static void draw_cell(size_t x, size_t y, char c)
{
    if (x >= CONSOLE_COLS || y >= CONSOLE_ROWS) {
        return;
    }
    size_t idx = y * CONSOLE_COLS + x;
    uint16_t entry = make_entry(c, color);
    video_memory[idx] = entry;
    shadow_buffer[idx] = entry;
    if (use_framebuffer_console) {
        framebuffer_console_draw_cell((uint32_t)x, (uint32_t)y, c, color);
    }
}

static void blank_cell(size_t x, size_t y)
{
    if (x >= CONSOLE_COLS || y >= CONSOLE_ROWS) {
        return;
    }
    size_t idx = y * CONSOLE_COLS + x;
    uint16_t blank = make_entry(' ', color);
    video_memory[idx] = blank;
    shadow_buffer[idx] = blank;
    if (use_framebuffer_console) {
        framebuffer_console_draw_cell((uint32_t)x, (uint32_t)y, ' ', color);
    }
}

static void move_cursor(void)
{
    uint16_t pos = (uint16_t)(cursor_y * CONSOLE_COLS + cursor_x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void)
{
    if (cursor_y < CONSOLE_ROWS) {
        return;
    }
    size_t visible = (CONSOLE_ROWS - 1) * CONSOLE_COLS;
    for (size_t i = 0; i < visible; ++i) {
        video_memory[i] = video_memory[i + CONSOLE_COLS];
        shadow_buffer[i] = shadow_buffer[i + CONSOLE_COLS];
    }
    uint16_t blank = make_entry(' ', color);
    for (size_t i = visible; i < CONSOLE_ROWS * CONSOLE_COLS; ++i) {
        video_memory[i] = blank;
        shadow_buffer[i] = blank;
    }
    cursor_y = CONSOLE_ROWS - 1;
    sync_fb_console();
}

void console_init(void)
{
    cursor_x = cursor_y = 0;
    use_framebuffer_console = framebuffer_console_attach();
    console_clear();
}

void console_clear(void)
{
    uint16_t blank = make_entry(' ', color);
    for (size_t i = 0; i < CONSOLE_COLS * CONSOLE_ROWS; ++i) {
        video_memory[i] = blank;
        shadow_buffer[i] = blank;
    }
    cursor_x = cursor_y = 0;
    move_cursor();
    sync_fb_console();
}

void console_putc(char c)
{
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0 || cursor_y > 0) {
            if (cursor_x == 0) {
                cursor_y--;
                cursor_x = CONSOLE_COLS - 1;
            } else {
                cursor_x--;
            }
            blank_cell(cursor_x, cursor_y);
        }
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~(8 - 1);
    } else {
        draw_cell(cursor_x, cursor_y, c);
        cursor_x++;
    }

    if (cursor_x >= CONSOLE_COLS) {
        cursor_x = 0;
        cursor_y++;
    }
    scroll();
    move_cursor();
}

void console_write(const char *s)
{
    while (*s) {
        console_putc(*s++);
    }
}

void console_write_hex(uint32_t v)
{
    const char *hex = "0123456789ABCDEF";
    console_write("0x");
    for (int i = 28; i >= 0; i -= 4) {
        console_putc(hex[(v >> i) & 0xF]);
    }
}

void console_write_dec(uint32_t v)
{
    if (v == 0) {
        console_putc('0');
        return;
    }
    char buf[16];
    int i = 0;
    while (v > 0 && i < 15) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i--) {
        console_putc(buf[i]);
    }
}

void console_show_boot_splash(const char *version)
{
    if (framebuffer_available()) {
        framebuffer_render_splash(version);
        sync_fb_console();
        return;
    }

    static const char *lines[] = {
        "",
        "                        8888888b.                    .d88888b.   .d8888b.  ",
        "                        888   Y88b                  d88P\" \"Y88b d88P  Y88b ",
        "                        888    888                  888     888 Y88b.      ",
        "                        888   d88P .d88b.  88888b.  888     888  \"Y888b.   ",
        "                        8888888P\" d8P  Y8b 888 \"88b 888     888     \"Y88b. ",
        "                        888       88888888 888  888 888     888       \"888 ",
        "                        888       Y8b.     888  888 Y88b. .d88P Y88b  d88P ",
        "                        888        \"Y8888  888  888  \"Y88888P\"   \"Y8888P\"  ",
        "",
        "                               PenOS :: Minimal 32-bit Kernel",
        "                     Assembly-grade playground for paging | drivers | tasks",
        "--------------------------------------------------------------------------------",
        ""};
    console_clear();
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
        console_write(lines[i]);
        console_putc('\n');
    }
    if (version && *version) {
        console_write("Version: ");
        console_write(version);
        console_putc('\n');
    }
}
