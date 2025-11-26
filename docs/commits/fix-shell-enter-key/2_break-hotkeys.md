fix/shell-enter-key

commit message : shell: add esc/ctrl+c break hotkeys

commit description :

Cutting demo tasks used to require running `kill <pid>` while the spinner spammed the console. ESC produced no action and `Ctrl+C` never arrived at the shell because the PS/2 driver ignored modifier scancodes. This change teaches the keyboard driver to track Shift/Ctrl, emit proper control characters (ETX for `Ctrl+C`), and let the shell intercept ESC/`Ctrl+C`. When either hotkey fires, the shell kills any running `spinner`/`counter` threads immediately, prints a short status, and redraws the prompt so users can get back to typing without fighting the spinner output. README + architecture docs now mention the hotkeys so newcomers know how to stop the optional demo workers fast.

What is done in this commit

Keyboard driver:

- Tracks Shift/Ctrl state, exposes shifted characters (upper-case letters, punctuation) and generates ASCII control codes when Ctrl modifies alphabetic keys so `Ctrl+C` becomes `0x03`.

Shell/console:

- `shell_run`'s line editor now treats ESC and `Ctrl+C` as break signals that kill all running `spinner`/`counter` tasks using scheduler APIs and prints how many workers were stopped before re-issuing `PenOS>`.

Docs:

- README quick-start and device-driver sections mention the ESC/`Ctrl+C` hotkeys.
- `docs/architecture.md` records that the keyboard driver emits control codes and the shell uses ESC/`Ctrl+C` to stop demo threads quickly.

Behavioral result:

- Launch `spawn spinner`, tap ESC (or `Ctrl+C`), and the spinner halts immediately with `[shell] stopped 1 demo task(s)` before a fresh `PenOS>` prompt appears—no need to type `kill` while spinner output fights your input.
