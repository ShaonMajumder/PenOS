#ifndef UI_DESKTOP_H
#define UI_DESKTOP_H

#include <stdint.h>

/*
 * App kinds supported by the PenOS desktop.  New GUI modules (e.g. terminal)
 * can reference these identifiers without knowing the internal window_mgr.
 */
typedef enum {
    APP_NONE = 0,
    APP_TERMINAL,
    APP_APPS,
    APP_SETTINGS,
    APP_RECYCLE,
    APP_FILES,
    APP_ABOUT
} app_kind_t;

/*
 * Entry point for the PenOS framebuffer desktop.
 *
 * Called from the shell via the "gui" command.
 * If no framebuffer is available, this function will print a message
 * and return immediately without changing console state.
 */
void desktop_start(void);

/*
 * Mouse callback registered with mouse_set_handler() while the desktop
 * is active. Receives mouse deltas and button state and updates the
 * desktop cursor / click handling.
 */
void desktop_handle_mouse(int dx, int dy, uint8_t buttons);

/* Simple notification line rendered on the top bar */
void desktop_notify(const char *message);

/* Autostart flag helpers (used by kernel/shell) */
void desktop_set_autostart(int enabled);
int desktop_autostart_enabled(void);

#endif /* UI_DESKTOP_H */
