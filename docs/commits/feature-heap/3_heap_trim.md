# Commit 3 - Heap trimming + stats
**Branch:** feature/heap-trim  \
**Commit:** "Trim heap tail and report stats"  \
**Summary:** Added `heap_bytes_in_use/free` plus a tail-trimming routine that unmaps idle higher-half pages and returns their frames to the PMM when the freelist releases the top blocks.

Problem
: The higher-half heap could free blocks but never gave pages back to the PMM, so long-idle systems still consumed the full mapped region and there was no way to inspect allocator usage.

Solution
: After each `kfree`, the allocator now coalesces and walks to the tail: if the highest block(s) are free, it pops them from the list, adjusts `heap_curr`, unmaps pages beyond the new boundary, and calls `pmm_free_frame`. Two helper APIs expose the number of bytes in use vs. reserved for diagnostics.

Architecture
```
heap_head -> ... -> tail (free?)
            |-- if free: remove tail, shrink heap_curr
            |-- align heap_curr to page boundary
            |-- while mapped_end > target:
                 phys = paging_virt_to_phys
                 paging_unmap + pmm_free_frame
```

Data structures
- Adds a global byte counter plus a helper to find the list tail; no external bookkeeping changes required.

Tradeoffs
- Only the outermost tail is reclaimed; large free regions in the middle remain mapped until they reach the top via natural churn.
- Trimming happens synchronously inside `kfree`, which is acceptable for now given the small heap.

Interactions
- Paging layer supplies `paging_virt_to_phys/unmap`, PMM reclaims frames, and future diagnostics can read the new stats.

Scientific difficulty
: Ensuring the freelist manipulation stayed consistent while removing the tail blocks (and unmapping their storage) required careful ordering so we never touch unmapped memory.

What to learn
: Keeping the heap monotonic and tracking only the tail makes reclaiming idle pages trivial?even simple freelists can return RAM to the system without switching to a more complex allocator.
