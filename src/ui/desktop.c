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
    if (*idx + 1 >= capacity)
    {
        return;
    }
    buf[*idx] = c;
    (*idx)++;
}

static void append_str(char *buf, size_t *idx, size_t capacity, const char *str)
{
    while (*str)
    {
        append_char(buf, idx, capacity, *str++);
    }
}

/* ---- Layout constants --------------------------------------------------- */

#define TOP_BAR_HEIGHT 32
#define STATUS_ORB_RADIUS 26
#define APP_GRID_COLS 2
#define APP_GRID_ROWS 3
#define APP_GRID_BUTTON_W 84
#define APP_GRID_BUTTON_H 70
#define APP_GRID_PAD 14
#define APP_GRID_WIDTH (APP_GRID_COLS * (APP_GRID_BUTTON_W + APP_GRID_PAD) + APP_GRID_PAD)
#define TASK_STRIP_HEIGHT 48
#define TASK_BUTTON_WIDTH 160
#define TASK_BUTTON_HEIGHT 30
#define TASK_STRIP_MARGIN 10
#define CONTEXT_MENU_WIDTH 180
#define CONTEXT_MENU_ITEM_HEIGHT 20
#define CONTEXT_MENU_PADDING 6
#define CURSOR_W 11
#define CURSOR_H 17
#define DEFAULT_WINDOW_W 640
#define DEFAULT_WINDOW_H 360
#define TITLEBAR_HEIGHT 28
#define SPLASH_DELAY_TICKS 120
#define WINDOW_SHADOW_OFFSET_X 6
#define WINDOW_SHADOW_OFFSET_Y 10
#define MIN_FRAME_TICKS 4

#define CLOSE_BTN_DIAMETER 18
#define CLOSE_BTN_MARGIN 10

#define FILE_ROW_HEIGHT 18

#define MAX_WINDOWS 8

/* ---- Desktop models ----------------------------------------------------- */

typedef struct
{
    const char *label;
    app_kind_t kind;
} launcher_item_t;

typedef struct
{
    const char *label;
    app_kind_t kind;
    int exit_action;
} context_action_t;

typedef struct
{
    const char *name;
    const char *type;
    const char *detail;
} vfs_entry_t;

typedef struct desktop_window
{
    int id;
    app_kind_t kind;
    int x, y;
    int w, h;
    int z; /* z-order; higher draws on top */
    int visible;
    int has_focus;

    int dragging;
    int drag_offset_x;
    int drag_offset_y;

    terminal_buffer_t terminal; /* used when kind == APP_TERMINAL */
    int selected_index;         /* used by Files app */
    int minimized;
} desktop_window_t;

typedef struct
{
    int width;
    int height;
    int cursor_x;
    int cursor_y;
    uint8_t buttons;
    uint8_t prev_buttons;
    int active;
    int scene_dirty;
} desktop_state_t;

/* ---- Static desktop data ------------------------------------------------ */

static desktop_state_t g_desktop;
static desktop_window_t g_windows[MAX_WINDOWS];
static int g_next_window_id = 1;
static int g_next_z = 1;
static int g_spawn_offset = 0;
static app_kind_t g_focused_kind = APP_NONE;
static int g_autostart = 1;
static char g_notification[96] = "Welcome to PenOS";
static uint64_t g_notification_set_ticks = 0;
static uint64_t g_last_draw_ticks = 0;
static struct
{
    int visible;
    int x;
    int y;
} g_context_menu = {0};

static const launcher_item_t g_launcher[] = {
    {"Terminal", APP_TERMINAL},
    {"Files", APP_FILES},
    {"Settings", APP_SETTINGS},
    {"Recycle", APP_RECYCLE},
    {"About", APP_ABOUT},
};
#define LAUNCHER_COUNT (int)(sizeof(g_launcher) / sizeof(g_launcher[0]))

static const context_action_t g_context_actions[] = {
    {"Open Terminal", APP_TERMINAL, 0},
    {"Open Files", APP_FILES, 0},
    {"Settings", APP_SETTINGS, 0},
    {"About PenOS", APP_ABOUT, 0},
    {"Exit GUI", APP_NONE, 1},
};
#define CONTEXT_ACTION_COUNT (int)(sizeof(g_context_actions) / sizeof(g_context_actions[0]))

static void context_menu_open(int x, int y);
static void context_menu_close(void);
static int context_menu_hit_test(int x, int y);
static void context_menu_activate(int index);
static void draw_context_menu(void);
static void draw_taskstrip(void);
static int handle_taskstrip_click(int x, int y);
static app_kind_t shortcut_for_char(int ch);

