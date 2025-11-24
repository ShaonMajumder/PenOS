# PenOS Architecture Overview

PenOS currently boots through GRUB, which loads `kernel.bin` and hands control to `start` in `src/boot.s`. The assembly stub builds a stack and calls `kernel_main`, entering C in 32-bit protected mode.

1. CPU bring-up
   - `gdt_init` loads a flat descriptor table via `gdt_flush`.
   - `idt_init` clears the descriptor table, while `interrupt_init` wires exception and IRQ stubs generated in `src/arch/x86/isr_stubs.S`.
   - The PIC is remapped to vectors 32-47 in `pic_remap` to avoid clashing with CPU exceptions.
   - The PIT is configured at 100 Hz (`timer_init`), incrementing a global tick counter and calling the scheduler stub each interrupt.

2. Memory management
   - `pmm_init` inspects the multiboot info block and computes a bitmap-backed frame allocator that hands out 4 KiB frames and supports freeing.
   - `paging_init` now builds a fresh page directory using frames from the PMM, identity-maps the first 16 MiB, mirrors the kernel into the higher half (0xC0000000+phys), installs a recursive mapping slot, and finally flips CR3/CR0 to enable paging. `paging_map/paging_unmap` expose helpers for future subsystems.
   - `heap_init` reserves a 16 MiB higher-half window (starting at 0xC1000000) and manages it via a doubly-linked free list with boundary tags; `kmalloc` splits free blocks and maps additional pages lazily, while `kfree` returns blocks to the list and coalesces neighbors.

3. Devices and drivers
   - The keyboard driver registers on IRQ1, translating set-1 scancodes into ASCII and buffering them for consumers (the shell).
   - Additional device hooks (mouse, sound, NIC) are intentionally left for future milestones.

4. Kernel services
   - `sched/sched.c` now implements a round-robin scheduler. Timer interrupts snapshot the active task's `interrupt_frame_t`, pick the next runnable task, and patch the frame so `iret` resumes the chosen thread on its dedicated stack. Demo counter/spinner threads show the preemption in action.
   - `apps/sysinfo.c` demonstrates how subsystems compose: it queries PMM, timer, and scheduler state before printing via the console.

5. UI and shell
   - `ui/console.c` provides a VGA text console with scrolling and cursor management.
   - `shell/shell.c` blocks on keyboard input, tokenizes simple commands, and dispatches to built-ins and demo apps.

## TODO highlights

- Heap freelist currently lacks trimming back to the PMM; future work could unmap idle pages or add slab allocators.
- Flesh out the scheduler to reclaim finished tasks and allow user-created threads.
- Extend the shell, add filesystem plus storage drivers, and integrate GUI or framebuffer output.
