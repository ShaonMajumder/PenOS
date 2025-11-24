# Changelog

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