static const char *g_apps_lines[] = {
    "* System Info snapshot",
    "* Scheduler demo (spinner/counter)",
    "* Future PenScript shell",
    "",
    "Apps list lives here."};

static const char *g_settings_lines[] = {
    "Theme: PenOS Ubuntu remix",
    "Resolution: 1024 x 768",
    "Kernel Heap: higher-half freelist",
    "Scheduler: preemptive RR @100Hz",
    "",
    "Settings are read-only in this build."};

static const char *g_recycle_lines[] = {
    "Recycle Bin is empty.",
    "",
    "Soon it will show deleted items."};

static const char *g_about_lines[] = {
    "PenOS Desktop v1",
    "",
    "Ubuntu-inspired GUI running in a bare-metal framebuffer.",
    "Mouse + keyboard driven, ESC returns to shell.",
    "",
    "This build showcases dock launchers, focus, and selection."};

static const vfs_entry_t g_vfs_entries[] = {
    {"Documents", "dir", "3 items"},
    {"Screenshots", "dir", "empty"},
    {"README.txt", "file", "2 KB"},
    {"build.log", "file", "8 KB"},
    {"penos-logo.png", "file", "16 KB"},
    {"TODO.md", "file", "1 KB"},
};
#define VFS_ENTRY_COUNT (int)(sizeof(g_vfs_entries) / sizeof(g_vfs_entries[0]))

void desktop_notify(const char *message)
{
    if (!message)
    {
        return;
    }
    size_t len = strlen(message);
    if (len >= sizeof(g_notification))
    {
        len = sizeof(g_notification) - 1;
    }
    memcpy(g_notification, message, len);
    g_notification[len] = '\0';
    g_desktop.scene_dirty = 1;
    g_notification_set_ticks = timer_ticks();
}

void desktop_set_autostart(int enabled)
{
    g_autostart = enabled ? 1 : 0;
}

int desktop_autostart_enabled(void)
{
    return g_autostart;
}


/* ---- Utility helpers ---------------------------------------------------- */

static app_kind_t shortcut_for_char(int ch)
{
    switch (ch)
    {
    case '!':
        return APP_TERMINAL;
    case '@':
        return APP_FILES;
    case '#':
        return APP_SETTINGS;
    case '$':
        return APP_RECYCLE;
    case '%':
        return APP_ABOUT;
    default:
        return APP_NONE;
    }
}

static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return (x >= rx && x < rx + rw && y >= ry && y < ry + rh);
}

static const char *app_title(app_kind_t kind)
{
    switch (kind)
    {
    case APP_TERMINAL:
        return "Terminal";
    case APP_APPS:
        return "System Info";
    case APP_SETTINGS:
        return "Settings";
    case APP_RECYCLE:
        return "Recycle Bin";
    case APP_FILES:
        return "Files";
    case APP_ABOUT:
        return "About PenOS";
    default:
        return "";
    }
}

static void format_clock(char *buf, size_t len)
{
    if (len < 6)
    {
        if (len)
            buf[0] = '\0';
        return;
    }
    uint32_t tick32 = (uint32_t)timer_ticks();
    uint32_t seconds = tick32 / 100U;
    uint32_t minutes = (uint32_t)(seconds / 60);
    uint32_t hours = (minutes / 60) % 24;
    minutes %= 60;
    buf[0] = '0' + (hours / 10);
    buf[1] = '0' + (hours % 10);
    buf[2] = ':';
    buf[3] = '0' + (minutes / 10);
    buf[4] = '0' + (minutes % 10);
    buf[5] = '\0';
}

static desktop_window_t *desktop_find_window_by_kind(app_kind_t kind)
{
    if (kind == APP_NONE)
    {
        return NULL;
    }
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        if (g_windows[i].visible && g_windows[i].kind == kind)
        {
            return &g_windows[i];
        }
    }
    return NULL;
}

static desktop_window_t *desktop_find_free_slot(void)
{
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        if (!g_windows[i].visible && g_windows[i].kind == APP_NONE)
        {
            return &g_windows[i];
        }
    }
    return NULL;
}

static desktop_window_t *desktop_find_window_at(int x, int y)
{
    desktop_window_t *best = NULL;
    int best_z = -1;
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        desktop_window_t *w = &g_windows[i];
        if (!w->visible || w->minimized)
        {
            continue;
        }
        if (!point_in_rect(x, y, w->x, w->y, w->w, w->h))
        {
            continue;
        }
        if (w->z > best_z)
        {
            best_z = w->z;
            best = w;
        }
    }
    return best;
}

static desktop_window_t *desktop_focused_window(void)
{
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        if (g_windows[i].visible && g_windows[i].has_focus)
        {
            return &g_windows[i];
        }
    }
    return NULL;
}

