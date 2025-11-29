# <img src="penos-favicon.svg" alt="PenOS" style="height: 1em; vertical-align: middle;"> PenOS

PenOS is our own 32-bit x86 kernel, built from scratch and booted via GRUB. A custom assembly stub in `src/boot.s` brings the CPU into protected mode and hands off to `kernel_main`, where each subsystem is staged predictably:

- **CPU & Interrupt Stack:** flat GDT/IDT tables, PIC remap, PIT timer, and shared ISR stubs keep exceptions and timer IRQs under control.
- **Memory Core:** bitmap physical allocator, higher-half paging with a recursive mapping slot, and a freelist-based kernel heap carve out RAM safely.
- **Scheduler & Tasks:** a preemptive round-robin scheduler plus lifecycle tracking powers demo workloads launched from the shell.
- **Drivers & Syscalls:** PS/2 keyboard and mouse drivers plus an `int 0x80` syscall gate expose essential I/O and diagnostics.
- **UI Path:** VGA console with a Multiboot framebuffer renderer gives us a PenOS splash/logo, console overlay, and modern-feeling shell even on bare metal QEMU/VirtualBox.

For recruiters or non-OS folks, PenOS is a portfolio-ready OS that boots, shows a graphical splash, and provides an interactive shell while demonstrating low-level mastery. Technical reviewers get a clean modular tree (`arch`, `mem`, `drivers`, `sched`, `shell`, `ui`), a `make` pipeline that emits a themed ISO, and extensive documentation (`docs/architecture.md`, per-feature commit write-ups). Marketers can pitch it as “branded polish uncommon in hobby kernels,” “full-stack mastery from bootloader to GUI-ready framebuffer,” and “a launchpad for upcoming filesystem, networking, and PenScript work” that’s already mapped out in the roadmap.

PenOS remains a compact yet real operating system intended for learning and portfolio demonstrations. GRUB loads a Multiboot-compliant kernel that brings up protected-mode infrastructure, configures paging and heaps in the higher half, enables interrupts, keeps the GRUB-provided 1024x768x32 framebuffer alive, renders a gradient PenOS splash/logo, and still drops you at a quiet text-mode shell that now sits inside a tinted overlay. Demo tasks (counter/spinner) are opt-in via shell commands so the default console stays calm in both the framebuffer and VGA fallbacks.

Before the kernel runs, GRUB renders a themed 1024x768 splash that uses a rasterized export of `penos-boot-splash.svg` (`grub/themes/penos/penos-boot-splash.png`). The kernel's framebuffer splash is derived from the same palette and appears immediately after GRUB hands off control, creating a seamless boot experience. Update that PNG with `python -m pip install --user cairosvg` followed by `cairosvg penos-boot-splash.svg -o grub/themes/penos/penos-boot-splash.png -W 1024 -H 768` (or any vector tool) whenever you tweak the SVG so the bootloader, kernel splash, and docs stay in sync.

![Boot splash](penos-boot-splash.svg)

## Quick Start

Requirements: `gcc` with 32-bit support, `ld`, `grub-mkrescue`, `xorriso`, and `qemu-system-i386`.

```
make            # build build/kernel.bin
make iso        # produce PenOS.iso (GRUB BIOS image)
make run        # boot the ISO in QEMU
```

`make iso` copies the PenOS kernel plus the GRUB theme assets (`grub/themes/penos/`) so the bootloader shows the graphical splash automatically. If you update the PNG, rerun `make iso` to bake it into `PenOS.iso`.

Boot `PenOS.iso` in QEMU/VirtualBox/VMware or dd it to a BIOS-capable USB stick. The flow is bootloader splash -> framebuffer splash/overlay -> init log -> `PenOS>` prompt. Shell commands: `help`, `clear`, `echo <text>`, `ticks`, `sysinfo`, `ps`, `spawn <counter|spinner>`, `kill <pid>`, `halt`, `shutdown`, `pwd`, `cd <dir>`, `ls`. Use `spawn ...` when you want the preemptive demo threads to emit `[counter] tick N` or spinner glyphs; by default there is no background chatter. Tap `ESC` or `Ctrl+C` at any prompt to instantly stop the demo tasks without typing `kill`, and run `shutdown` to power off QEMU/VirtualBox/Bochs cleanly.

The shell now supports **tab completion** for both commands and file paths. Try `cd /mnt<TAB>` to explore the host filesystem shared via VirtIO-9P.

## Feature Map

