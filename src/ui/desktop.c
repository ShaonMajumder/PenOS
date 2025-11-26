#include "ui/desktop.h"
#include "ui/fb.h"
#include "ui/console.h"
#include "drivers/mouse.h"
#include "drivers/keyboard.h"
#include "arch/x86/timer.h"
#include "apps/terminal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void append_char(char *buf, size_t *idx, size_t capacity, char c)
{
    if (*idx + 1 >= capacity) {
        return;
    }
    buf[*idx] = c;
    (*idx)++;
}

static void append_str(char *buf, size_t *idx, size_t capacity, const char *str)
{
    while (*str) {
        append_char(buf, idx, capacity, *str++);
    }
}

/* ---- Layout constants --------------------------------------------------- */

#define TASKBAR_HEIGHT            52
#define ICON_W                    96
#define ICON_H                    96
#define CURSOR_W                  11
#define CURSOR_H                  17
#define WINDOW_W                  460
#define WINDOW_H                  280
#define TITLEBAR_HEIGHT           26
#define SPLASH_DELAY_TICKS        120

#define START_BTN_MARGIN_X        16
#define START_BTN_MARGIN_Y        10
#define START_BTN_WIDTH           120
#define START_BTN_HEIGHT          (TASKBAR_HEIGHT - 20)

#define START_MENU_WIDTH          220
#define START_MENU_ENTRY_HEIGHT   28

#define MAX_WINDOWS               8

/* ---- Desktop models ----------------------------------------------------- */

typedef struct {
    const char *label;
    int x;
    int y;
    app_kind_t kind;
} icon_t;

typedef struct {
    const char *label;
    app_kind_t kind;
} start_menu_entry_t;

typedef struct {
    const char *name;
    const char *type;
    const char *detail;
} vfs_entry_t;

typedef struct desktop_window {
    int  id;
    app_kind_t kind;
    int  x, y;
    int  w, h;
    int  z;             /* z-order; higher draws on top */
    int  visible;
    int  has_focus;

    int  dragging;
    int  drag_offset_x;
    int  drag_offset_y;

    terminal_buffer_t terminal; /* used when kind == APP_TERMINAL */
} desktop_window_t;

typedef struct {
    int width;
    int height;
    int cursor_x;
    int cursor_y;
    uint8_t buttons;
    uint8_t prev_buttons;
    int active;
    int scene_dirty;
    int start_menu_open;
} desktop_state_t;

/* ---- Static desktop data ------------------------------------------------ */

static desktop_state_t g_desktop;
static desktop_window_t g_windows[MAX_WINDOWS];
static int g_next_window_id = 1;
static int g_next_z = 1;
static int g_spawn_offset = 0;

static const icon_t g_icons[] = {
    {"Apps",        40,  70, APP_APPS},
    {"Settings",   170,  70, APP_SETTINGS},
    {"Recycle Bin",300,  70, APP_RECYCLE},
    {"Files",      430,  70, APP_FILES},
};
#define ICON_COUNT (int)(sizeof(g_icons) / sizeof(g_icons[0]))

static const start_menu_entry_t g_start_menu[] = {
    {"Terminal",     APP_TERMINAL},
    {"System Info",  APP_APPS},
    {"Files",        APP_FILES},
    {"Settings",     APP_SETTINGS},
    {"Recycle Bin",  APP_RECYCLE},
    {"About PenOS",  APP_ABOUT},
};
#define START_MENU_COUNT (int)(sizeof(g_start_menu) / sizeof(g_start_menu[0]))

static const char *g_apps_lines[] = {
    "* System Info snapshot",
    "* Scheduler demo (spinner/counter)",
    "* Future PenScript shell",
    "",
    "Icons launch stubs only today."
};

static const char *g_settings_lines[] = {
    "Theme: PenOS Indigo",
    "Resolution: 1024 x 768",
    "Kernel Heap: higher-half freelist",
    "Scheduler: preemptive RR @100Hz",
    "",
    "Settings are read-only in this build."
};