static void desktop_focus_window(desktop_window_t *win)
{
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        g_windows[i].has_focus = 0;
    }
    if (!win || !win->visible)
    {
        g_focused_kind = APP_NONE;
        return;
    }
    win->has_focus = 1;
    win->z = g_next_z++;
    g_focused_kind = win->kind;
    g_desktop.scene_dirty = 1;
}

static void desktop_close_window(desktop_window_t *win)
{
    if (!win)
    {
        return;
    }
    if (win->kind == g_focused_kind)
    {
        g_focused_kind = APP_NONE;
    }
    memset(win, 0, sizeof(*win));
    g_desktop.scene_dirty = 1;
}

static void clamp_window_to_screen(desktop_window_t *win)
{
    if (!win || win->w <= 0 || win->h <= 0)
    {
        return;
    }
    int left_margin = APP_GRID_WIDTH + APP_GRID_PAD * 2;
    int min_x = left_margin;
    int max_x = g_desktop.width - 12 - win->w;
    if (max_x < min_x)
    {
        max_x = min_x;
    }
    int min_y = TOP_BAR_HEIGHT + 12;
    int max_y = g_desktop.height - TASK_STRIP_HEIGHT - 12 - win->h;
    if (max_y < min_y)
    {
        max_y = min_y;
    }
    if (win->x < min_x)
        win->x = min_x;
    if (win->x > max_x)
        win->x = max_x;
    if (win->y < min_y)
        win->y = min_y;
    if (win->y > max_y)
        win->y = max_y;
}

static desktop_window_t *desktop_spawn_window(app_kind_t kind)
{
    if (kind == APP_NONE)
    {
        return NULL;
    }
    desktop_window_t *existing = desktop_find_window_by_kind(kind);
    if (existing)
    {
        existing->minimized = 0;
        desktop_focus_window(existing);
        return existing;
    }
    desktop_window_t *slot = desktop_find_free_slot();
    if (!slot)
    {
        console_write("[gui] No free window slots\n");
        return NULL;
    }

    memset(slot, 0, sizeof(*slot));
    slot->id = g_next_window_id++;
    slot->kind = kind;
    slot->w = DEFAULT_WINDOW_W;
    slot->h = DEFAULT_WINDOW_H;
    slot->selected_index = -1;
    slot->minimized = 0;

    int left_margin = APP_GRID_WIDTH + APP_GRID_PAD * 2;
    int available_width = g_desktop.width - left_margin - 24 - WINDOW_SHADOW_OFFSET_X;
    if (available_width < 160)
    {
        available_width = 160;
    }
    slot->w = available_width;
    int base_x = left_margin + (available_width - slot->w) / 2;
    if (base_x < left_margin)
    {
        base_x = left_margin;
    }
    int available_height = g_desktop.height - TOP_BAR_HEIGHT - TASK_STRIP_HEIGHT - 24 - WINDOW_SHADOW_OFFSET_Y;
    if (available_height < 160)
    {
        available_height = 160;
    }
    slot->h = available_height;
    int base_y = TOP_BAR_HEIGHT + 12 + (available_height - slot->h) / 2;
    int top_margin = TOP_BAR_HEIGHT + 12;
    if (base_y < top_margin)
    {
        base_y = top_margin;
    }
    int offset = g_spawn_offset;
    g_spawn_offset = (g_spawn_offset + 28) % 140;

    slot->x = base_x + offset;
    slot->y = base_y + offset;
    slot->visible = 1;
    slot->z = g_next_z++;
    slot->has_focus = 1;

    if (kind == APP_TERMINAL)
    {
        terminal_init(&slot->terminal);
    }

    clamp_window_to_screen(slot);
    desktop_focus_window(slot);
    char note[48];
    size_t pos = 0;
    append_str(note, &pos, sizeof(note), "Opened ");
    append_str(note, &pos, sizeof(note), app_title(kind));
    note[pos] = '\0';
    desktop_notify(note);
    return slot;
}

/* ---- Launcher, system tile, taskstrip ----------------------------------- */

static int launcher_button_rect(int index, int *rx, int *ry, int *rw, int *rh)
{
    if (index < 0 || index >= LAUNCHER_COUNT)
    {
        return 0;
    }
    int col = index % APP_GRID_COLS;
    int row = index / APP_GRID_COLS;
    int start_x = APP_GRID_PAD;
    int start_y = TOP_BAR_HEIGHT + 60;
    int x = start_x + col * (APP_GRID_BUTTON_W + APP_GRID_PAD);
    int y = start_y + row * (APP_GRID_BUTTON_H + APP_GRID_PAD);
    if (rx)
        *rx = x;
    if (ry)
        *ry = y;
    if (rw)
        *rw = APP_GRID_BUTTON_W;
    if (rh)
        *rh = APP_GRID_BUTTON_H;
    return 1;
}

