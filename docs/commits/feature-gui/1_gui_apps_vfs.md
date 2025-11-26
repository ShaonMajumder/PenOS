# GUI Apps & VFS – Interactive Desktop

Branch: `feature/gui-apps-vfs`  
Commit message: `Add interactive desktop GUI, in-memory VFS, and GUI shell integration`  
Commit summary: Hook framebuffer + mouse into a clickable PenOS desktop with simple windows, a fake VFS-backed Files view, and a `gui` shell command that cleanly enters and exits the GUI.

This commit is the implementation of the earlier **"GUI Apps & VFS – Follow-Up Prompt"** and builds directly on the framebuffer desktop, mouse plumbing, and `gui` shell hook introduced in previous PenOS feature prompts.

---

## 1. Context & Goals

**Prompt reference**

- Follow-up design: **"GUI Apps & VFS – Follow-Up Prompt"**  
- Global project framing: PenOS is “a simple but real 32-bit x86 operating system” with GRUB+Multiboot, protected-mode kernel, PMM/paging, scheduler, drivers, shell, and a roadmap for GUI, filesystem, networking, and PenScript.

**Starting point (before this commit)**

- GRUB already boots PenOS into a 1024×768×32 linear framebuffer (via the Multiboot VIDEO flag in `src/boot.s`).
- `fb_init()` configures framebuffer access and a text overlay.
- `mouse_init()` enables PS/2 streaming; the kernel can track a global mouse state.
- `desktop_start()` could draw a static background, taskbar, icons, cursor, and a splash screen, and ESC would return to the text shell.
- The shell exposes a `gui` command to launch the compositor, but icons were non-interactive and there were no real GUI “apps” or VFS model behind the Files icon.

**Goals of this commit**

- Detect mouse clicks on the “Apps”, “Settings”, “Recycle Bin”, and “Files” desktop icons.
- Introduce a tiny, in-memory VFS model to back the Files window.
- Implement one-window-at-a-time GUI “apps” with title bars and close buttons.
- Preserve ESC as a guaranteed escape back to the text shell.
- Keep the implementation simple and responsive, without a full window manager.

---

## 2. Architecture Overview

### 2.1 Mouse → Desktop event pipeline

- `src/drivers/mouse.c`
  - Maintains a global `mouse_state_t` (`x`, `y`, `buttons`) updated on each PS/2 IRQ.
  - Exposes `mouse_set_handler(mouse_handler_t)` so higher layers can receive `(dx, dy, buttons)` callbacks without polling.
- `src/ui/desktop.c`
  - Registers `desktop_handle_mouse` as the active mouse handler during GUI mode.
  - Keeps a `desktop_state_t` with:
    - Current screen size (`width`, `height`).
    - Cursor position (`cursor_x`, `cursor_y`) and button state (`buttons`, `prev_buttons`).
    - Active window type (`WIN_NONE`, `WIN_APPS`, `WIN_SETTINGS`, `WIN_RECYCLE`, `WIN_FILES`).
    - A `scene_dirty` flag and an `active` flag to control redraws and the main loop.
  - On each callback:
    - Updates the cursor position, clamped to the framebuffer bounds.
    - Detects a left-button rising edge and calls `process_click()` at the cursor location.
    - Otherwise just marks the scene dirty so the compositor can redraw.

### 2.2 Desktop icons & hit-testing

- Icons are described by:

  ```c
  typedef struct {
      const char *label;
      int x;
      int y;
      icon_id_t id;
  } icon_t;
  ```

- `g_icons[]` defines the four desktop icons: **Apps**, **Settings**, **Recycle Bin**, and **Files**.
- Hit-testing uses a simple rectangle helper:

  ```c
  static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh);
  ```

- On click:
  - If the cursor is inside an icon’s rectangle, `handle_icon_click()` maps the `icon_id_t` to a `window_type_t` and calls `open_window()` with `WIN_APPS`, `WIN_SETTINGS`, `WIN_RECYCLE`, or `WIN_FILES`.

### 2.3 Window model & chrome

- Only one window can be active at a time: `desktop_state_t.window`.
- `open_window(window_type_t type)`:
  - Centers the window of size `WINDOW_W×WINDOW_H` with a slight vertical offset.
  - Handles `WIN_NONE` as “no window”, effectively closing any open panel.
  - Marks the scene dirty so the compositor will repaint.
- `draw_window()`:
  - Draws a shadow, window body, and header bar.
  - Draws a close button (“x”) rectangle in the top-right of the header.
  - Dispatches to content-specific renderers:
    - `WIN_APPS` → `draw_lines_block()` over `g_apps_lines[]`.
    - `WIN_SETTINGS` → `draw_lines_block()` over `g_settings_lines[]`.
    - `WIN_RECYCLE` → `draw_lines_block()` over `g_recycle_lines[]`.
    - `WIN_FILES` → `draw_files_block()` backed by the VFS entries.
