fix/shell-enter-key

commit message : input: fix enter/backspace handling in shell

commit description :

Pressing Enter inside the PenOS shell did nothing because the PS/2 driver left the set-1 scancode for the Return key mapped to 0. The shell's line reader stops only on `\r` or `\n`, so users trying to run `spawn counter` / `spawn spinner` could type the command but the newline never arrived and the prompt never advanced. At the same time, the console code treated ASCII 8 like any printable character, so hitting Backspace merely printed a hollow-square glyph instead of moving the cursor left and erasing. This change maps the Enter scancode to `'\n'` and teaches the console to interpret `\b` by moving the cursor back and blanking the previous cell so in-line editing behaves like a normal TTY. The documentation notes for the driver and README device-driver row were refreshed to call out the improved shell behavior.

What is done in this commit

Keyboard driver:

- Maps the Enter scancode in `src/drivers/keyboard.c` to `'\n'` so the shell receives a newline and executes commands when the user presses Enter.

Console:

- `src/ui/console.c` now interprets ASCII backspace by moving the cursor left (wrapping to the previous row when needed) and clearing that cell so character deletion actually removes screen contents.

Docs:

- README device-driver row now mentions the newline + backspace behavior so users know the shell input path behaves like a normal terminal.
- `docs/architecture.md` clarifies that the keyboard driver emits `\n` for Enter and forwards backspace edits while translating set-1 scancodes for the shell.

Behavioral result:

- In QEMU/VirtualBox/real hardware you can type `spawn spinner`, press Enter, and the shell immediately runs the command; while typing, Backspace now erases characters visually instead of leaving stray glyphs.