static int launcher_hit_test(int x, int y)
{
    for (int i = 0; i < LAUNCHER_COUNT; ++i)
    {
        int rx, ry, rw, rh;
        launcher_button_rect(i, &rx, &ry, &rw, &rh);
        if (point_in_rect(x, y, rx, ry, rw, rh))
        {
            return i;
        }
    }
    return -1;
}

static void draw_launcher(void)
{
    int hover = launcher_hit_test(g_desktop.cursor_x, g_desktop.cursor_y);
    for (int i = 0; i < LAUNCHER_COUNT; ++i)
    {
        int rx, ry, rw, rh;
        launcher_button_rect(i, &rx, &ry, &rw, &rh);
        desktop_window_t *win = desktop_find_window_by_kind(g_launcher[i].kind);
        int focused = (win && win->has_focus) || (g_focused_kind == g_launcher[i].kind);
        uint32_t base = focused ? 0xFF1B3A4B : 0xFF142530;
        if (hover == i)
        {
            base = 0xFF224357;
        }
        fb_fill_rect(rx - 2, ry - 2, rw + 4, rh + 4, 0x77000000);
        fb_fill_rect(rx, ry, rw, rh, base);

        fb_draw_string(rx + 8, ry + rh / 2 - 6, g_launcher[i].label, 0xFFE5E7EB, 0);

        if (win && win->visible)
        {
            fb_draw_string(rx + 6, ry + rh - 14, win->minimized ? "-" : "*", focused ? 0xFF3ADAD9 : 0xFF4F8C8C, 0);
        }
    }
}

static void context_menu_open(int x, int y)
{
    int width = CONTEXT_MENU_WIDTH;
    int height = CONTEXT_ACTION_COUNT * CONTEXT_MENU_ITEM_HEIGHT + CONTEXT_MENU_PADDING * 2;
    if (x + width > g_desktop.width)
    {
        x = g_desktop.width - width;
    }
    if (y + height > g_desktop.height)
    {
        y = g_desktop.height - height;
    }
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    g_context_menu.visible = 1;
    g_context_menu.x = x;
    g_context_menu.y = y;
    g_desktop.scene_dirty = 1;
}

static void context_menu_close(void)
{
    if (!g_context_menu.visible)
        return;
    g_context_menu.visible = 0;
    g_desktop.scene_dirty = 1;
}

static int context_menu_hit_test(int x, int y)
{
    if (!g_context_menu.visible)
        return -1;
    int width = CONTEXT_MENU_WIDTH;
    int height = CONTEXT_ACTION_COUNT * CONTEXT_MENU_ITEM_HEIGHT + CONTEXT_MENU_PADDING * 2;
    if (!point_in_rect(x, y, g_context_menu.x, g_context_menu.y, width, height))
    {
        return -1;
    }
    int rel_y = y - (g_context_menu.y + CONTEXT_MENU_PADDING);
    if (rel_y < 0)
        return -1;
    int idx = rel_y / CONTEXT_MENU_ITEM_HEIGHT;
    if (idx >= 0 && idx < CONTEXT_ACTION_COUNT)
    {
        return idx;
    }
    return -1;
}

static void context_menu_activate(int index)
{
    if (index < 0 || index >= CONTEXT_ACTION_COUNT)
        return;
    const context_action_t *action = &g_context_actions[index];
    if (action->exit_action)
    {
        desktop_notify("Exiting GUI...");
        g_desktop.active = 0;
        return;
    }
    desktop_window_t *win = desktop_spawn_window(action->kind);
    if (win && win->minimized)
    {
        win->minimized = 0;
        desktop_focus_window(win);
    }
}

static void draw_context_menu(void)
{
    if (!g_context_menu.visible)
        return;
    int width = CONTEXT_MENU_WIDTH;
    int height = CONTEXT_ACTION_COUNT * CONTEXT_MENU_ITEM_HEIGHT + CONTEXT_MENU_PADDING * 2;
    fb_fill_rect(g_context_menu.x, g_context_menu.y, width, height, 0xF0151F27);
    int hover = context_menu_hit_test(g_desktop.cursor_x, g_desktop.cursor_y);
    for (int i = 0; i < CONTEXT_ACTION_COUNT; ++i)
    {
        int item_y = g_context_menu.y + CONTEXT_MENU_PADDING + i * CONTEXT_MENU_ITEM_HEIGHT;
        if (i == hover)
        {
            fb_fill_rect(g_context_menu.x, item_y, width, CONTEXT_MENU_ITEM_HEIGHT, 0xFF1F3C4B);
        }
        fb_draw_string(g_context_menu.x + 10, item_y + 4, g_context_actions[i].label, 0xFFE5E7EB, 0);
    }
}

