# PenOS Functional Overview and Controls

Use this guide to understand how PenOS behaves from power-on through shutdown, plus the hotkeys that let you switch between text and GUI workflows.

## Boot Flow & Modes

1. **GRUB menu**

   - `PenOS (Text mode)` — default entry; passes `penos.mode=text` so the kernel boots directly to the shell.
   - `PenOS (GUI mode)` — passes `penos.mode=gui`; after init the framebuffer desktop auto-starts, then returns to the shell when you press `ESC`.

2. **Kernel initialization**

   - CPU: GDT, IDT, PIC, PIT @100 Hz.
   - Memory: bitmap PMM, higher-half paging, kernel heap.
   - Drivers: PS/2 keyboard and mouse (mouse handler is hooked only when the desktop runs).
   - Scheduler/syscalls: preemptive round-robin, `int 0x80` dispatcher.

3. **Shell availability**

   - Text mode prints `[boot] Text mode selected; use 'gui' to launch desktop.` then shows `PenOS>`.
   - GUI mode launches the desktop first, then the shell continues once you exit the GUI with `ESC`.

## Shell Commands & Behavior

---

| Command                        | Purpose                                 |                                                               |
| ------------------------------ | --------------------------------------- | ------------------------------------------------------------- |
| `help`, `clear`, `echo <text>` | Basic utilities.                        |                                                               |
| `ticks`                        | Show PIT tick counter.                  |                                                               |
| `sysinfo`                      | Print scheduler/memory stats.           |                                                               |
| `ps`                           | List tasks; use with `kill <pid>`.      |                                                               |
| `spawn <counter                | spinner>`                               | Start demo tasks (preemptive proof); stop via `kill` or keys. |
| `gui`                          | Manually start the framebuffer desktop. |                                                               |
| `autogui on/off/status`        | Toggle desktop autostart.               |                                                               |
| `halt`                         | Halt CPU (HLT loop).                    |                                                               |
| `shutdown`                     | ACPI shutdown (QEMU/VirtualBox).        |                                                               |

## Hotkeys & Interaction Rules

- **Shell interrupt**

  - `ESC` or `Ctrl+C` during shell input stops the current line, kills demo tasks, and prints a status message.
  - Useful when a demo task is spamming output.

- **GUI desktop**

  - `gui` command or GUI boot mode calls `desktop_start()`.
  - Mouse is captured; keyboard input goes to the active window.
  - `ESC` exits the desktop, restores the console, and prints `[gui] Desktop closed.`.

- **Autostart toggle**

  - `autogui on` launches the desktop automatically once after boot.
  - `autogui off` keeps the system in text mode until you type `gui`.
  - `autogui status` reports current setting.

- **Shutdown shortcut**

  - Use `shutdown` to trigger ACPI poweroff.
  - `halt` leaves CPU in an infinite `hlt` loop for debugging.

## Typical Usage Patterns

1. **Headless / server-style**

   - Boot text mode.
   - Use `spawn` + `ps` to demo the scheduler.
   - Finish with `shutdown`.

2. **Desktop demo**

   - Boot GUI mode or run `gui` from text mode.
   - Explore splash, icons, and terminal window.
   - `ESC` returns to shell; shut down normally.

3. **Education / debugging**

   - Boot text mode with autogui disabled.
   - Inspect log output during subsystem init.
   - Toggle GUI on demand to show framebuffer capabilities.

This overview lives alongside `README.md` so reviewers can easily see how to operate PenOS without digging through source code.
