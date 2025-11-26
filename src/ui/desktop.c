#include "ui/desktop.h"
#include "ui/fb.h"
#include "ui/console.h"
#include "drivers/mouse.h"
#include "drivers/keyboard.h"
#include "arch/x86/timer.h"
#include <stddef.h>

#define TASKBAR_HEIGHT 52
#define ICON_W 96
#define ICON_H 96
#define CURSOR_W 11
#define CURSOR_H 17
#define WINDOW_W 460
#define WINDOW_H 280
#define SPLASH_DELAY_TICKS 120

typedef enum {
    ICON_APPS,
    ICON_SETTINGS,
    ICON_RECYCLE,
    ICON_FILES,
    ICON_COUNT
} icon_id_t;

typedef enum {
    WIN_NONE,
    WIN_APPS,
    WIN_SETTINGS,
    WIN_RECYCLE,
    WIN_FILES
} window_type_t;

typedef struct {
    const char *label;
    int x;
    int y;
    icon_id_t id;
} icon_t;

typedef struct {
    const char *name;
    const char *type;
    const char *detail;
} vfs_entry_t;

typedef struct {
    int width;
    int height;
    int cursor_x;
    int cursor_y;
    uint8_t buttons;
    uint8_t prev_buttons;
    int active;
    int scene_dirty;
    window_type_t window;
    int window_x;
    int window_y;
} desktop_state_t;

static desktop_state_t g_desktop;

static const icon_t g_icons[ICON_COUNT] = {
    { "Apps",        40,  70, ICON_APPS },
    { "Settings",   170, 70, ICON_SETTINGS },
    { "Recycle Bin",300, 70, ICON_RECYCLE },
    { "Files",      430, 70, ICON_FILES }
};

static const char *g_apps_lines[] = {
    "* System Info",
    "* Scheduler demo (spinner / counter)",
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
    "Soon it will show deleted items"
};

static const vfs_entry_t g_vfs_entries[] = {
    { "Documents", "dir", "3 items" },
    { "Screenshots", "dir", "empty" },
    { "README.txt", "file", "2 KB" },
    { "build.log", "file", "8 KB" },
    { "penos-logo.png", "file", "16 KB" },
    { "TODO.md", "file", "1 KB" }
};

static void append_char(char *dst, size_t *idx, size_t cap, char c)
{
    if (*idx < cap - 1) {
        dst[*idx] = c;
        (*idx)++;
    }
}

static void append_str(char *dst, size_t *idx, size_t cap, const char *text)
{
    while (*text) {
        append_char(dst, idx, cap, *text++);
    }
}

static void draw_string_centered(int x, int y, const char *text, uint32_t fg, uint32_t bg)
{
    (void)bg;
    int cursor = x;
    while (*text) {
        fb_draw_char(cursor, y, *text++, fg, 0x00000000);
        cursor += 8;
    }
}

static void draw_background(void)
{
    fb_clear(0xFF0B1736);
}

static void draw_taskbar(void)
{
    int y = g_desktop.height - TASKBAR_HEIGHT;
    fb_fill_rect(0, y, g_desktop.width, TASKBAR_HEIGHT, 0xFF181C27);
    fb_fill_rect(16, y + 10, 120, TASKBAR_HEIGHT - 20, 0xFF2E8B57);
    fb_draw_string(32, y + 20, "PenOS", 0xFFFFFFFF, 0xFF2E8B57);
    fb_draw_string(160, y + 20, "ESC returns to the shell", 0xFFCBD5F5, 0xFF181C27);
}

static void draw_icon(const icon_t *icon)
{
    fb_fill_rect(icon->x, icon->y, ICON_W, ICON_H, 0xFF233457);
    fb_fill_rect(icon->x + 3, icon->y + 3, ICON_W - 6, ICON_H - 6, 0xFF394B7B);
    fb_draw_string(icon->x + 16, icon->y + ICON_H + 10, icon->label, 0xFFE5E7EB, 0x00000000);
}

static void draw_icons(void)
{
    for (size_t i = 0; i < ICON_COUNT; ++i) {
        draw_icon(&g_icons[i]);
    }
}

