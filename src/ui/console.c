#include "ui/console.h"
#include "arch/x86/io.h"
#include <stddef.h>
#include <stdint.h>

static uint16_t *video_memory = (uint16_t *)0xB8000;
static size_t cursor_x = 0;
static size_t cursor_y = 0;
static uint8_t color = 0x0F;

static void move_cursor(void)
{
    uint16_t pos = cursor_y * 80 + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void)
{
    if (cursor_y < 25) {
        return;
    }
    size_t i;
    for (i = 0; i < 24 * 80; ++i) {
        video_memory[i] = video_memory[i + 80];
    }
    for (i = 24 * 80; i < 25 * 80; ++i) {
        video_memory[i] = (color << 8) | ' ';
    }
    cursor_y = 24;
}

void console_init(void)
{
    cursor_x = cursor_y = 0;
    console_clear();
}

void console_clear(void)
{
    for (size_t i = 0; i < 80 * 25; ++i) {
        video_memory[i] = (color << 8) | ' ';
    }
    cursor_x = cursor_y = 0;
    move_cursor();
}

void console_putc(char c)
{
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~(8 - 1);
    } else {
        video_memory[cursor_y * 80 + cursor_x] = (color << 8) | c;
        cursor_x++;
    }

    if (cursor_x >= 80) {
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
