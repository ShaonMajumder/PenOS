#include "apps/terminal.h"
#include "ui/fb.h"

#include <stddef.h>
#include <string.h>

static const char *PROMPT = "PenOS$ ";

static void terminal_scroll(terminal_buffer_t *term)
{
    memmove(term->lines[0],
            term->lines[1],
            (TERM_MAX_LINES - 1) * TERM_LINE_LEN);
    memset(term->lines[TERM_MAX_LINES - 1], 0, TERM_LINE_LEN);
    term->line_count = TERM_MAX_LINES;
    if (term->cursor_line > 0) {
        term->cursor_line--;
    }
}

static void terminal_newline(terminal_buffer_t *term)
{
    if (term->line_count < TERM_MAX_LINES) {
        term->cursor_line = term->line_count;
        term->line_count++;
    } else {
        terminal_scroll(term);
        term->cursor_line = TERM_MAX_LINES - 1;
    }
    term->cursor_col = 0;
    memset(term->lines[term->cursor_line], 0, TERM_LINE_LEN);
}

static void terminal_write_literal(terminal_buffer_t *term, const char *text)
{
    for (const char *p = text; *p; ++p) {
        if (term->cursor_col >= TERM_LINE_LEN - 1) {
            break;
        }
        term->lines[term->cursor_line][term->cursor_col++] = *p;
        term->lines[term->cursor_line][term->cursor_col] = '\0';
    }
}

void terminal_init(terminal_buffer_t *term)
{
    if (!term) {
        return;
    }
    memset(term, 0, sizeof(*term));
    term->line_count = 1;
    terminal_write_literal(term, PROMPT);
}

void terminal_put_char(terminal_buffer_t *term, char c)
{
    if (!term) {
        return;
    }
    if (c == '\r') {
        return;
    }
    if (c == '\n') {
        terminal_newline(term);
        terminal_write_literal(term, PROMPT);
        return;
    }
    if (c == '\b' || c == 127) {
        if (term->cursor_col > (int)strlen(PROMPT)) {
            term->cursor_col--;
            term->lines[term->cursor_line][term->cursor_col] = '\0';
        }
        return;
    }
    if (c < 32 || c > 126) {
        return;
    }
    if (term->cursor_col >= TERM_LINE_LEN - 1) {
        return;
    }
    term->lines[term->cursor_line][term->cursor_col++] = c;
    term->lines[term->cursor_line][term->cursor_col] = '\0';
}

void terminal_draw(const terminal_buffer_t *term,
                   int x,
                   int y,
                   int w,
                   int h,
                   uint32_t fg,
                   uint32_t bg)
{
    if (!term) {
        return;
    }
    fb_fill_rect(x, y, w, h, bg);
    int max_lines = h / 16;
    if (max_lines <= 0) {
        return;
    }

    int start = 0;
    if (term->line_count > max_lines) {
        start = term->line_count - max_lines;
    }
    int line_y = y + 4;
    for (int i = start; i < term->line_count; ++i) {
        fb_draw_string(x + 6, line_y, term->lines[i], fg, 0);
        line_y += 16;
        if (line_y > y + h - 12) {
            break;
        }
    }
}
