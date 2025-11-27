# Framebuffer Double Buffering

Branch: feature/gui-double-buffering
Commit message: Add framebuffer double buffering and frame markers
Commit summary: Replace direct VRAM writes with a 1024×768 back-buffer, expose fb_begin_frame/fb_end_frame hooks, and wrap the PenOS desktop compositor so each frame is drawn off-screen then blitted, eliminating flicker.

Feature summary:

- Added a static software back-buffer in `src/ui/framebuffer.c`. All drawing routines (pixels, rects, glyphs, console, splash) now paint into the buffer, and `framebuffer_present()` copies the buffer to hardware once per frame.
- Updated `include/ui/framebuffer.h`, `include/ui/fb.h`, and `src/ui/fb.c` to surface the new `framebuffer_present()` plus `fb_begin_frame()` / `fb_end_frame()` wrappers while keeping the existing fb\_\* API intact.
- Wrapped `draw_scene()` in `src/ui/desktop.c` with the new frame markers so the GUI renders atomically.
- Removed the libc `memcpy` dependency by blitting manually to satisfy freestanding build restrictions.

Architecture explanation:
The hardware framebuffer is still initialized from Multiboot, but a fixed 1024×768 ARGB array now serves as the canonical render target. `framebuffer_present()` performs the only hardware write, copying prepared pixels row-by-row. The desktop compositor issues drawing commands exactly as before; the only change is that it brackets each frame with begin/end calls so the swap happens once per scene. This keeps input/event handling untouched while preventing mid-frame tearing.

Code implementation (key files):

- `include/ui/framebuffer.h`: added `framebuffer_present()` declaration.
- `src/ui/framebuffer.c`: introduced back-buffer storage, helper routines, and rewrote drawing ops to operate on the buffer; added manual blit loop.
- `include/ui/fb.h`: declared `fb_begin_frame()` / `fb_end_frame()`.
- `src/ui/fb.c`: implemented the new calls and kept existing helpers.
- `src/ui/desktop.c`: invoked `fb_begin_frame()` / `fb_end_frame()` around `draw_scene()`.

What I should learn:

- How double buffering hides intermediate drawing artifacts on a bare-metal framebuffer.
- How to extend a graphics abstraction without changing higher-level GUI logic.
- Why freestanding kernels avoid libc calls like `memcpy` and how to replace them with simple loops.
