# Commit 1 - Bitmap physical memory manager

Commit message: “Implement bitmap physical memory manager”.
Feature summary: Replaced the naive sequential PMM with a bitmap allocator that understands Multiboot memory regions, reserves firmware/kernel space, exposes freeing, and keeps logging intact.
Architecture explanation: Introduced a 1-bit-per-frame bitmap, guarded by helpers, and reworked initialization to parse the firmware map twice—first to size the allocator, then to clear only usable frames. Allocation walks from a hint to keep scans short; freeing clears the bit and updates the hint.
Code implementation: See src/mem/pmm.c plus new docs and version bump files above.
Diff focus:
Added bitmap helpers and two-pass Multiboot parsing.
Updated pmm_alloc_frame/pmm_free_frame semantics.
Added commit + version documentation.
What to learn: Firmware-provided maps must drive memory ownership decisions; once paging/heap features arrive, a reusable frame allocator becomes foundational.
Merged back into develop with semantic bump to v0.1.1, and documented in CHANGELOG.md plus docs/versions/v0.1.1.md.

Problem
: The sequential allocator in `src/mem/pmm.c` ignored the Multiboot memory map, could not free frames, and would happily hand out RAM that overlapped firmware or the kernel image.

Solution
: Implemented a bitmap-backed PMM sized for up to 4 GiB. The initializer now reads the Multiboot memory map, reserves the kernel/first MiB, and releases only genuinely available regions. `pmm_alloc_frame` scans for an unused frame starting from a hint, marks it in the bitmap, and `pmm_free_frame` clears the bit for later reuse.

Architecture

```
Multiboot mmap --> pmm_init
                     |-> bitmap_fill (mark all used)
                     |-> release_region (clear usable frames)
alloc/free <--> bitmap test/set helpers
```

Data structures

- `frame_bitmap[]`: 1 bit per 4 KiB frame, covering up to 1,048,576 frames.
- `base_usable_frame`: floor that keeps firmware/kernal/page tables reserved.
- `search_hint`: simple cursor that reduces allocation scan latency.

Tradeoffs

- Bitmap lives in `.bss` (128 KiB) which is acceptable for now.
- Allocation is a linear scan; future work could add buddy allocators or free lists if fragmentation becomes an issue.

Interactions

- Paging can now request frames without worrying about overlapping the kernel image.
- `app_sysinfo` reports the same total memory API but now reflects the real platform size.

Scientific difficulty
: Carefully translating Multiboot's packed memory map entries, preserving reserved regions, and keeping the allocator entirely freestanding required precise pointer arithmetic and guard rails against overflow.

What to learn
: Even early kernels benefit from ?real? allocators?parsing firmware maps and tracking ownership is mandatory once paging, heaps, or drivers need deterministic RAM management.