static int task_button_rect(int index, int *rx, int *ry, int *rw, int *rh)
{
    int y = g_desktop.height - TASK_STRIP_HEIGHT + 6;
    int x = TASK_STRIP_MARGIN + index * (TASK_BUTTON_WIDTH + TASK_STRIP_MARGIN);
    if (rx) *rx = x;
    if (ry) *ry = y;
    if (rw) *rw = TASK_BUTTON_WIDTH;
    if (rh) *rh = TASK_BUTTON_HEIGHT;
    return 1;
}

static void draw_system_tile(void)
{
    int tile_w = 150;
    int tile_h = 24;
    int x = g_desktop.width - tile_w - 12;
    int y = 4;
    fb_fill_rect(x, y, tile_w, tile_h, 0xFF132228);
    fb_fill_rect(x + 10, y + 6, 26, 12, 0xFFE0E0E0);
    fb_fill_rect(x + 36, y + 10, 3, 4, 0xFFE0E0E0);

    int wifi_x = x + 50;
    for (int arc = 0; arc < 3; ++arc)
    {
        fb_draw_string(wifi_x + arc * 6, y + 6 + arc, "~", 0xFFAED8EA, 0);
    }

    char clock_buf[6];
    format_clock(clock_buf, sizeof(clock_buf));
    fb_draw_string(x + tile_w - 40, y + 5, clock_buf, 0xFFE5E7EB, 0);
}

static void draw_status_orb(void)
{
    int cx = g_desktop.width / 2;
    int cy = TOP_BAR_HEIGHT / 2;
    for (int dy = -STATUS_ORB_RADIUS; dy <= STATUS_ORB_RADIUS; ++dy)
    {
        for (int dx = -STATUS_ORB_RADIUS; dx <= STATUS_ORB_RADIUS; ++dx)
        {
            if (dx * dx + dy * dy <= STATUS_ORB_RADIUS * STATUS_ORB_RADIUS)
            {
                uint32_t color = 0xFF158A9A;
                if (dx * dx + dy * dy > (STATUS_ORB_RADIUS - 3) * (STATUS_ORB_RADIUS - 3))
                {
                    color = 0xFF1090A0;
                }
                fb_put_pixel(cx + dx, cy + dy, color);
            }
        }
    }
    fb_draw_string(cx - 8, cy - 4, "P", 0xFFFFFFFF, 0);
}

static void draw_top_bar(void)
{
    fb_fill_rect(0, 0, g_desktop.width, TOP_BAR_HEIGHT, 0xFF061018);
    uint64_t age = timer_ticks() - g_notification_set_ticks;
    if (age > 500 && g_notification[0] != '\0')
    {
        g_notification[0] = '\0';
        g_desktop.scene_dirty = 1;
    }
    fb_draw_string(APP_GRID_WIDTH + 30, 8, g_notification, 0xFFE5E7EB, 0);
    draw_system_tile();
    draw_status_orb();
}

/* ---- Drawing ------------------------------------------------------------ */

static void draw_background(void)
{
    for (int y = 0; y < g_desktop.height; ++y)
    {
        uint8_t mix = (uint8_t)((y * 140) / g_desktop.height);
        uint32_t base = 0xFF0F1F24;
        base += ((uint32_t)mix << 8);
        for (int x = 0; x < g_desktop.width; ++x)
        {
            uint32_t color = base;
            if (((x + y) & 0x1F) == 0)
            {
                color = 0xFF123C46;
            }
            fb_put_pixel(x, y, color);
        }
    }
    fb_fill_rect(APP_GRID_PAD, TOP_BAR_HEIGHT + 48, APP_GRID_WIDTH + APP_GRID_PAD, g_desktop.height - (TOP_BAR_HEIGHT + 96), 0x22182634);
}

static void draw_lines_block(int x, int y, const char *const *lines, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        fb_draw_string(x, y + (int)(i * 16), lines[i], 0xFFE5E7EB, 0);
    }
}

static void draw_files_block(const desktop_window_t *win, int x, int y, int w)
{
    for (int i = 0; i < VFS_ENTRY_COUNT; ++i)
    {
        const vfs_entry_t *entry = &g_vfs_entries[i];
        int row_y = y + i * FILE_ROW_HEIGHT;
        if (win && win->selected_index == i)
        {
            fb_fill_rect(x - 6, row_y - 2, w + 12, FILE_ROW_HEIGHT, 0x33E95420);
        }
        char line[96];
        size_t idx = 0;
        append_str(line, &idx, sizeof(line), entry->name);
        append_str(line, &idx, sizeof(line), " (");
        append_str(line, &idx, sizeof(line), entry->type);
        append_str(line, &idx, sizeof(line), ") - ");
        append_str(line, &idx, sizeof(line), entry->detail);
        line[idx] = '\0';
        fb_draw_string(x, row_y, line, 0xFFE5E7EB, 0);
    }
}

