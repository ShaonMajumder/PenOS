# Commit 2 - Heap freelist and kfree
**Branch:** feature/heap  \
**Commit:** "Add freelist allocator and kfree"  \
**Summary:** Introduced boundary-tag metadata plus a doubly-linked free list so `kmalloc` reuses blocks and `kfree` coalesces neighbors.

Problem
: The higher-half heap only grew because `kfree` was a stub. Long-lived kernels would leak every allocation, preventing schedulers, IPC, or filesystems from reusing memory.

Solution
: Added boundary-tag metadata to each allocation and introduced a doubly-linked free list. `kmalloc` now searches free blocks before requesting new pages, splitting oversized blocks when possible. `kfree` marks blocks free, validates double frees, and coalesces neighbors to mitigate fragmentation.

Architecture

```
[block header | payload]
block->next/prev form a list ordered by address
kmalloc -> find free block or request new -> split if large
kfree -> mark free -> coalesce prev/next
```

Data structures

- `heap_block_t`: stores size, free flag, and links.
- Global `heap_head` maintains the ordered list regardless of allocation state.

Tradeoffs

- The heap still never returns pages to the PMM; once mapped, pages remain resident.
- Metadata overhead per allocation equals `sizeof(heap_block_t)`.

Interactions

- Paging + PMM untouched except for zeroing new pages.
- Console logs double-free attempts to aid debugging.

Scientific difficulty
: Implementing coalescing correctly requires careful pointer math and alignment while keeping the allocator freestanding.

What to learn
: Even a small freelist dramatically improves system longevity-reclaiming memory enables upcoming subsystems (scheduler stacks, filesystem caches) without rebooting.
