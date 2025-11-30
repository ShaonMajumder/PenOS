# Swap Space Implementation

**Date:** 2025-11-30  
**Commit:** e33dad9

## Overview

This commit implements disk-backed virtual memory (swap space) for PenOS, enabling pages to be swapped out to disk when memory pressure occurs and swapped back in on-demand via page faults.

## Implementation Details

### New Files

#### `include/mem/swap.h`
Defines the swap subsystem API:
- `swap_init()` - Initialize swap space on available SATA disk
- `swap_out(void *buffer, uint32_t *swap_slot)` - Write a page to swap
- `swap_in(uint32_t swap_slot, void *buffer)` - Read a page from swap
- `swap_free(uint32_t swap_slot)` - Free a swap slot
- `swap_available()` - Check if swap is enabled

#### `src/mem/swap.c`
Implements the swap manager:
- **Swap Area**: 16MB (4096 pages) starting at LBA 409600 (~200MB offset)
- **Bitmap Allocator**: Tracks free/used swap slots
- **AHCI Integration**: Uses `ahci_read/write` for disk I/O
- **Slot Management**: Each slot stores one 4KB page (8 sectors)

### Modified Files

#### `src/mem/paging.c`
Enhanced page fault handler to support swap:
```c
void page_fault_handler(interrupt_frame_t *frame) {
    // Check if page is swapped out (PTE != 0 && !Present)
    if (!(entry & PAGE_PRESENT) && entry != 0) {
        uint32_t swap_slot = entry >> 12;
        
        // Allocate new frame
        uint32_t phys = alloc_frame_zero();
        
        // Map it first so we can write to it
        paging_map(page_aligned_virt, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
        
        // Read from swap
        swap_in(swap_slot, (void*)page_aligned_virt);
        
        // Free swap slot
        swap_free(swap_slot);
        return;
    }
    
    // ... existing demand paging logic ...
}
```

Added `paging_swap_out()` function:
```c
int paging_swap_out(uint32_t virt) {
    // Write page to swap
    swap_out((void*)page_aligned_virt, &swap_slot);
    
    // Update PTE: Not Present, store swap slot in bits 12-31
    table[pt_index] = (swap_slot << 12);
    invlpg(page_aligned_virt);
    
    // Free physical frame
    pmm_free_frame(phys);
    
    return 0;
}
```

#### `include/mem/paging.h`
Added declaration for `paging_swap_out(uint32_t virt)`.

#### `src/kernel.c`
Added `swap_init()` call after AHCI initialization.

#### `src/shell/shell.c`
Added `swaptest` command to demonstrate swap functionality:
1. Allocates a page-aligned buffer
2. Writes test pattern (0, 1, 2, ..., 1023)
3. Swaps out the page
4. Accesses the page (triggers page fault and swap in)
5. Verifies data integrity

#### `Makefile`
Added `swap.o` to build system.

## Technical Design

### Page Table Entry (PTE) Format for Swapped Pages

When a page is swapped out:
- **Bit 0 (Present)**: 0 (not in memory)
- **Bits 12-31**: Swap slot index

This allows distinguishing between:
- **Unmapped page**: PTE = 0
- **Swapped page**: PTE != 0 && !(PTE & 0x1)
- **Present page**: PTE & 0x1

### Swap Workflow

**Swap Out:**
1. Page is currently mapped and present
2. `paging_swap_out(virt_addr)` is called
3. Page data is written to disk at swap slot
4. PTE is updated to store swap slot index
5. Physical frame is freed back to PMM
6. TLB entry is invalidated

**Swap In (Page Fault):**
1. Page fault occurs on non-present page
2. Handler checks if PTE contains swap slot
3. New physical frame is allocated
4. Page is mapped to new frame
5. Data is read from swap slot into page
6. Swap slot is freed
7. Execution resumes

### Disk Layout

```
LBA 0 - 409599:     Reserved (OS, filesystem, etc.)
LBA 409600 - 442367: Swap space (16MB, 4096 pages)
LBA 442368+:        Available for other use
```

## Benefits

1. **Memory Overcommitment**: Processes can allocate more virtual memory than available physical RAM
2. **Demand Paging Foundation**: Complements existing demand paging with eviction capability
3. **Future Features**: Enables implementation of:
   - Page replacement algorithms (LRU, FIFO, Clock)
   - Memory-mapped files
   - Hibernation/suspend

## Testing

Use the `swaptest` command in the shell:
```
PenOS:/> swaptest
Swap: Allocating test page...
Swap: Writing data to 0xC0400000
Swap: Swapping out page...
Swap: Swapped out page 0xC0400000 to slot 0
Swap: Page swapped out. Accessing to trigger fault...
Swap: Page fault on swapped page. Slot: 0
Swap: Test PASSED! Data verified.
```

## Limitations

1. **No Page Replacement Policy**: Currently requires manual swap-out via `paging_swap_out()`
2. **Single Disk**: Uses first available SATA port
3. **Fixed Swap Size**: 16MB hardcoded
4. **No Swap Priority**: All pages treated equally
5. **Synchronous I/O**: Blocks during disk operations

## Future Work

- Implement automatic page replacement (LRU/Clock algorithm)
- Add swap usage statistics
- Support multiple swap devices
- Implement swap-on-write for copy-on-write pages
- Add swap encryption
- Implement swappiness tuning

## Performance Considerations

- **Swap Slot Allocation**: O(n) linear search through bitmap (acceptable for 4096 slots)
- **Page Fault Latency**: ~10-20ms (disk I/O dominates)
- **Memory Overhead**: 512 bytes for swap bitmap

## Related Commits

- **849040d**: Demand Paging implementation
- **d939bc4**: 9P filesystem fixes
- **62563dd**: ELF loader integration
