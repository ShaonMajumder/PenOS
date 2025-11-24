# PenOS Architecture Overview

PenOS currently boots through GRUB, which loads `kernel.bin` and hands control to `start` in `src/boot.s`. The assembly stub builds a stack and calls `kernel_main`, entering C in 32-bit protected mode.

1. CPU bring-up
   - `gdt_init` loads a flat descriptor table via `gdt_flush`.
   - `idt_init` clears the descriptor table, while `interrupt_init` wires exception and IRQ stubs generated in `src/arch/x86/isr_stubs.S`.
   - The PIC is remapped to vectors 32-47 in `pic_remap` to avoid clashing with CPU exceptions.
   - The PIT is configured at 100 Hz (`timer_init`), incrementing a global tick counter and calling the scheduler stub each interrupt.

2. Memory management
   - `pmm_init` inspects the multiboot info block and computes a simple linear frame allocator that hands out 4 KiB frames sequentially above 1 MiB. `pmm_free_frame` is stubbed for future reclamation.
   - `paging_init` identity maps the first 4 MiB via statically reserved page directory/table memory and turns on paging by setting CR3/CR0.

3. Devices and drivers
   - The keyboard driver registers on IRQ1, translating set-1 scancodes into ASCII and buffering them for consumers (the shell).
   - Additional device hooks (mouse, sound, NIC) are intentionally left for future milestones.

4. Kernel services
   - `sched/sched.c` holds a placeholder task table and exposes hook points used by the timer to eventually trigger round-robin switching.
   - `apps/sysinfo.c` demonstrates how subsystems can compose: it queries PMM, timer, and scheduler state before printing via the console.

5. UI and shell
   - `ui/console.c` provides a VGA text console with scrolling and cursor management.
   - `shell/shell.c` blocks on keyboard input, tokenizes simple commands, and dispatches to built-ins and demo apps.

## TODO highlights

- Replace the PMM linear allocator with a bitmap allocator that tracks frees.
- Map the kernel into the higher half and stand up a kernel heap to allocate dynamic structures.
- Flesh out `sched_tick` to context switch tasks built from allocated stacks.
- Extend the shell, add filesystem plus storage drivers, and integrate GUI or framebuffer output.