static void draw_window_header(const char *title, int close_x, int close_y)
{
    fb_fill_rect(close_x, close_y, 22, 16, 0xFF7F1D1D);
    fb_draw_string(close_x + 6, close_y + 4, "x", 0xFFFFFFFF, 0xFF7F1D1D);
    fb_draw_string(g_desktop.window_x + 12, g_desktop.window_y + 6, title, 0xFFFFFFFF, 0x00000000);
}

static void draw_lines_block(int x, int y, const char **lines, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        fb_draw_string(x, y + (int)(i * 16), lines[i], 0xFFE5E7EB, 0x00000000);
    }
}

static void draw_files_block(int x, int y)
{
    for (size_t i = 0; i < sizeof(g_vfs_entries) / sizeof(g_vfs_entries[0]); ++i) {
        const vfs_entry_t *entry = &g_vfs_entries[i];
        char line[96];
        size_t idx = 0;
        append_str(line, &idx, sizeof(line), entry->name);
        append_char(line, &idx, sizeof(line), ' ');
        append_char(line, &idx, sizeof(line), '(');
        append_str(line, &idx, sizeof(line), entry->type);
        append_char(line, &idx, sizeof(line), ')');
        append_str(line, &idx, sizeof(line), " - ");
        append_str(line, &idx, sizeof(line), entry->detail);
        line[idx] = '\0';
        fb_draw_string(x, y + (int)(i * 16), line, 0xFFE5E7EB, 0x00000000);
    }
}

static void draw_window(void)
{
    if (g_desktop.window == WIN_NONE) {
        return;
    }

    int shadow = 8;
    fb_fill_rect(g_desktop.window_x + shadow, g_desktop.window_y + shadow, WINDOW_W, WINDOW_H, 0x66000000);
    fb_fill_rect(g_desktop.window_x, g_desktop.window_y, WINDOW_W, WINDOW_H, 0xFF1D2434);
    fb_fill_rect(g_desktop.window_x, g_desktop.window_y, WINDOW_W, 26, 0xFF111827);

    int close_x = g_desktop.window_x + WINDOW_W - 28;
    int close_y = g_desktop.window_y + 4;

    switch (g_desktop.window) {
    case WIN_APPS:
        draw_window_header("Apps", close_x, close_y);
        draw_lines_block(g_desktop.window_x + 18, g_desktop.window_y + 40, g_apps_lines, sizeof(g_apps_lines)/sizeof(g_apps_lines[0]));
        break;
    case WIN_SETTINGS:
        draw_window_header("Settings", close_x, close_y);
        draw_lines_block(g_desktop.window_x + 18, g_desktop.window_y + 40, g_settings_lines, sizeof(g_settings_lines)/sizeof(g_settings_lines[0]));
        break;
    case WIN_RECYCLE:
        draw_window_header("Recycle Bin", close_x, close_y);
        draw_lines_block(g_desktop.window_x + 18, g_desktop.window_y + 40, g_recycle_lines, sizeof(g_recycle_lines)/sizeof(g_recycle_lines[0]));
        break;
    case WIN_FILES:
        draw_window_header("Files", close_x, close_y);
        draw_files_block(g_desktop.window_x + 18, g_desktop.window_y + 40);
        break;
    default:
        break;
    }
}

static void draw_cursor(void)
{
    fb_fill_rect(g_desktop.cursor_x, g_desktop.cursor_y, CURSOR_W, CURSOR_H, 0xFFFFFFFF);
    fb_fill_rect(g_desktop.cursor_x + CURSOR_W - 2, g_desktop.cursor_y + CURSOR_H / 2, 6, CURSOR_H / 2, 0xFFFFFFFF);
}

static void draw_scene(void)
{
    draw_background();
    draw_icons();
    draw_window();
    draw_taskbar();
    draw_cursor();
}

static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void open_window(window_type_t type)
{
    if (type == WIN_NONE) {
        g_desktop.window = WIN_NONE;
        g_desktop.scene_dirty = 1;
        return;
    }
    g_desktop.window = type;
    g_desktop.window_x = g_desktop.width / 2 - WINDOW_W / 2;
    g_desktop.window_y = g_desktop.height / 2 - WINDOW_H / 2 - 20;
    if (g_desktop.window_y < 40) {
        g_desktop.window_y = 40;
    }
    g_desktop.scene_dirty = 1;
}

