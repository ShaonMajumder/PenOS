#ifndef APPS_TERMINAL_H
#define APPS_TERMINAL_H

#include <stdint.h>

#define TERM_MAX_LINES 64
#define TERM_LINE_LEN  80

typedef struct {
    char lines[TERM_MAX_LINES][TERM_LINE_LEN];
    int  line_count;
    int  cursor_line;
    int  cursor_col;
} terminal_buffer_t;

void terminal_init(terminal_buffer_t *term);
void terminal_put_char(terminal_buffer_t *term, char c);
void terminal_draw(const terminal_buffer_t *term,
                   int x,
                   int y,
                   int w,
                   int h,
                   uint32_t fg,
                   uint32_t bg);

#endif /* APPS_TERMINAL_H */
