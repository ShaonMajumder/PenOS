# Commit 1 - PS/2 mouse driver
**Branch:** feature/mouse-driver  \
**Commit:** "Add PS/2 mouse driver logging deltas"  \
**Summary:** Enabled the auxiliary PS/2 device, registered an IRQ12 handler, parsed 3-byte packets, and logged/retained cursor+button state for future GUI input.

Problem
: PenOS only handled keyboard input; IRQ12 was idle and there was no infrastructure to capture mouse motion, blocking any GUI or pointer-based demos.

Solution
: Implemented a classic PS/2 mouse driver: enabled the auxiliary port, updated the controller command byte to allow mouse IRQs, sent the enable streaming command, and hooked an IRQ12 handler that assembles 3-byte packets. Each packet updates a tracked `mouse_state_t` and prints the deltas/buttons to the console for quick verification.

Architecture
```
mouse_init() -> enable aux (0xA8) -> tweak command byte -> send 0xF4
IRQ12 -> mouse_irq() -> collect 3 bytes -> mouse_handle_packet()
                              -> update global state + log movement
```

Data structures
- `mouse_state_t` captures cumulative X/Y and a bitmask of pressed buttons.
- The driver retains the latest state and exposes `mouse_get_state` for future consumers.

Tradeoffs
- Logging every packet is noisy but invaluable while a GUI is absent; callers can disable it later.
- The state is global; eventual multi-threaded input subsystems will need buffering/locking.

Interactions
- `interrupts_init` now calls `mouse_init` after the IDT gates are installed, using the existing `register_interrupt_handler` path for IRQ12.
- The console output demonstrates the driver works even without a framebuffer.

Scientific difficulty
: Getting the PS/2 command sequencing right (waiting on status bits before writing) and remembering to send commands via port 0xD4 was key; a bad sequence silently drops the mouse.

What to learn
: Even in text-only OS builds you can validate mouse input by logging packets?laying the groundwork now avoids scrambling when GUI work begins.