static const char *g_recycle_lines[] = {
    "Recycle Bin is empty.",
    "",
    "Soon it will show deleted items."
};

static const char *g_about_lines[] = {
    "PenOS Desktop v1",
    "",
    "This framebuffer UI is built entirely in C",
    "with PS/2 input and a toy window manager.",
    "",
    "ESC always returns to the shell.",
    "Terminal windows run a fake prompt for now."
};

static const vfs_entry_t g_vfs_entries[] = {
    {"Documents",       "dir",  "3 items"},
    {"Screenshots",     "dir",  "empty"},
    {"README.txt",      "file", "2 KB"},
    {"build.log",       "file", "8 KB"},
    {"penos-logo.png",  "file", "16 KB"},
    {"TODO.md",         "file", "1 KB"},
};
#define VFS_ENTRY_COUNT (int)(sizeof(g_vfs_entries) / sizeof(g_vfs_entries[0]))

/* ---- Utility helpers ---------------------------------------------------- */

static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return (x >= rx && x < rx + rw && y >= ry && y < ry + rh);
}

static const char *app_title(app_kind_t kind)
{
    switch (kind) {
    case APP_TERMINAL: return "Terminal";
    case APP_APPS:     return "System Info";
    case APP_SETTINGS: return "Settings";
    case APP_RECYCLE:  return "Recycle Bin";
    case APP_FILES:    return "Files";
    case APP_ABOUT:    return "About PenOS";
    default:           return "";
    }
}

static void start_button_rect(int *x, int *y, int *w, int *h)
{
    if (x) *x = START_BTN_MARGIN_X;
    if (w) *w = START_BTN_WIDTH;
    if (h) *h = START_BTN_HEIGHT;
    if (y) *y = g_desktop.height - TASKBAR_HEIGHT + START_BTN_MARGIN_Y;
}

static void start_menu_bounds(int *x, int *y, int *w, int *h)
{
    int width = START_MENU_WIDTH;
    int height = START_MENU_ENTRY_HEIGHT * START_MENU_COUNT + 12;
    int menu_x = START_BTN_MARGIN_X;
    int menu_y = g_desktop.height - TASKBAR_HEIGHT - height - 4;
    if (menu_y < 8) {
        menu_y = 8;
    }
    if (x) *x = menu_x;
    if (y) *y = menu_y;
    if (w) *w = width;
    if (h) *h = height;
}

/* ---- Window manager ----------------------------------------------------- */

static desktop_window_t *desktop_find_window_by_kind(app_kind_t kind)
{
    if (kind == APP_NONE) {
        return NULL;
    }
    for (int i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].visible && g_windows[i].kind == kind) {
            return &g_windows[i];
        }
    }
    return NULL;
}

static desktop_window_t *desktop_find_free_slot(void)
{
    for (int i = 0; i < MAX_WINDOWS; ++i) {
        if (!g_windows[i].visible && g_windows[i].kind == APP_NONE) {
            return &g_windows[i];
        }
    }
    return NULL;
}

static desktop_window_t *desktop_find_window_at(int x, int y)
{
    desktop_window_t *best = NULL;
    int best_z = -1;
    for (int i = 0; i < MAX_WINDOWS; ++i) {
        desktop_window_t *w = &g_windows[i];
        if (!w->visible) {
            continue;
        }
        if (!point_in_rect(x, y, w->x, w->y, w->w, w->h)) {
            continue;
        }
        if (w->z > best_z) {
            best_z = w->z;
            best = w;
        }
    }
    return best;
}

static desktop_window_t *desktop_focused_window(void)
{
    for (int i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].visible && g_windows[i].has_focus) {
            return &g_windows[i];
        }
    }
    return NULL;
}

static void desktop_focus_window(desktop_window_t *win)
{
    for (int i = 0; i < MAX_WINDOWS; ++i) {
        g_windows[i].has_focus = 0;
    }
    if (!win || !win->visible) {
        return;
    }
    win->has_focus = 1;
    win->z = g_next_z++;
    g_desktop.scene_dirty = 1;
}