static void draw_window(const desktop_window_t *win)
{
    if (!win->visible || win->kind == APP_NONE)
    {
        return;
    }

    fb_fill_rect(win->x + WINDOW_SHADOW_OFFSET_X,
                 win->y + WINDOW_SHADOW_OFFSET_Y,
                 win->w,
                 win->h,
                 0x44000000);

    uint32_t body = 0xFF1C1E24;
    fb_fill_rect(win->x, win->y, win->w, win->h, body);

    uint32_t title_color = win->has_focus ? 0xFFE95420 : 0xFFBF3F18;
    fb_fill_rect(win->x, win->y, win->w, TITLEBAR_HEIGHT, title_color);
    fb_draw_string(win->x + 16, win->y + 6, app_title(win->kind), 0xFFFFFFFF, title_color);

    int close_x = win->x + win->w - CLOSE_BTN_DIAMETER - CLOSE_BTN_MARGIN;
    int close_y = win->y + (TITLEBAR_HEIGHT - CLOSE_BTN_DIAMETER) / 2;
    int radius = CLOSE_BTN_DIAMETER / 2;
    for (int dy = 0; dy < CLOSE_BTN_DIAMETER; ++dy)
    {
        for (int dx = 0; dx < CLOSE_BTN_DIAMETER; ++dx)
        {
            int cx = dx - radius;
            int cy = dy - radius;
            if (cx * cx + cy * cy <= radius * radius)
            {
                fb_put_pixel(close_x + dx, close_y + dy, 0xFFCE3C3C);
            }
        }
    }
    fb_draw_string(close_x + radius - 3, close_y + radius - 6, "x", 0xFFFFFFFF, 0);

    int content_x = win->x + 24;
    int content_y = win->y + TITLEBAR_HEIGHT + 18;
    int content_w = win->w - 48;
    int content_h = win->h - TITLEBAR_HEIGHT - 32;

    switch (win->kind)
    {
    case APP_TERMINAL:
        terminal_draw(&win->terminal, content_x, content_y, content_w, content_h, 0xFFE5E7EB, 0xFF050A12);
        break;
    case APP_APPS:
        draw_lines_block(content_x, content_y, g_apps_lines, sizeof(g_apps_lines) / sizeof(g_apps_lines[0]));
        break;
    case APP_SETTINGS:
        draw_lines_block(content_x, content_y, g_settings_lines, sizeof(g_settings_lines) / sizeof(g_settings_lines[0]));
        break;
    case APP_RECYCLE:
        draw_lines_block(content_x, content_y, g_recycle_lines, sizeof(g_recycle_lines) / sizeof(g_recycle_lines[0]));
        break;
    case APP_FILES:
        draw_files_block(win, content_x, content_y, content_w);
        break;
    case APP_ABOUT:
        draw_lines_block(content_x, content_y, g_about_lines, sizeof(g_about_lines) / sizeof(g_about_lines[0]));
        break;
    default:
        break;
    }
}

