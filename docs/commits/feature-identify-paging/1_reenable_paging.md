Branch

feature/identity-paging

Commit message

Enable identity-mapped paging and restore paging_init()

Short commit description (for git log)

Reintroduce paging_init() with a flat 4 GiB identity map so the MMU is enabled while the kernel, heap, framebuffer, and GUI continue to run at their existing virtual addresses.

Commit doc (drop-in Markdown)

Suggested path:
docs/commits/mem/1_identity_paging_enable.md

# Identity-Mapped Paging Initialization

Branch: `feature/identity-paging`  
Commit message: `Enable identity-mapped paging and restore paging_init()`  
Commit summary: Turn paging back on using a simple 1:1 identity map of the 32-bit address space and re-enable `paging_init()` in `kernel_main`, without changing any existing virtual addresses or GUI behavior.

---

## 1. Context & Motivation

Previously, `paging_init()` was disabled during GUI bring-up because the old paging setup expected a higher-half kernel while most of the kernel still assumed identity-mapped addresses. As soon as paging was enabled, pointers to the heap, stacks, and framebuffer became invalid and the system would hang before reaching the shell or `desktop_start()`.

At the same time, we want paging **enabled** so that future work on a higher-half kernel and more advanced memory management can proceed without constantly toggling the MMU on and off.

This commit re-enables paging in a **conservative** way: by installing a flat 1:1 identity map over the entire 32-bit address space. All existing code continues to see the same virtual addresses, but CR0.PG is now set and the MMU is active.

---

## 2. Design Overview

### 2.1 Public paging API

`include/mem/paging.h` now defines a small paging API and common flags:

```c
#define PAGE_SIZE    0x1000
#define PAGE_PRESENT 0x00000001
#define PAGE_RW      0x00000002
#define PAGE_USER    0x00000004
#define KERNEL_VIRT_BASE 0xC0000000

void paging_init(void);
void paging_map(uint32_t virt, uint32_t phys, uint32_t flags);
void paging_unmap(uint32_t virt);
uint32_t paging_virt_to_phys(uint32_t virt);


The KERNEL_VIRT_BASE constant is preserved for future higher-half work but is not used in this commit; all mappings are identity-mapped.

2.2 Identity-mapped page tables

src/mem/paging.c now implements a simple non-PAE, 4 KiB page based layout:

One global page directory with 1024 entries:

static uint32_t page_directory[PAGE_DIRECTORY_ENTRIES]
    attribute((aligned(4096)));


A full set of page tables (1024 × 1024 entries) covering the entire 4 GiB space:

static uint32_t page_tables[PAGE_DIRECTORY_ENTRIES][PAGE_TABLE_ENTRIES]
    attribute((aligned(4096)));


During paging_init():

The page directory and all page tables are zeroed with memset.

For each page directory index pd and page table index pt:

The page directory entry is set to point at page_tables[pd] with PAGE_PRESENT | PAGE_RW.

The page table entry is set to:

uint32_t phys = (pd << 22) | (pt << 12);
page_tables[pd][pt] = phys | PAGE_PRESENT | PAGE_RW;


This creates a full identity mapping: every 4 KiB physical page at phys is mapped to the same virtual address.

load_page_directory() loads the page directory physical address into CR3 and sets the PG bit in CR0 to enable paging.

Helper functions:

paging_map() and paging_unmap() manipulate individual entries in the preallocated tables and invalidate the TLB entry with invlpg.

paging_virt_to_phys() looks up the page table entry and returns the mapped physical address (or 0 if unmapped).

Because the tables are identity-mapped as well, using their virtual addresses when building the directory works as expected.

This design trades some memory (~4 MiB for all tables) for simplicity and safety at this stage.

3. Kernel Initialization Changes

src/kernel.c now includes "mem/paging.h" and calls paging_init() in the normal boot sequence:

gdt_init();
idt_init();
pic_remap();
interrupt_init();
timer_init(100);
pmm_init(mb_info);
paging_init();
heap_init();
keyboard_init();
mouse_init();
syscall_init();
sched_init();


Key points:

pmm_init(mb_info) still runs before paging, so physical memory management is initialized first.

Paging is enabled before the heap, keyboard, mouse, syscalls, and scheduler bring-up, but all of these still see the same addresses as before because of the identity mapping.

The framebuffer and desktop code continue to work unchanged: the linear framebuffer remains reachable at the same virtual address it had before paging was turned on.

GUI entry behavior:

The existing logic that checks fb_present() and optionally calls desktop_start() is preserved.

shell_run() still executes after GUI exit, and ESC remains the escape back to the shell.

4. What This Commit Does Not Do

This change deliberately does not:

Move the kernel to a higher-half virtual address (e.g., 0xC0000000).

Introduce per-process address spaces or user-mode mappings.

Change any existing pointer arithmetic in the kernel.

Touch the framebuffer, scheduler, heap internals, or GUI code.

All virtual addresses remain equal to their physical addresses; the MMU is simply validating accesses via the identity map.

These constraints make it safe to turn paging on without breaking the current GUI and shell.

5. Testing

Build and run:

make iso
qemu-system-i386 -cdrom PenOS.iso


Expected behavior:

PenOS boots to the framebuffer splash and text console as before.

Boot log contains:

Paging enabled with identity mapping.


The shell prompt appears and responds to commands (help, ticks, sysinfo, etc.).

The gui command still launches the PenOS desktop; mouse and keyboard work normally.

Pressing ESC from the GUI returns to the shell and prints [gui] Desktop closed..

With this commit, PenOS now runs with paging enabled using a flat identity map, providing a solid base for future higher-half and virtual-memory work without regressing the existing GUI features.
```