static void desktop_close_window(desktop_window_t *win)
{
    if (!win) {
        return;
    }
    memset(win, 0, sizeof(*win));
    g_desktop.scene_dirty = 1;
}

static desktop_window_t *desktop_spawn_window(app_kind_t kind)
{
    if (kind == APP_NONE) {
        return NULL;
    }
    desktop_window_t *existing = desktop_find_window_by_kind(kind);
    if (existing) {
        desktop_focus_window(existing);
        return existing;
    }
    desktop_window_t *slot = desktop_find_free_slot();
    if (!slot) {
        console_write("[gui] No free window slots\n");
        return NULL;
    }

    memset(slot, 0, sizeof(*slot));
    slot->id = g_next_window_id++;
    slot->kind = kind;
    slot->w = WINDOW_W;
    slot->h = WINDOW_H;

    int base_x = (g_desktop.width - WINDOW_W) / 2;
    int base_y = (g_desktop.height - WINDOW_H) / 2 - 20;
    if (base_y < 40) {
        base_y = 40;
    }
    int offset = g_spawn_offset;
    g_spawn_offset = (g_spawn_offset + 32) % 160;

    slot->x = base_x + offset;
    slot->y = base_y + offset;
    slot->visible = 1;
    slot->z = g_next_z++;
    slot->has_focus = 1;

    if (kind == APP_TERMINAL) {
        terminal_init(&slot->terminal);
    }

    desktop_focus_window(slot);
    return slot;
}

/* ---- Drawing ------------------------------------------------------------ */

static void draw_background(void)
{
    fb_clear(0xFF0B1736);
}

static void draw_taskbar(void)
{
    int bar_y = g_desktop.height - TASKBAR_HEIGHT;
    fb_fill_rect(0, bar_y, g_desktop.width, TASKBAR_HEIGHT, 0xFF181C27);

    int sx, sy, sw, sh;
    start_button_rect(&sx, &sy, &sw, &sh);
    uint32_t start_color = g_desktop.start_menu_open ? 0xFF3AA169 : 0xFF2E8B57;
    fb_fill_rect(sx, sy, sw, sh, start_color);
    fb_draw_string(sx + 18, sy + 8, "PenOS", 0xFFFFFFFF, start_color);

    fb_draw_string(sx + sw + 32, sy + 8, "ESC returns to the shell", 0xFFCBD5F5, 0xFF181C27);
}

static void draw_icons(void)
{
    for (int i = 0; i < ICON_COUNT; ++i) {
        const icon_t *icon = &g_icons[i];
        fb_fill_rect(icon->x, icon->y, ICON_W, ICON_H, 0xFF233457);
        fb_fill_rect(icon->x + 3, icon->y + 3, ICON_W - 6, ICON_H - 6, 0xFF394B7B);
        fb_draw_string(icon->x + 16, icon->y + ICON_H + 10, icon->label, 0xFFE5E7EB, 0);
    }
}

static void draw_lines_block(int x, int y, const char *const *lines, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        fb_draw_string(x, y + (int)(i * 16), lines[i], 0xFFE5E7EB, 0);
    }
}

static void draw_files_block(int x, int y)
{
    for (int i = 0; i < VFS_ENTRY_COUNT; ++i) {
        const vfs_entry_t *entry = &g_vfs_entries[i];
        char line[96];
        size_t idx = 0;
        append_str(line, &idx, sizeof(line), entry->name);
        append_str(line, &idx, sizeof(line), " (");
        append_str(line, &idx, sizeof(line), entry->type);
        append_str(line, &idx, sizeof(line), ") - ");
        append_str(line, &idx, sizeof(line), entry->detail);
        line[idx] = '\0';
        fb_draw_string(x, y + i * 16, line, 0xFFE5E7EB, 0);
    }
}