static void draw_windows(void)
{
    int order[MAX_WINDOWS];
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        order[i] = i;
    }

    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        for (int j = i + 1; j < MAX_WINDOWS; ++j)
        {
            if (g_windows[order[i]].z > g_windows[order[j]].z)
            {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    for (int k = 0; k < MAX_WINDOWS; ++k)
    {
        if (!g_windows[order[k]].minimized)
        {
            draw_window(&g_windows[order[k]]);
        }
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
    fb_begin_frame();
    draw_background();
    draw_windows();
    draw_top_bar();
    draw_launcher();
    draw_taskstrip();
    draw_context_menu();
    draw_cursor();
    fb_end_frame();
}

/* ---- Dock interaction --------------------------------------------------- */

static void files_handle_body_click(desktop_window_t *win, int x, int y)
{
    if (!win)
    {
        return;
    }
    int content_x = win->x + 24;
    int content_y = win->y + TITLEBAR_HEIGHT + 18;
    int content_w = win->w - 48;
    int content_h = VFS_ENTRY_COUNT * FILE_ROW_HEIGHT;
    if (!point_in_rect(x, y, content_x, content_y, content_w, content_h))
    {
        win->selected_index = -1;
        g_desktop.scene_dirty = 1;
        return;
    }
    int relative = y - content_y;
    int idx = relative / FILE_ROW_HEIGHT;
    if (idx >= 0 && idx < VFS_ENTRY_COUNT)
    {
        win->selected_index = idx;
        char note[64];
        size_t pos = 0;
        append_str(note, &pos, sizeof(note), "Selected ");
        append_str(note, &pos, sizeof(note), g_vfs_entries[idx].name);
        note[pos] = '\0';
        desktop_notify(note);
        g_desktop.scene_dirty = 1;
    }
}

static void handle_window_click(int x, int y)
{
    desktop_window_t *win = desktop_find_window_at(x, y);
    if (!win)
    {
        desktop_focus_window(NULL);
        return;
    }

    int close_x = win->x + win->w - CLOSE_BTN_DIAMETER - CLOSE_BTN_MARGIN;
    int close_y = win->y + (TITLEBAR_HEIGHT - CLOSE_BTN_DIAMETER) / 2;
    if (point_in_rect(x, y, close_x, close_y, CLOSE_BTN_DIAMETER, CLOSE_BTN_DIAMETER))
    {
        if (win->minimized)
        {
            win->minimized = 0;
        }
        else
        {
            desktop_close_window(win);
        }
        return;
    }

    desktop_focus_window(win);
    if (point_in_rect(x, y, win->x, win->y, win->w, TITLEBAR_HEIGHT))
    {
        win->dragging = 1;
        win->drag_offset_x = x - win->x;
        win->drag_offset_y = y - win->y;
    }
    else if (win->kind == APP_FILES)
    {
        files_handle_body_click(win, x, y);
    }
}

static void process_click(int x, int y)
{
    if (g_context_menu.visible)
    {
        int idx = context_menu_hit_test(x, y);
        if (idx >= 0)
        {
            context_menu_activate(idx);
            context_menu_close();
            return;
        }
        context_menu_close();
    }

    if (handle_taskstrip_click(x, y))
    {
        return;
    }

    int launcher_index = launcher_hit_test(x, y);
    if (launcher_index >= 0)
    {
        desktop_window_t *win = desktop_spawn_window(g_launcher[launcher_index].kind);
        if (win && win->minimized)
        {
            win->minimized = 0;
            desktop_focus_window(win);
        }
        return;
    }

    desktop_window_t *win = desktop_find_window_at(x, y);
    if (win)
    {
        handle_window_click(x, y);
        return;
    }
}

static desktop_window_t *desktop_drag_window(void)
{
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        if (g_windows[i].visible && g_windows[i].dragging)
        {
            return &g_windows[i];
        }
    }
    return NULL;
}

/* ---- Input handling ----------------------------------------------------- */

void desktop_handle_mouse(int dx, int dy, uint8_t buttons)
{
    if (!g_desktop.active)
    {
        return;
    }

    g_desktop.cursor_x += dx;
    g_desktop.cursor_y -= dy; /* PS/2 Y axis is inverted */

    if (g_desktop.cursor_x < 0)
        g_desktop.cursor_x = 0;
    if (g_desktop.cursor_y < 0)
        g_desktop.cursor_y = 0;
    if (g_desktop.cursor_x > g_desktop.width - CURSOR_W)
        g_desktop.cursor_x = g_desktop.width - CURSOR_W;
    if (g_desktop.cursor_y > g_desktop.height - CURSOR_H)
        g_desktop.cursor_y = g_desktop.height - CURSOR_H;

    uint8_t prev = g_desktop.buttons;
    g_desktop.prev_buttons = prev;
    g_desktop.buttons = buttons;

    int left_prev = (prev & 0x1) != 0;
    int left_now = (buttons & 0x1) != 0;
    int right_prev = (prev & 0x2) != 0;
    int right_now = (buttons & 0x2) != 0;

    if (!right_prev && right_now)
    {
        desktop_window_t *hit = desktop_find_window_at(g_desktop.cursor_x, g_desktop.cursor_y);
        int on_launcher = launcher_hit_test(g_desktop.cursor_x, g_desktop.cursor_y) >= 0;
        int on_task = g_desktop.cursor_y >= g_desktop.height - TASK_STRIP_HEIGHT;
        if (!hit && !on_launcher && !on_task)
        {
            context_menu_open(g_desktop.cursor_x, g_desktop.cursor_y);
        }
        else
        {
            context_menu_close();
        }
    }

    if (!left_prev && left_now)
    {
        process_click(g_desktop.cursor_x, g_desktop.cursor_y);
    }

    desktop_window_t *drag_win = desktop_drag_window();
    if (drag_win && left_now)
    {
        drag_win->x = g_desktop.cursor_x - drag_win->drag_offset_x;
        drag_win->y = g_desktop.cursor_y - drag_win->drag_offset_y;
        clamp_window_to_screen(drag_win);
        g_desktop.scene_dirty = 1;
    }

    if (drag_win && !left_now)
    {
        drag_win->dragging = 0;
    }
}

static void desktop_handle_key(int ch)
{
    app_kind_t shortcut = shortcut_for_char(ch);
    if (shortcut != APP_NONE)
    {
        desktop_window_t *win = desktop_spawn_window(shortcut);
        if (win && win->minimized)
        {
            win->minimized = 0;
            desktop_focus_window(win);
        }
        return;
    }

    desktop_window_t *focus = desktop_focused_window();
    if (!focus)
    {
        return;
    }
    if (focus->kind == APP_TERMINAL)
    {
        terminal_put_char(&focus->terminal, (char)ch);
        g_desktop.scene_dirty = 1;
    }
}

static void flush_keyboard(void)
{
    while (keyboard_read_char() != -1)
    {
        /* discard buffered keys */
    }
}

static void show_splash(void)
{
    fb_clear(0xFF050C16);
    fb_draw_string(g_desktop.width / 2 - 80, g_desktop.height / 2 - 12, "PenOS Desktop", 0xFF38BDF8, 0);
    fb_draw_string(g_desktop.width / 2 - 120, g_desktop.height / 2 + 12, "Preparing GUI environment...", 0xFF9CA3AF, 0);

    uint64_t start = timer_ticks();
    while (timer_ticks() - start < SPLASH_DELAY_TICKS)
    {
        __asm__ volatile("hlt");
    }
}

static void desktop_loop(void)
{
    while (g_desktop.active)
    {
        uint64_t now = timer_ticks();
        if (g_desktop.scene_dirty && (g_last_draw_ticks == 0 || now - g_last_draw_ticks >= MIN_FRAME_TICKS))
        {
            draw_scene();
            g_desktop.scene_dirty = 0;
            g_last_draw_ticks = now;
        }

        int ch = keyboard_read_char();
        if (ch == 27)
        { /* ESC */
            g_desktop.active = 0;
            break;
        }
        else if (ch != -1)
        {
            desktop_handle_key(ch);
        }

        __asm__ volatile("hlt");
    }
}

/* ---- Entry point -------------------------------------------------------- */

void desktop_start(void)
{
    if (!fb_present())
    {
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

    memset(g_windows, 0, sizeof(g_windows));
    g_next_window_id = 1;
    g_next_z = 1;
    g_spawn_offset = 0;
    g_focused_kind = APP_NONE;
    strncpy(g_notification, "Welcome to PenOS", sizeof(g_notification) - 1);
    g_notification[sizeof(g_notification) - 1] = '\0';
    g_notification_set_ticks = timer_ticks();
    context_menu_close();

    if (!g_desktop.active)
    {
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
static void draw_taskstrip(void)
{
    fb_fill_rect(0, g_desktop.height - TASK_STRIP_HEIGHT, g_desktop.width, TASK_STRIP_HEIGHT, 0xFF09121A);
    int index = 0;
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        if (!g_windows[i].visible)
        {
            continue;
        }
        int rx, ry, rw, rh;
        task_button_rect(index++, &rx, &ry, &rw, &rh);
        uint32_t color = g_windows[i].has_focus ? 0xFF1D3A4B : 0xFF14212A;
        if (point_in_rect(g_desktop.cursor_x, g_desktop.cursor_y, rx, ry, rw, rh))
        {
            color = 0xFF264B5F;
        }
        fb_fill_rect(rx, ry, rw, rh, color);
        fb_draw_string(rx + 10, ry + 8, app_title(g_windows[i].kind), 0xFFE5E7EB, 0);
        if (g_windows[i].minimized)
        {
            fb_draw_string(rx + rw - 16, ry + 8, "-", 0xFFE5E7EB, 0);
        }
    }
}
static int handle_taskstrip_click(int x, int y)
{
    if (y < g_desktop.height - TASK_STRIP_HEIGHT)
    {
        return 0;
    }
    int index = 0;
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        if (!g_windows[i].visible)
        {
            continue;
        }
        int rx, ry, rw, rh;
        task_button_rect(index++, &rx, &ry, &rw, &rh);
        if (!point_in_rect(x, y, rx, ry, rw, rh))
        {
            continue;
        }
        desktop_window_t *win = &g_windows[i];
        if (win->minimized)
        {
            win->minimized = 0;
            desktop_focus_window(win);
        }
        else if (win->has_focus)
        {
            win->minimized = 1;
            win->has_focus = 0;
            if (g_focused_kind == win->kind)
            {
                g_focused_kind = APP_NONE;
            }
        }
        else
        {
            desktop_focus_window(win);
        }
        g_desktop.scene_dirty = 1;
        return 1;
    }
    return 0;
}
