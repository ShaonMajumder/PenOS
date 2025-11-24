# Commit 1 - Bootstrap protected-mode kernel

Commit summary: laid down GRUB boot path, CPU init (GDT/IDT/PIC), interrupt dispatch, PIT timer, sequential PMM + identity paging, keyboard driver, console UI, shell, sysinfo demo, and docs. See commit doc for architecture notes.

docs/commits/feature-core/1_bootstrap.md plus docs/versions/v0.1.0.md.

Problem
: Need a bootable baseline that enters C via Multiboot, configures CPU structures, turns on interrupts, and exposes minimal device plus shell services.

Solution
: Implemented a freestanding build with GRUB-friendly entry, linker script, and stack stub. Added GDT/IDT setup, PIC remapping, PIT tick driver, interrupt dispatch glue, and a console-backed shell. Memory management currently consists of a sequential physical frame allocator plus identity-mapped paging. The scheduler hooks run on the timer, while the keyboard driver feeds the shell.

Architecture

```
GRUB -> boot.s -> kernel_main
          |-> gdt/idt/pic -> interrupts -> timer/keyboard
          |-> pmm -> paging -> console/shell -> demo apps
```

Data structures

- `interrupt_frame_t`: layout matching the stack frame built in assembly.
- `task_t`: placeholder metadata for future context switching.
- Sequential PMM indices representing 4 KiB frames.

Tradeoffs

- PMM currently never frees.
- Paging maps only the first 4 MiB.
- Scheduler stub only counts tasks.

Interactions

- Timer IRQ increments ticks and notifies the scheduler stub.
- Keyboard IRQ populates a ring buffer consumed by the shell.
- `app_sysinfo` queries PMM, timer, and scheduler state.

Scientific difficulty
: Aligning low level assembly stubs with the C interrupt frame and keeping PIC remap plus paging sequencing correct.

What to learn
: A minimal yet real OS needs disciplined layering. Each subsystem is deliberately simple but real, enabling incremental future growth.