static void draw_window_content(const desktop_window_t *win, int x, int y, int w, int h)
{
    switch (win->kind) {
    case APP_TERMINAL:
        terminal_draw(&win->terminal, x, y, w, h, 0xFFE5E7EB, 0xFF0B111C);
        break;
    case APP_APPS:
        draw_lines_block(x, y, g_apps_lines, sizeof(g_apps_lines) / sizeof(g_apps_lines[0]));
        break;
    case APP_SETTINGS:
        draw_lines_block(x, y, g_settings_lines, sizeof(g_settings_lines) / sizeof(g_settings_lines[0]));
        break;
    case APP_RECYCLE:
        draw_lines_block(x, y, g_recycle_lines, sizeof(g_recycle_lines) / sizeof(g_recycle_lines[0]));
        break;
    case APP_FILES:
        draw_files_block(x, y);
        break;
    case APP_ABOUT:
        draw_lines_block(x, y, g_about_lines, sizeof(g_about_lines) / sizeof(g_about_lines[0]));
        break;
    default:
        break;
    }
}

static void draw_window(const desktop_window_t *win)
{
    if (!win->visible || win->kind == APP_NONE) {
        return;
    }

    uint32_t base_color = 0xFF1D2434;
    uint32_t title_color = win->has_focus ? 0xFF111827 : 0xFF2A3144;
    fb_fill_rect(win->x + 8, win->y + 8, win->w, win->h, 0x55000000);
    fb_fill_rect(win->x, win->y, win->w, win->h, base_color);
    fb_fill_rect(win->x, win->y, win->w, TITLEBAR_HEIGHT, title_color);
    fb_draw_string(win->x + 12, win->y + 6, app_title(win->kind), 0xFFFFFFFF, title_color);

    int close_w = 22;
    int close_h = 16;
    int close_x = win->x + win->w - close_w - 6;
    int close_y = win->y + (TITLEBAR_HEIGHT - close_h) / 2;
    fb_fill_rect(close_x, close_y, close_w, close_h, 0xFF7F1D1D);
    fb_draw_string(close_x + 6, close_y + 4, "x", 0xFFFFFFFF, 0xFF7F1D1D);

    int content_x = win->x + 18;
    int content_y = win->y + TITLEBAR_HEIGHT + 14;
    int content_w = win->w - 36;
    int content_h = win->h - TITLEBAR_HEIGHT - 32;
    draw_window_content(win, content_x, content_y, content_w, content_h);
}

