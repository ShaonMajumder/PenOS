# Demand Paging

## Overview
This commit introduces demand paging to PenOS. Demand paging is a memory management technique where pages are loaded into memory only when they are accessed. This allows for more efficient memory usage and enables features like lazy loading of executables.

## Implementation Details

### Page Fault Handler
A page fault handler (`page_fault_handler`) has been implemented in `src/mem/paging.c`. This handler is triggered by interrupt 14 (Page Fault Exception).

The handler performs the following steps:
1.  **Reads CR2**: The CR2 register contains the linear address that caused the page fault.
2.  **Analyzes Error Code**: The error code pushed onto the stack provides details about the fault (present/not-present, read/write, user/supervisor).
3.  **Checks for Demand Paging Condition**: If the fault was caused by a non-present page (`present` bit clear) and occurred in user mode (`user` bit set), it is treated as a demand paging request.
4.  **Allocates Frame**: A new physical frame is allocated using `alloc_frame_zero()`.
5.  **Maps Page**: The faulting address is aligned to the page boundary, and the new frame is mapped to this virtual address with `PAGE_PRESENT | PAGE_RW | PAGE_USER` flags.
6.  **Resumes Execution**: The handler returns, and the CPU retries the instruction that caused the fault. Since the page is now present, the instruction succeeds.

### Exception Dispatching
The interrupt dispatching logic in `src/arch/x86/interrupts.c` was updated to allow registered handlers to handle exceptions. Previously, any exception (interrupt < 20) would cause an immediate kernel panic. Now, `isr_dispatch` checks if a handler is registered for the exception vector. If so, it calls the handler; otherwise, it panics.

### Initialization
The `paging_init` function now registers the `page_fault_handler` for interrupt 14.

## Benefits
-   **Lazy Allocation**: User space memory is allocated only when actually used.
-   **Robustness**: The system can now handle page faults gracefully instead of crashing immediately (for valid demand paging cases).
-   **Foundation for Future Features**: This is a prerequisite for more advanced memory management features like copy-on-write (fork), memory-mapped files, and swapping.
