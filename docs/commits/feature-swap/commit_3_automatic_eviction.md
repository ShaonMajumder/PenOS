# Commit 3: Automatic Page Eviction

## Overview
This commit implements automatic page eviction (swapping) when the system runs out of physical memory.

## Changes
- **`src/mem/paging.c`**:
  - Implemented `paging_evict_page()` using the **Clock Algorithm** (Second Chance).
  - Modified `alloc_frame_zero()` to trigger eviction when `pmm_alloc_frame()` fails.
  - The eviction logic scans user-space pages (0-3GB), checking the "Accessed" bit.
    - If Accessed = 1: Clear bit, give second chance.
    - If Accessed = 0: Swap out page to disk.

## How it works
1. When `alloc_frame_zero()` is called (e.g., during a page fault) and PMM returns 0 (OOM).
2. It calls `paging_evict_page()`.
3. The scanner iterates through the Page Directory and Page Tables.
4. It finds a "victim" page that hasn't been accessed recently.
5. It calls `paging_swap_out(victim)` to write the page to disk and free the physical frame.
6. `alloc_frame_zero()` retries allocation, which should now succeed.

## Verification
- The system can now handle memory pressure by swapping pages to disk.
- If swap space is full or no victim is found, the kernel will panic (safe fail).