- Clicking the close button calls `handle_window_click()`, which checks the close-button rectangle and, if hit, calls `open_window(WIN_NONE)`.

### 2.4 Lightweight in-memory VFS

- `g_vfs_entries[]` is a small, in-memory array of faux files and directories:

  ```c
  typedef struct {
      const char *name;
      const char *type;   // "dir" or "file"
      const char *detail; // e.g., "3 items", "2 KB"
  } vfs_entry_t;
  ```

- `draw_files_block()` iterates this array and formats each entry as a line like:

  ```
  Documents (dir) - 3 items
  ```

  which is then drawn into the Files window.

- There is no persistence or backing storage yet: this is a pure in-memory model used to make the Files window feel real while keeping kernel complexity low. The structure is chosen so it can evolve into a true VFS later.

### 2.5 GUI lifecycle & shell integration

- `desktop_start()`:
  - Checks `fb_present()` and validates framebuffer dimensions.
  - Hides the framebuffer console overlay via `fb_console_set_visible(0)`.
  - Shows a “PenOS Desktop” splash screen for a short delay using `timer_ticks()`.
  - Registers the desktop mouse handler with `mouse_set_handler(desktop_handle_mouse)`.
  - Enters `desktop_loop()`.
- `desktop_loop()`:
  - If `scene_dirty` is set, calls `draw_scene()` (background, icons, active window, taskbar, cursor).
  - Polls `keyboard_read_char()` and exits when ESC (`27`) is pressed.
  - Executes `hlt` in the idle path to avoid spinning the CPU.
- On exit:
  - Clears the mouse handler via `mouse_set_handler(NULL)`.
  - Re-enables the text console overlay with `fb_console_set_visible(1)`.
  - Prints `[gui] Desktop closed.` via the shell.
- `src/shell/shell.c`:
  - Includes `"ui/desktop.h"` and adds the `gui` command.
  - `cmd_gui()` just calls `desktop_start();` and returns when the user exits GUI mode with ESC.
  - Existing commands (`help`, `clear`, `echo`, `ticks`, `sysinfo`, `ps`, `spawn`, `kill`, `halt`, `shutdown`) are unchanged.

---

## 3. Code Implementation (file-by-file)

- `src/boot.s`, `src/kernel.c`
  - Multiboot header now requests the 1024×768×32 framebuffer; `kernel_main` calls `fb_init`, brings up devices, launches the desktop automatically when a framebuffer is present, and still falls back to the text shell.
- `include/drivers/mouse.h`, `src/drivers/mouse.c`
  - Added `mouse_handler_t` plus `mouse_set_handler`, and the ISR now forwards `(dx, dy, buttons)` to the registered handler so the desktop can consume real-time mouse events.
- `include/ui/fb.h`, `src/ui/fb.c`
  - Provide a thin wrapper over the low-level framebuffer driver (`fb_clear/fill/string`) and a helper to hide/show the text console overlay from GUI mode.
- `include/ui/desktop.h`, `src/ui/desktop.c`
  - Implement the splash screen, background, taskbar, icons, hit-testing, VFS data, window chrome, cursor drawing, and the ESC-driven GUI loop that uses `mouse_set_handler`.
- `src/shell/shell.c`
  - Adds the `gui` command and integrates the GUI launch/exit flow with the existing shell prompt.
- `src/ui/framebuffer.c`
  - Text rendering now uses `mask = 0x80u >> col` when walking each glyph row so pixels are plotted left-to-right, fixing the mirrored/rotated framebuffer text that was visible in QEMU.

---

## 4. What You Should Learn

- Event layering: how a low-level PS/2 mouse driver can be kept generic and then wired into a higher-level desktop via a callback.
- Hit-testing basics: simple rectangles are enough to build a usable icon-based desktop before adding a real window manager.
- Mocking complex subsystems: an in-memory VFS table can stand in for a real filesystem while you design UI/UX.
- Mode switching: the shell↔GUI boundary is just a function call and state flips (mouse handler + console overlay), with ESC as an always-available escape.
- Incremental GUI design: one-window-at-a-time panels, shared window chrome, and a simple compositor loop form a solid foundation for future features.

---

## 5. Testing

```sh
make iso
qemu-system-i386 -cdrom PenOS.iso
```

- At `PenOS>` run: `gui`.
- Verify:
  - A brief “PenOS Desktop” splash appears.
  - Desktop shows gradient background, taskbar, four icons (Apps, Settings, Recycle Bin, Files).
  - All text (splash, titles, Files listing) is upright and readable.
  - Clicking icons opens/closes windows as expected.
  - ESC exits back to the shell and prints `[gui] Desktop closed.`.
  - Text-mode shell commands still work as before.