static void handle_icon_click(int x, int y)
{
    for (size_t i = 0; i < ICON_COUNT; ++i) {
        const icon_t *icon = &g_icons[i];
        if (point_in_rect(x, y, icon->x, icon->y, ICON_W, ICON_H)) {
            switch (icon->id) {
            case ICON_APPS:
                open_window(WIN_APPS);
                return;
            case ICON_SETTINGS:
                open_window(WIN_SETTINGS);
                return;
            case ICON_RECYCLE:
                open_window(WIN_RECYCLE);
                return;
            case ICON_FILES:
                open_window(WIN_FILES);
                return;
            default:
                break;
            }
        }
    }
}

static void handle_window_click(int x, int y)
{
    if (g_desktop.window == WIN_NONE) {
        return;
    }
    int close_x = g_desktop.window_x + WINDOW_W - 28;
    int close_y = g_desktop.window_y + 4;
    if (point_in_rect(x, y, close_x, close_y, 22, 16)) {
        open_window(WIN_NONE);
    }
}

static void process_click(int x, int y)
{
    if (g_desktop.window != WIN_NONE) {
        if (point_in_rect(x, y, g_desktop.window_x, g_desktop.window_y, WINDOW_W, WINDOW_H)) {
            handle_window_click(x, y);
            return;
        }
    }
    handle_icon_click(x, y);
}

void desktop_handle_mouse(int dx, int dy, uint8_t buttons)
{
    if (!g_desktop.active) {
        return;
    }
    g_desktop.cursor_x += dx;
    g_desktop.cursor_y += dy;
    if (g_desktop.cursor_x < 0) {
        g_desktop.cursor_x = 0;
    } else if (g_desktop.cursor_x > g_desktop.width - CURSOR_W) {
        g_desktop.cursor_x = g_desktop.width - CURSOR_W;
    }
    if (g_desktop.cursor_y < 0) {
        g_desktop.cursor_y = 0;
    } else if (g_desktop.cursor_y > g_desktop.height - CURSOR_H) {
        g_desktop.cursor_y = g_desktop.height - CURSOR_H;
    }
    g_desktop.prev_buttons = g_desktop.buttons;
    g_desktop.buttons = buttons;
    if (!(g_desktop.prev_buttons & 0x1) && (g_desktop.buttons & 0x1)) {
        process_click(g_desktop.cursor_x, g_desktop.cursor_y);
    } else {
        g_desktop.scene_dirty = 1;
    }
}

static void flush_keyboard(void)
{
    while (keyboard_read_char() != -1) {
        (void)0;
    }
}

static void show_splash(void)
{
    fb_clear(0xFF050C16);
    draw_string_centered(g_desktop.width / 2 - 80, g_desktop.height / 2 - 12, "PenOS Desktop", 0xFF38BDF8, 0);
    draw_string_centered(g_desktop.width / 2 - 120, g_desktop.height / 2 + 12, "Preparing GUI environment...", 0xFF9CA3AF, 0);
    uint64_t start = timer_ticks();
    while (timer_ticks() - start < SPLASH_DELAY_TICKS) {
        __asm__ volatile ("hlt");
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
        if (ch == 27) {
            g_desktop.active = 0;
            break;
        }
        __asm__ volatile ("hlt");
    }
}

void desktop_start(void)
{
    if (!fb_present()) {
        console_write("[gui] Framebuffer not available. Use a graphical target in GRUB.\n");
        return;
    }
    console_write("[gui] Launching desktop (ESC to exit)...\n");
    flush_keyboard();
    fb_console_set_visible(0);

    g_desktop.width = (int)fb_width();
    g_desktop.height = (int)fb_height();
    g_desktop.cursor_x = g_desktop.width / 2;
    g_desktop.cursor_y = g_desktop.height / 2;
    g_desktop.buttons = 0;
    g_desktop.prev_buttons = 0;
    g_desktop.window = WIN_NONE;
    g_desktop.scene_dirty = 1;
    g_desktop.active = (g_desktop.width > 0 && g_desktop.height > 0);

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