static void draw_windows(void)
{
    int order[MAX_WINDOWS];
    for (int i = 0; i < MAX_WINDOWS; ++i) {
        order[i] = i;
    }

    for (int i = 0; i < MAX_WINDOWS; ++i) {
        for (int j = i + 1; j < MAX_WINDOWS; ++j) {
            if (g_windows[order[i]].z > g_windows[order[j]].z) {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    for (int k = 0; k < MAX_WINDOWS; ++k) {
        draw_window(&g_windows[order[k]]);
    }
}

static void draw_start_menu(void)
{
    if (!g_desktop.start_menu_open) {
        return;
    }
    int mx, my, mw, mh;
    start_menu_bounds(&mx, &my, &mw, &mh);
    fb_fill_rect(mx + 4, my + 4, mw, mh, 0x66000000);
    fb_fill_rect(mx, my, mw, mh, 0xFF141B2A);

    for (int i = 0; i < START_MENU_COUNT; ++i) {
        int line_y = my + 6 + i * START_MENU_ENTRY_HEIGHT;
        fb_draw_string(mx + 16, line_y, g_start_menu[i].label, 0xFFE5E7EB, 0);
    }
}

static void draw_cursor(void)
{
    fb_fill_rect(g_desktop.cursor_x, g_desktop.cursor_y, CURSOR_W, CURSOR_H, 0xFFFFFFFF);
    fb_fill_rect(g_desktop.cursor_x + CURSOR_W - 2,
                 g_desktop.cursor_y + CURSOR_H / 2,
                 6,
                 CURSOR_H / 2,
                 0xFFFFFFFF);
}

static void draw_scene(void)
{
    draw_background();
    draw_icons();
    draw_windows();
    draw_taskbar();
    draw_start_menu();
    draw_cursor();
}

/* ---- Mouse and start menu handling -------------------------------------- */

static void handle_icon_click(int x, int y)
{
    for (int i = 0; i < ICON_COUNT; ++i) {
        const icon_t *icon = &g_icons[i];
        if (point_in_rect(x, y, icon->x, icon->y, ICON_W, ICON_H)) {
            desktop_spawn_window(icon->kind);
            g_desktop.start_menu_open = 0;
            return;
        }
    }
    desktop_focus_window(NULL);
    g_desktop.start_menu_open = 0;
}

static void handle_start_menu_selection(int x, int y)
{
    int mx, my, mw, mh;
    start_menu_bounds(&mx, &my, &mw, &mh);
    if (!point_in_rect(x, y, mx, my, mw, mh)) {
        g_desktop.start_menu_open = 0;
        return;
    }
    int relative = y - (my + 6);
    int idx = relative / START_MENU_ENTRY_HEIGHT;
    if (idx >= 0 && idx < START_MENU_COUNT) {
        desktop_spawn_window(g_start_menu[idx].kind);
    }
    g_desktop.start_menu_open = 0;
}

static void handle_window_click(int x, int y)
{
    desktop_window_t *win = desktop_find_window_at(x, y);
    if (!win) {
        desktop_focus_window(NULL);
        return;
    }

    int close_w = 22;
    int close_h = 16;
    int close_x = win->x + win->w - close_w - 6;
    int close_y = win->y + (TITLEBAR_HEIGHT - close_h) / 2;
    if (point_in_rect(x, y, close_x, close_y, close_w, close_h)) {
        desktop_close_window(win);
        return;
    }

    desktop_focus_window(win);
    if (point_in_rect(x, y, win->x, win->y, win->w, TITLEBAR_HEIGHT)) {
        win->dragging = 1;
        win->drag_offset_x = x - win->x;
        win->drag_offset_y = y - win->y;
    }
    g_desktop.start_menu_open = 0;
}

static void clamp_window(desktop_window_t *win)
{
    if (win->x < 0) win->x = 0;
    if (win->y < 0) win->y = 0;
    if (win->x + win->w > g_desktop.width) {
        win->x = g_desktop.width - win->w;
    }
    int bottom_limit = g_desktop.height - TASKBAR_HEIGHT;
    if (win->y + win->h > bottom_limit) {
        win->y = bottom_limit - win->h;
        if (win->y < 0) win->y = 0;
    }
}

static void process_click(int x, int y)
{
    int sx, sy, sw, sh;
    start_button_rect(&sx, &sy, &sw, &sh);
    if (point_in_rect(x, y, sx, sy, sw, sh)) {
        g_desktop.start_menu_open = !g_desktop.start_menu_open;
        g_desktop.scene_dirty = 1;
        return;
    }

    if (g_desktop.start_menu_open) {
        int mx, my, mw, mh;
        start_menu_bounds(&mx, &my, &mw, &mh);
        if (point_in_rect(x, y, mx, my, mw, mh)) {
            handle_start_menu_selection(x, y);
            g_desktop.scene_dirty = 1;
            return;
        } else {
            g_desktop.start_menu_open = 0;
        }
    }

    desktop_window_t *win = desktop_find_window_at(x, y);
    if (win) {
        handle_window_click(x, y);
        g_desktop.scene_dirty = 1;
        return;
    }

    handle_icon_click(x, y);
    g_desktop.scene_dirty = 1;
}

static desktop_window_t *desktop_drag_window(void)
{
    for (int i = 0; i < MAX_WINDOWS; ++i) {
        if (g_windows[i].visible && g_windows[i].dragging) {
            return &g_windows[i];
        }
    }
    return NULL;
}

void desktop_handle_mouse(int dx, int dy, uint8_t buttons)
{
    if (!g_desktop.active) {
        return;
    }

    g_desktop.cursor_x += dx;
    g_desktop.cursor_y -= dy; /* PS/2 Y axis is inverted */

    if (g_desktop.cursor_x < 0) g_desktop.cursor_x = 0;
    if (g_desktop.cursor_y < 0) g_desktop.cursor_y = 0;
    if (g_desktop.cursor_x > g_desktop.width - CURSOR_W) g_desktop.cursor_x = g_desktop.width - CURSOR_W;
    if (g_desktop.cursor_y > g_desktop.height - CURSOR_H) g_desktop.cursor_y = g_desktop.height - CURSOR_H;

    uint8_t prev = g_desktop.buttons;
    g_desktop.prev_buttons = prev;
    g_desktop.buttons = buttons;

    int left_prev = (prev & 0x1) != 0;
    int left_now = (buttons & 0x1) != 0;

    if (!left_prev && left_now) {
        process_click(g_desktop.cursor_x, g_desktop.cursor_y);
    }

    desktop_window_t *drag_win = desktop_drag_window();
    if (drag_win && left_now) {
        drag_win->x = g_desktop.cursor_x - drag_win->drag_offset_x;
        drag_win->y = g_desktop.cursor_y - drag_win->drag_offset_y;
        clamp_window(drag_win);
        g_desktop.scene_dirty = 1;
    }

    if (drag_win && !left_now) {
        drag_win->dragging = 0;
    }
}

/* ---- Keyboard handling -------------------------------------------------- */

static void desktop_handle_key(int ch)
{
    desktop_window_t *focus = desktop_focused_window();
    if (!focus) {
        return;
    }
    if (focus->kind == APP_TERMINAL) {
        terminal_put_char(&focus->terminal, (char)ch);
        g_desktop.scene_dirty = 1;
    }
}

static void flush_keyboard(void)
{
    while (keyboard_read_char() != -1) {
        /* discard buffered keys */
    }
}

static void show_splash(void)
{
    fb_clear(0xFF050C16);
    fb_draw_string(g_desktop.width / 2 - 80, g_desktop.height / 2 - 12, "PenOS Desktop", 0xFF38BDF8, 0);
    fb_draw_string(g_desktop.width / 2 - 120, g_desktop.height / 2 + 12, "Preparing GUI environment...", 0xFF9CA3AF, 0);

    uint64_t start = timer_ticks();
    while (timer_ticks() - start < SPLASH_DELAY_TICKS) {
        __asm__ volatile("hlt");
    }
}

static void desktop_loop(void)
{
    while (g_desktop.active) {
        if (g_desktop.scene_dirty) {
            draw_scene();
            g_desktop.scene_dirty = 0;
        }

        int ch = keyboard_read_char();
        if (ch == 27) { /* ESC */
            g_desktop.start_menu_open = 0;
            g_desktop.active = 0;
            break;
        } else if (ch != -1) {
            desktop_handle_key(ch);
        }

        __asm__ volatile("hlt");
    }
}

/* ---- Entry point -------------------------------------------------------- */

void desktop_start(void)
{
    if (!fb_present()) {
        console_write("[gui] Framebuffer not available. Use a graphical target in GRUB.\n");
        return;
    }

    console_write("[gui] Launching desktop (ESC to exit)...\n");
    flush_keyboard();
    fb_console_set_visible(0);

    memset(&g_desktop, 0, sizeof(g_desktop));
    g_desktop.width = (int)fb_width();
    g_desktop.height = (int)fb_height();
    g_desktop.cursor_x = g_desktop.width / 2;
    g_desktop.cursor_y = g_desktop.height / 2;
    g_desktop.active = (g_desktop.width > 0 && g_desktop.height > 0);
    g_desktop.scene_dirty = 1;
    g_desktop.start_menu_open = 0;

    memset(g_windows, 0, sizeof(g_windows));
    g_next_window_id = 1;
    g_next_z = 1;
    g_spawn_offset = 0;

    if (!g_desktop.active) {
        console_write("[gui] Invalid framebuffer dimensions.\n");
        fb_console_set_visible(1);
        return;
    }

    show_splash();
    mouse_set_handler(desktop_handle_mouse);
    desktop_loop();
    mouse_set_handler(NULL);

    fb_console_set_visible(1);
    console_clear();
    console_write("[gui] Desktop closed.\n");
}