| Area | What you get today | Deep-dive docs | Key sources |
| --- | --- | --- | --- |
| Boot & CPU bring-up | GRUB theme + raster PenOS splash, `boot.s` + `kernel_main`, flat GDT/IDT, PIC remap, PIT @100 Hz, ASCII splash before logs | [`docs/architecture.md`](docs/architecture.md#1-cpu-bring-up), [`docs/commits/feature-core/1_bootstrap.md`](docs/commits/feature-core/1_bootstrap.md) | `grub/themes/penos`, `src/boot.s`, `src/arch/x86/gdt.c`, `src/arch/x86/idt.c`, `src/arch/x86/timer.c` |
| Memory management | Bitmap PMM, dynamic paging with higher-half mirror + recursive slot, higher-half heap with freelist + trimming | [`docs/architecture.md`](docs/architecture.md#2-memory-management), [`docs/commits/feature-pmm/1_bitmap_pmm.md`](docs/commits/feature-pmm/1_bitmap_pmm.md), [`docs/commits/feature-paging/1_dynamic_vmm.md`](docs/commits/feature-paging/1_dynamic_vmm.md), [`docs/commits/feature-heap/1_kernel_heap.md`](docs/commits/feature-heap/1_kernel_heap.md) + [`2_heap_freelist.md`](docs/commits/feature-heap/2_heap_freelist.md) + [`3_heap_trim.md`](docs/commits/feature-heap/3_heap_trim.md) | `src/mem/pmm.c`, `src/mem/paging.c`, `src/mem/heap.c` |
| Interrupts & scheduler | Shared ISR stubs, `interrupt_frame_t` contract, timer-driven preemptive round robin, zombie reaping + task APIs | [`docs/architecture.md`](docs/architecture.md#1-cpu-bring-up), [`docs/commits/feature-scheduler/1_preemptive_rr.md`](docs/commits/feature-scheduler/1_preemptive_rr.md), [`docs/commits/feature-scheduler/2_task_lifecycle.md`](docs/commits/feature-scheduler/2_task_lifecycle.md) | `src/arch/x86/isr_stubs.S`, `src/arch/x86/interrupts.c`, `src/sched/sched.c` |
| Filesystem | VirtIO-9P client (9P2000.L), directory navigation (`cd`, `ls`, `pwd`), host filesystem access (WSL/Linux), descriptor chaining | [`docs/commits/feature-virtio-9p-filesystem/feature-virtio-9p-filesystem.md`](docs/commits/feature-virtio-9p-filesystem/feature-virtio-9p-filesystem.md) | `src/drivers/virtio.c`, `src/fs/9p.c`, `include/fs/9p.h` |
| Device drivers | PS/2 keyboard buffering for shell input (Enter -> newline, Backspace erases, and Ctrl combos emit control codes so `Ctrl+C` interrupts demo tasks), PS/2 mouse streaming with position logs for future GUI work | [`docs/architecture.md`](docs/architecture.md#3-devices-and-drivers), [`docs/commits/feature-mouse/1_ps2_mouse.md`](docs/commits/feature-mouse/1_ps2_mouse.md), [`docs/versions/v0.7.0.md`](docs/versions/v0.7.0.md) | `src/drivers/keyboard.c`, `src/drivers/mouse.c`, `src/arch/x86/pic.c` |
| Kernel services | `int 0x80` syscall dispatcher (`sys_write`, `sys_ticks`), system info demo app, scheduler/shell integration | [`docs/architecture.md`](docs/architecture.md#4-kernel-services), [`docs/commits/feature-syscall/1_int80.md`](docs/commits/feature-syscall/1_int80.md) | `src/sys/syscall.c`, `src/apps/sysinfo.c` |
| UI + shell | Framebuffer splash/logo + console overlay (with VGA fallback), quiet prompt with current path, tab completion for commands and paths, blocking shell with task management | [`docs/architecture.md`](docs/architecture.md#5-ui-and-shell), [`docs/commits/feature-virtio-9p-filesystem/commit_2_tab-completion.md`](docs/commits/feature-virtio-9p-filesystem/commit_2_tab-completion.md) | `src/ui/framebuffer.c`, `src/ui/console.c`, `src/shell/shell.c` |
| Diagnostics & releases | Versioned change logs, docs index, branding assets | [`docs/README.md`](docs/README.md), [`docs/versions/v0.8.0.md`](docs/versions/v0.8.0.md) ... [`v0.1.0.md`](docs/versions/v0.1.0.md) | `docs/versions/*.md`, `penos-boot-splash.svg`, `penos-favicon.svg` |

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
  - [`docs/commits/feature-virtio-9p-filesystem/feature-virtio-9p-filesystem.md`](docs/commits/feature-virtio-9p-filesystem/feature-virtio-9p-filesystem.md) - VirtIO-9P filesystem implementation.
  - [`docs/commits/feature-virtio-9p-filesystem/commit_2_tab-completion.md`](docs/commits/feature-virtio-9p-filesystem/commit_2_tab-completion.md) - Shell tab completion.
  - `docs/commits/fix-wsl-multiboot-scheduler-stability/1_fix-wsl-multiboot-scheduler-stability.md` - WSL bring-up + scheduler stability notes.
- Release history: [`docs/versions/v0.7.0.md`](docs/versions/v0.7.0.md) down through [`docs/versions/v0.1.0.md`](docs/versions/v0.1.0.md).

Future work (networking, GUI, PenScript, etc.) can grow alongside these documented modules.
