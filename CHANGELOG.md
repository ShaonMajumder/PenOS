# Changelog

## v0.8.0

- Added a Multiboot framebuffer module that initializes the 1024x768x32 mode, draws the PenOS splash/logo, exposes pixel/rect/text/sprite helpers, and pipes the shell console into a tinted overlay without losing the VGA fallback.
- Introduced a `shutdown` shell command backed by a tiny power-off helper so QEMU/Bochs/VirtualBox guests can exit cleanly from the prompt.
- Updated docs (architecture, README, release notes) plus the build (`-DPENOS_VERSION`) so the splash and console know the runtime version string.

## v0.7.0

- Added a PS/2 mouse driver that enables the auxiliary port, parses 3-byte packets on IRQ12, and logs cursor/button state for future GUI work.

## v0.6.0

- Scheduler now reclaims zombie threads (freeing stacks) and exposes ps/spawn/kill shell commands backed by new lifecycle APIs.

## v0.5.1

- Heap now reports usage stats and trims free pages at the higher-half tail, returning frames to the PMM.

## v0.5.0

- Added an `int 0x80` syscall dispatcher with `sys_write` and `sys_ticks`, plus cleaned up PIC EOIs for software interrupts.

## v0.4.0

- Implemented a preemptive round-robin kernel scheduler with dedicated per-task stacks and demo counter/spinner threads.

## v0.3.1

- Extended the kernel heap with boundary-tag metadata, freelist search, and real `kfree` plus block coalescing.

## v0.3.0

- Added a higher-half kernel heap: bump allocator backed by paging/PMM with lazy page mapping and `kmalloc` API.

## v0.2.0

- Rebuilt the paging subsystem: dynamic page directory/tables from the bitmap PMM, identity window for the first 16 MiB, kernel mirrored into the higher half, and exported mapping helpers.

## v0.1.1

- Replace the sequential physical allocator with a bitmap PMM that parses the Multiboot memory map and supports freeing frames.

## v0.1.0

- Initial bootable kernel with console shell, timer, keyboard, and stub memory manager.
