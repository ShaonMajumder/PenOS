# PenOS Architecture Overview

PenOS currently boots through GRUB, which loads `kernel.bin` and hands control to `start` in `src/boot.s`. The assembly stub builds a stack and calls `kernel_main`, entering C in 32-bit protected mode.

1. CPU bring-up
   - `gdt_init` loads a flat descriptor table via `gdt_flush`.
   - `idt_init` clears the descriptor table, while `interrupt_init` wires exception and IRQ stubs generated in `src/arch/x86/isr_stubs.S`.
   - The PIC is remapped to vectors 32-47 in `pic_remap` to avoid clashing with CPU exceptions.
   - The PIT is configured at 100 Hz (`timer_init`), incrementing a global tick counter and calling the scheduler stub each interrupt.
   - `console_show_boot_splash` clears the VGA console and renders an ASCII PenOS splash once before normal init logs resume.

2. Memory management
   - `pmm_init` inspects the multiboot info block and computes a bitmap-backed frame allocator that hands out 4 KiB frames and supports freeing.
   - `paging_init` now builds a fresh page directory using frames from the PMM, identity-maps the first 16 MiB, mirrors the kernel into the higher half (0xC0000000+phys), installs a recursive mapping slot, and finally flips CR3/CR0 to enable paging. `paging_map/paging_unmap` expose helpers for future subsystems.
   - `heap_init` reserves a 16 MiB higher-half window (starting at 0xC1000000) and manages it via a doubly-linked free list with boundary tags; `kmalloc` splits free blocks and maps additional pages lazily, while `kfree` returns blocks to the list, coalesces neighbors, and (via tail trimming) unmaps idle pages so frames go back to the PMM. `heap_bytes_in_use/free` supply quick diagnostics.

3. Devices and drivers
   - The keyboard driver registers on IRQ1, translating set-1 scancodes into ASCII and buffering them for consumers (the shell).
   - The mouse driver enables the PS/2 auxiliary port, captures 3-byte packets on IRQ12, updates an internal state struct (delta + button mask), and currently logs movements for debugging; this same state will feed a future GUI or input subsystem.

4. Kernel services
   - `sched/sched.c` now implements a round-robin scheduler with task lifecycle management: finished or killed threads enter a ZOMBIE state, their stacks are released via `kfree`, and shell commands (`ps`, `spawn`, `kill`) drive the lifecycle. Timer interrupts snapshot the active task's `interrupt_frame_t`, pick the next runnable task, and patch the frame so `iret` resumes the chosen thread on its dedicated stack. Demo workers (`counter`, `spinner`) are no longer auto-spawned; they run only when the shell requests them so the console stays quiet by default.
   - `sys/syscall.c` registers an `int 0x80` handler that decodes syscall numbers (EAX) and arguments (EBX/ECX/EDX). Early syscalls cover console write and querying the PIT tick counter.
   - `apps/sysinfo.c` demonstrates how subsystems compose: it queries PMM, timer, and scheduler state before printing via the console.

5. UI and shell
   - `ui/console.c` provides a VGA text console with scrolling, cursor management, and a branded ASCII splash helper that is shown once per boot.
   - `shell/shell.c` blocks on keyboard input, tokenizes simple commands, and now offers task management (`ps`, `spawn <counter|spinner>`, `kill <pid>`) in addition to the diagnostic commands. Because demo tasks are opt-in, the shell prompt is quiet until the user launches them.

## TODO highlights

- Extend the shell, add filesystem plus storage drivers, and integrate GUI or framebuffer output.
- Expand the syscall table (and eventually raise the gate's DPL) so ring-3 processes can invoke richer kernel services.
- Add user-mode tasks plus IPC so the new lifecycle/shell APIs manage more than demo workers.
