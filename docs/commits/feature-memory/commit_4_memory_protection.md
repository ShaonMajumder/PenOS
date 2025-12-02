# Commit 1: Limited Page-Level Protection

## Overview
This commit enhances memory protection by enforcing granular permissions on kernel sections and enabling the Write Protect (WP) bit in CR0.

## Changes
- **`linker.ld`**: Added symbols (`_text_start`, `_text_end`, `_rodata_start`, etc.) to define boundaries of kernel sections.
- **`src/mem/paging.c`**:
  - Updated `map_kernel_higher_half()` to use these symbols.
  - Mapped `.text` and `.rodata` as **Read-Only** (Supervisor).
  - Mapped `.data` and `.bss` as **Read-Write** (Supervisor).
  - Enabled **CR0.WP (Write Protect)** bit in `paging_init()`. This ensures that even the kernel (Ring 0) cannot write to Read-Only pages, catching bugs like buffer overflows in code segments.

## Security Improvements
- **Code Integrity**: Kernel code (`.text`) is now read-only. Accidental writes to code will trigger a Page Fault.
- **Constant Protection**: Read-only data (`.rodata`) is protected from modification.
- **Supervisor Only**: All kernel mappings are marked as Supervisor-only (no `PAGE_USER` flag), preventing user-mode access to kernel memory.

## Verification
- Verified that the kernel still boots.
- Attempting to write to a string literal or function pointer in the kernel should now cause a Page Fault (Int 14).
