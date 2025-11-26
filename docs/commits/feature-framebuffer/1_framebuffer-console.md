# Commit 1 - Framebuffer splash + console overlay
**Branch:** feature/framebuffer-console  \
**Commit:** "Add framebuffer splash, console overlay, and shutdown command"  \
**Summary:** Initialize the GRUB-provided framebuffer, expose drawing primitives (pixel/rect/text/sprite), project the text console into a tinted overlay, render the PenOS splash/logo, and add a shell-level shutdown command.

Problem
: PenOS still ran exclusively in VGA text mode even though GRUB already configured a 1024x768x32 framebuffer. GUI work could not begin, the boot splash had to stay ASCII, and there was no way to power off the emulator from the shell.

Solution
: Added a dedicated framebuffer module that validates the Multiboot mode, stores its geometry, and publishes helpers for drawing pixels, rectangles, bitmap sprites, and bitmap-font text at arbitrary scales. The console now mirrors characters into a shadow buffer so the framebuffer backend can paint them inside a reserved overlay region without dropping VGA compatibility. A new `framebuffer_render_splash` routine paints a gradient background, blits a stylized PenOS mark, prints release text (fed by a Makefile-provided `PENOS_VERSION` define), and outlines the console panel. A tiny power helper writes the common ACPI shutdown ports and backs the new `shutdown` shell command.

Architecture
```
kernel_main()
  framebuffer_init(mb_info)
  framebuffer_console_configure(80,25)
  console_init() -> framebuffer_console_attach()
  console_show_boot_splash(PENOS_VERSION)
framebuffer_render_splash()
  gradient + stripe background
  sprite via framebuffer_blit_sprite()
  draws overlay bounds
console_putc()
  updates VGA memory + shadow buffer
  framebuffer_console_draw_cell() when overlay active
shell 'shutdown' -> power_shutdown() -> ACPI ports -> hlt loop
```

Data structures
- `framebuffer_info_t` captures the linear framebuffer geometry while `framebuffer_console_state_t` tracks columns/rows, cell sizes, and overlay placement.
- The console keeps a 80x25 `shadow_buffer` so scroll/clear/backspace operations can refresh the overlay in one sweep.

Tradeoffs
- The overlay simply repaints entire glyph rectangles per character update; it is not yet double-buffered, but the simplicity keeps the code approachable and fast enough for a shell.
- The splash currently draws static art; animation/mouse cursor rendering will build on these primitives next.

Interactions
- `Makefile` now defines `PENOS_VERSION` (derived from the `VERSION` file) so both the framebuffer splash and ASCII fallback can print the release string without parsing files at runtime.
- `shell/shell.c` advertises the new `shutdown` command, and `README`/`docs/architecture.md` describe the framebuffer initialization path plus console coexistence.

Scientific difficulty
: Mapping the legacy VGA text flow onto a framebuffer overlay without rewriting the console required a careful mirroring strategy so scrolling and cursor management stayed correct across both outputs.

What to learn
: Start GUI bring-up by mirroring existing text systems; it keeps the kernel usable while graphics primitives mature and gives you immediate feedback that the framebuffer math is correct.
