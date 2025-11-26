# <img src="penos-favicon.svg" alt="PenOS" style="height: 1em; vertical-align: middle;"> PenOS

PenOS is a compact yet real 32-bit x86 operating system intended for learning and portfolio demonstrations. GRUB loads a Multiboot-compliant kernel that brings up protected-mode infrastructure, configures paging and heaps in the higher half, enables interrupts, shows a branded ASCII splash inspired by penos-boot-splash.svg, and drops you at a quiet text-mode shell. Demo tasks (counter/spinner) are opt-in via shell commands so the default console stays calm.

![Boot splash](penos-boot-splash.svg)

## Quick Start

Requirements: `gcc` with 32-bit support, `ld`, `grub-mkrescue`, `xorriso`, and `qemu-system-i386`.

```
make            # build build/kernel.bin
make iso        # produce PenOS.iso (GRUB BIOS image)
make run        # boot the ISO in QEMU
```

Boot `PenOS.iso` in QEMU/VirtualBox/VMware or dd it to a BIOS-capable USB stick. The flow is splash -> init log -> `PenOS>` prompt. Shell commands: `help`, `clear`, `echo <text>`, `ticks`, `sysinfo`, `ps`, `spawn <counter|spinner>`, `kill <pid>`, `halt`. Use `spawn ...` when you want the preemptive demo threads to emit `[counter] tick N` or spinner glyphs; by default there is no background chatter.

## Feature Map

| Area | What you get today | Deep-dive docs | Key sources |
| --- | --- | --- | --- |
| Boot & CPU bring-up | GRUB + `boot.s` + `kernel_main`, flat GDT/IDT, PIC remap, PIT @100 Hz, ASCII splash before logs | [`docs/architecture.md`](docs/architecture.md#1-cpu-bring-up), [`docs/commits/feature-core/1_bootstrap.md`](docs/commits/feature-core/1_bootstrap.md) | `src/boot.s`, `src/arch/x86/gdt.c`, `src/arch/x86/idt.c`, `src/arch/x86/timer.c` |
| Memory management | Bitmap PMM, dynamic paging with higher-half mirror + recursive slot, higher-half heap with freelist + trimming | [`docs/architecture.md`](docs/architecture.md#2-memory-management), [`docs/commits/feature-pmm/1_bitmap_pmm.md`](docs/commits/feature-pmm/1_bitmap_pmm.md), [`docs/commits/feature-paging/1_dynamic_vmm.md`](docs/commits/feature-paging/1_dynamic_vmm.md), [`docs/commits/feature-heap/1_kernel_heap.md`](docs/commits/feature-heap/1_kernel_heap.md) + [`2_heap_freelist.md`](docs/commits/feature-heap/2_heap_freelist.md) + [`3_heap_trim.md`](docs/commits/feature-heap/3_heap_trim.md) | `src/mem/pmm.c`, `src/mem/paging.c`, `src/mem/heap.c` |
| Interrupts & scheduler | Shared ISR stubs, `interrupt_frame_t` contract, timer-driven preemptive round robin, zombie reaping + task APIs | [`docs/architecture.md`](docs/architecture.md#1-cpu-bring-up), [`docs/commits/feature-scheduler/1_preemptive_rr.md`](docs/commits/feature-scheduler/1_preemptive_rr.md), [`docs/commits/feature-scheduler/2_task_lifecycle.md`](docs/commits/feature-scheduler/2_task_lifecycle.md) | `src/arch/x86/isr_stubs.S`, `src/arch/x86/interrupts.c`, `src/sched/sched.c` |
| Device drivers | PS/2 keyboard buffering for shell input (Enter translates to newline and console backspace erases characters so prompts behave like a normal TTY), PS/2 mouse streaming with position logs for future GUI work | [`docs/architecture.md`](docs/architecture.md#3-devices-and-drivers), [`docs/commits/feature-mouse/1_ps2_mouse.md`](docs/commits/feature-mouse/1_ps2_mouse.md), [`docs/versions/v0.7.0.md`](docs/versions/v0.7.0.md) | `src/drivers/keyboard.c`, `src/drivers/mouse.c`, `src/arch/x86/pic.c` |
| Kernel services | `int 0x80` syscall dispatcher (`sys_write`, `sys_ticks`), system info demo app, scheduler/shell integration | [`docs/architecture.md`](docs/architecture.md#4-kernel-services), [`docs/commits/feature-syscall/1_int80.md`](docs/commits/feature-syscall/1_int80.md) | `src/sys/syscall.c`, `src/apps/sysinfo.c` |
| UI + shell | VGA text console, one-shot PenOS splash, quiet prompt, blocking shell with task management + opt-in demo threads | [`docs/architecture.md`](docs/architecture.md#5-ui-and-shell) | `src/ui/console.c`, `src/shell/shell.c` |
| Diagnostics & releases | Versioned change logs, docs index, branding assets | [`docs/README.md`](docs/README.md), [`docs/versions/v0.7.0.md`](docs/versions/v0.7.0.md) ... [`v0.1.0.md`](docs/versions/v0.1.0.md) | `docs/versions/*.md`, `penos-boot-splash.svg`, `penos-favicon.svg` |

Use this table as a connected map: each feature row links directly to the design notes explaining the subsystem plus the primary source files to inspect.

## Documentation Guide

- [`docs/README.md`](docs/README.md) - mini index for the documentation tree.
- [`docs/architecture.md`](docs/architecture.md) - subsystem overview from boot to shell (best starting point).
- Commit deep dives:
  - [`docs/commits/feature-core/1_bootstrap.md`](docs/commits/feature-core/1_bootstrap.md) - initial bring-up.
  - [`docs/commits/feature-pmm/1_bitmap_pmm.md`](docs/commits/feature-pmm/1_bitmap_pmm.md) - bitmap physical allocator.
  - [`docs/commits/feature-paging/1_dynamic_vmm.md`](docs/commits/feature-paging/1_dynamic_vmm.md) - higher-half paging.
  - [`docs/commits/feature-heap/1_kernel_heap.md`](docs/commits/feature-heap/1_kernel_heap.md) + [`2_heap_freelist.md`](docs/commits/feature-heap/2_heap_freelist.md) + [`3_heap_trim.md`](docs/commits/feature-heap/3_heap_trim.md) - heap evolution.
  - [`docs/commits/feature-scheduler/1_preemptive_rr.md`](docs/commits/feature-scheduler/1_preemptive_rr.md) & [`2_task_lifecycle.md`](docs/commits/feature-scheduler/2_task_lifecycle.md) - preemption + lifecycle.
  - [`docs/commits/feature-syscall/1_int80.md`](docs/commits/feature-syscall/1_int80.md) - syscall dispatcher.
  - [`docs/commits/feature-mouse/1_ps2_mouse.md`](docs/commits/feature-mouse/1_ps2_mouse.md) - PS/2 mouse support.
  - `docs/commits/fix-wsl-multiboot-scheduler-stability/1_fix-wsl-multiboot-scheduler-stability.md` - WSL bring-up + scheduler stability notes.
- Release history: [`docs/versions/v0.7.0.md`](docs/versions/v0.7.0.md) down through [`docs/versions/v0.1.0.md`](docs/versions/v0.1.0.md).

Future work (filesystem, networking, GUI, PenScript, etc.) can grow alongside these documented modules.
