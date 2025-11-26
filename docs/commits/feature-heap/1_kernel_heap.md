# Commit 1 - Higher-half kernel heap
**Branch:** feature/heap  \
**Commit:** "Wire up higher-half kernel heap"  \
**Summary:** Reserved a higher-half heap window, implemented a bump allocator on top of paging, and logged the new memory story for v0.3.0.

Problem
: With paging in place, subsystems still lacked a dynamic allocator. `kmalloc` was unavailable, and every feature needing runtime buffers would have to hand-roll fixed arrays.

Solution
: Reserved a 16 MiB window starting at 0xC1000000 for the kernel heap. `heap_init` seeds a bump pointer, and `kmalloc` aligns requests, ensures the backing pages are mapped lazily via `paging_map`, and advances the pointer. `kfree` remains a stub, clearly marked for future work.

Architecture

```
HEAP_START = KERNEL_VIRT_BASE + 0x01000000
heap_curr ---------->
            [mapped as needed]
 pmm_alloc_frame() --> paging_map(virt, frame, RW)
```

Data structures

- `heap_curr` / `heap_mapped_end` track the used and mapped ranges.
- The heap uses the bitmap PMM plus paging helpers, ensuring allocations never overlap kernel text/data.

Tradeoffs

- No free list yet; `kfree` is a stub so memory only grows.
- Page-sized growth keeps mapping simple but means small allocations still cost whole pages eventually.

Interactions

- `kernel_main` calls `heap_init` after paging so future subsystems can safely `kmalloc`.
- The console logs when the heap comes online, aiding early bring-up debugging.

Scientific difficulty
: Needed to carefully manage virtual vs. physical addresses and guarantee that the allocator only touches higher-half space already mirrored through paging.

What to learn
: Even a minimal bump allocator vastly improves ergonomics once paging exists. By carving the heap out of a dedicated higher-half region, we keep the door open for future per-CPU heaps or slab allocators without rewriting every caller.
