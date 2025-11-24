#ifndef UI_CONSOLE_H
#define UI_CONSOLE_H
#include <stdint.h>

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char *s);
void console_write_hex(uint32_t v);
void console_write_dec(uint32_t v);

#endif
