# <img src="penos-favicon.svg" alt="PenOS" style="height: 1em; vertical-align: middle;"> PenOS

> **PenOS is not just a kernel demo – it is a small, real operating system you can boot, inspect, and extend.**

PenOS is our own 32‑bit x86 operating system, built from scratch and booted via GRUB. A custom assembly stub in `src/boot.s` brings the CPU into protected mode and hands off to `kernel_main`, where each subsystem is staged in a predictable order:

- **CPU & Interrupt Stack** – flat GDT/IDT tables, PIC remap, PIT timer, and shared ISR stubs keep exceptions and timer IRQs under control.
- **Memory Core** – bitmap physical allocator, higher‑half paging with a recursive mapping slot, and a freelist‑based kernel heap carve out RAM safely.
- **Scheduler & Tasks** – a preemptive round‑robin scheduler plus lifecycle tracking powers demo workloads launched from the shell.
- **Drivers & Syscalls** – PS/2 keyboard and mouse drivers plus an `int 0x80` syscall gate expose essential I/O and diagnostics.
- **UI Path** – VGA console and a Multiboot framebuffer renderer give us a PenOS splash/logo, console overlay, and modern‑feeling shell even on bare‑metal QEMU/VirtualBox.

From the outside, PenOS looks like a compact, branded OS that **boots, shows a graphical splash, and drops you into an interactive shell**. Under the hood it’s a carefully split tree—`arch`, `mem`, `drivers`, `sched`, `shell`, `ui`—with a simple `make` pipeline that emits a themed ISO and a documentation set that explains each subsystem.

---

## 1. What PenOS Gives You

### For reviewers and recruiters

- A **bootable ISO** (`PenOS.iso`) that runs under QEMU/VirtualBox/VMware.
- A **graphical boot experience**: GRUB theme + kernel framebuffer splash derived from `penos-boot-splash.svg`.
- A **quiet, interactive shell** (`PenOS>`) with real commands and preemptive demo tasks you can start/stop on demand.
- Clear **architecture docs** and per‑feature commit notes that show how the system grew over time.

### For you (the developer)

At the kernel level, PenOS is your playground for:

- Bringing a CPU from GRUB handoff into protected mode with your own GDT/IDT.
- Enabling paging and a higher‑half kernel, with a bitmap PMM and higher‑half heap.
- Wiring interrupts, timer, driver callbacks, and a preemptive scheduler.
- Building a text UI path that can evolve into a full framebuffer GUI.

Future work (filesystem, networking, GUI desktop, PenScript, etc.) is intentionally scoped as **follow‑up features**, so the repo stays teachable and extensible.

---

## 2. Boot & Splash Experience

PenOS aims to feel like a “real OS” from the first frame on screen.

1. **GRUB splash (bootloader)**
   GRUB renders a themed 1024x768 splash using a rasterized export of `penos-boot-splash.svg`:

   - Theme assets live under `grub/themes/penos/`.
   - The main image is `penos-boot-splash.png`.

2. **Kernel splash (framebuffer)**
   After GRUB hands control to `kernel.bin`, the kernel:

   - Keeps the 1024x768x32 framebuffer alive.
   - Renders a gradient PenOS logo/splash derived from the same palette as the GRUB theme.
   - Fades into or overlays the text console, so logs and the shell appear on top of the splash.

3. **Shell prompt**
   Once initialization completes, PenOS prints a short banner and leaves you at a quiet:

   ```
   PenOS>
   ```

   No demo tasks run by default; the console stays calm until **you** opt in.

> **Keeping assets in sync**
> Update the GRUB PNG whenever you tweak the SVG:
>
> ```bash
> python -m pip install --user cairosvg
> cairosvg penos-boot-splash.svg -o grub/themes/penos/penos-boot-splash.png -W 1024 -H 768
> ```
>
> Then rerun `make iso` so the new splash is baked into `PenOS.iso`.

![Boot splash](penos-boot-splash.svg)

---

## 3. Quick Start

### Prerequisites

You’ll need a basic 32‑bit toolchain and GRUB tooling:

- `gcc` with 32‑bit support
- `ld`
- `grub-mkrescue`
- `xorriso`
- `qemu-system-i386` (or VirtualBox/VMware/Bochs)

### Build & Run

```bash
make            # build build/kernel.bin
make iso        # produce PenOS.iso (GRUB BIOS image)
make run        # boot the ISO in QEMU
```

`make iso` copies the PenOS kernel and GRUB theme assets (`grub/themes/penos/`), so the bootloader always shows the graphical splash.

Boot `PenOS.iso` in QEMU/VirtualBox/VMware or dd it to a BIOS‑capable USB stick. The expected flow is:

1. **GRUB**: themed PenOS splash.
2. **Kernel**: framebuffer splash + overlay console.
3. **Init log**: short initialization sequence.
4. **Shell**: a quiet `PenOS>` prompt.

### Shell commands

From the shell you can:

- `help` – list available commands.
- `clear` – clear the console.
- `echo <text>` – print text back.
- `ticks` – show the PIT tick counter.
- `sysinfo` – dump basic system information.
- `ps` – list tasks.
- `spawn <counter|spinner>` – start demo kernel threads.
- `kill <pid>` – stop a specific task.
- `halt` – halt the CPU (HLT loop).
- `shutdown` – request a clean power‑off in QEMU/VirtualBox/Bochs.

Use `spawn ...` when you want the preemptive demo threads to emit `[counter] tick N` or spinner glyphs; by default there is **no background chatter**. In addition, `Ctrl+C` or `ESC` can be wired (depending on version) to interrupt noisy demos quickly.

---

## 4. Architecture at a Glance

PenOS remains compact but is decomposed like a real OS:

- **CPU & Interrupt Stack**
  Flat GDT/IDT, exception stubs, PIC remap, PIT @100 Hz, and an `interrupt_frame_t` contract shared between assembly and C.

- **Memory Core**
  A bitmap PMM, higher‑half paging with recursive mapping, and a freelist‑based kernel heap with trimming.

- **Scheduler & Tasks**
  Preemptive round‑robin scheduler, per‑task stacks, task lifecycle (READY/RUNNING/SLEEP/ZOMBIE), and shell‑driven spawn/kill.

- **Drivers**
  PS/2 keyboard (line‑buffered shell input, backspace, control codes) and PS/2 mouse (streaming packets, position logs) for future GUI work.

- **Kernel Services**
  A tiny `int 0x80` syscall dispatcher and a demo app (`sysinfo`) that composes timer, PMM, and scheduler state.

- **UI & Shell**
  VGA text console; a framebuffer splash/logo path that overlays the console (with VGA fallback if no framebuffer); and a blocking shell that owns the input loop.

---

## 5. Feature Map

Use this table as a **map** from high‑level feature → design docs → actual code.

| Area                   | What you get today                                                                                                                                                                                            | Deep‑dive docs                                                                                                                                                                                                                                                                                                                                                                                                                                                                               | Key sources                                                                                           |
| ---------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| Boot & CPU bring‑up    | GRUB theme + raster PenOS splash, `boot.s` + `kernel_main`, flat GDT/IDT, PIC remap, PIT @100 Hz, ASCII/console splash before logs                                                                            | [`docs/architecture.md`](docs/architecture.md#1-cpu-bring-up), [`docs/commits/feature-core/1_bootstrap.md`](docs/commits/feature-core/1_bootstrap.md)                                                                                                                                                                                                                                                                                                                                        | `grub/themes/penos`, `src/boot.s`, `src/arch/x86/gdt.c`, `src/arch/x86/idt.c`, `src/arch/x86/timer.c` |
| Memory management      | Bitmap PMM, dynamic paging with higher‑half mirror + recursive slot, higher‑half heap with freelist + trimming                                                                                                | [`docs/architecture.md`](docs/architecture.md#2-memory-management), [`docs/commits/feature-pmm/1_bitmap_pmm.md`](docs/commits/feature-pmm/1_bitmap_pmm.md), [`docs/commits/feature-paging/1_dynamic_vmm.md`](docs/commits/feature-paging/1_dynamic_vmm.md), [`docs/commits/feature-heap/1_kernel_heap.md`](docs/commits/feature-heap/1_kernel_heap.md) + [`2_heap_freelist.md`](docs/commits/feature-heap/2_heap_freelist.md) + [`3_heap_trim.md`](docs/commits/feature-heap/3_heap_trim.md) | `src/mem/pmm.c`, `src/mem/paging.c`, `src/mem/heap.c`                                                 |
| Interrupts & scheduler | Shared ISR stubs, `interrupt_frame_t` contract, timer‑driven preemptive round robin, zombie reaping + task APIs                                                                                               | [`docs/architecture.md`](docs/architecture.md#1-cpu-bring-up), [`docs/commits/feature-scheduler/1_preemptive_rr.md`](docs/commits/feature-scheduler/1_preemptive_rr.md), [`docs/commits/feature-scheduler/2_task_lifecycle.md`](docs/commits/feature-scheduler/2_task_lifecycle.md)                                                                                                                                                                                                          | `src/arch/x86/isr_stubs.S`, `src/arch/x86/interrupts.c`, `src/sched/sched.c`                          |
| Device drivers         | PS/2 keyboard buffering for shell input (Enter → newline, Backspace erases, Ctrl combos emit control codes so `Ctrl+C` can interrupt demo tasks), PS/2 mouse streaming with position logs for future GUI work | [`docs/architecture.md`](docs/architecture.md#3-devices-and-drivers), [`docs/commits/feature-mouse/1_ps2_mouse.md`](docs/commits/feature-mouse/1_ps2_mouse.md), [`docs/versions/v0.7.0.md`](docs/versions/v0.7.0.md)                                                                                                                                                                                                                                                                         | `src/drivers/keyboard.c`, `src/drivers/mouse.c`, `src/arch/x86/pic.c`                                 |
| Kernel services        | `int 0x80` syscall dispatcher (`sys_write`, `sys_ticks`), system info demo app, scheduler/shell integration                                                                                                   | [`docs/architecture.md`](docs/architecture.md#4-kernel-services), [`docs/commits/feature-syscall/1_int80.md`](docs/commits/feature-syscall/1_int80.md)                                                                                                                                                                                                                                                                                                                                       | `src/sys/syscall.c`, `src/apps/sysinfo.c`                                                             |
| UI + shell             | Framebuffer splash/logo + console overlay (with VGA fallback), quiet prompt, blocking shell with task management, opt‑in demos, and a `shutdown` command                                                      | [`docs/architecture.md`](docs/architecture.md#5-ui-and-shell)                                                                                                                                                                                                                                                                                                                                                                                                                                | `src/ui/framebuffer.c`, `src/ui/console.c`, `src/shell/shell.c`                                       |
| Diagnostics & releases | Versioned change logs, docs index, branding assets                                                                                                                                                            | [`docs/README.md`](docs/README.md), [`docs/versions/v0.8.0.md`](docs/versions/v0.8.0.md) ... [`v0.1.0.md`](docs/versions/v0.1.0.md)                                                                                                                                                                                                                                                                                                                                                          | `docs/versions/*.md`, `penos-boot-splash.svg`, `penos-favicon.svg`                                    |

---

## 6. Documentation Guide

If you want to **understand** PenOS rather than just boot it, start here:

- [`docs/README.md`](docs/README.md) – mini index for the documentation tree.
- [`docs/architecture.md`](docs/architecture.md) – subsystem overview from boot to shell.

Then dive into **commit‑style deep‑dives**, each documenting one big step in the OS:

- [`docs/commits/feature-core/1_bootstrap.md`](docs/commits/feature-core/1_bootstrap.md) – initial bring‑up.
- [`docs/commits/feature-pmm/1_bitmap_pmm.md`](docs/commits/feature-pmm/1_bitmap_pmm.md) – bitmap physical allocator.
- [`docs/commits/feature-paging/1_dynamic_vmm.md`](docs/commits/feature-paging/1_dynamic_vmm.md) – higher‑half paging.
- [`docs/commits/feature-heap/1_kernel_heap.md`](docs/commits/feature-heap/1_kernel_heap.md) + [`2_heap_freelist.md`](docs/commits/feature-heap/2_heap_freelist.md) + [`3_heap_trim.md`](docs/commits/feature-heap/3_heap_trim.md) – heap evolution.
- [`docs/commits/feature-scheduler/1_preemptive_rr.md`](docs/commits/feature-scheduler/1_preemptive_rr.md) & [`2_task_lifecycle.md`](docs/commits/feature-scheduler/2_task_lifecycle.md) – preemption + lifecycle.
- [`docs/commits/feature-syscall/1_int80.md`](docs/commits/feature-syscall/1_int80.md) – syscall dispatcher.
- [`docs/commits/feature-mouse/1_ps2_mouse.md`](docs/commits/feature-mouse/1_ps2_mouse.md) – PS/2 mouse support.
- `docs/commits/fix-wsl-multiboot-scheduler-stability/1_fix-wsl-multiboot-scheduler-stability.md` – WSL bring‑up + scheduler stability notes.

Release history is tracked in:

- [`docs/versions/v0.8.0.md`](docs/versions/v0.8.0.md) down through [`docs/versions/v0.1.0.md`](docs/versions/v0.1.0.md).

Each release doc explains what changed at a “product” level, while the commit docs explain **how**.

---

## 7. Roadmap & Future Work

PenOS is intentionally structured so new features fit into clear folders and docs:

- **Filesystem** – block device abstraction, a simple on‑disk format, and VFS layer.
- **Networking** – basic drivers and a small TCP/IP stack.
- **GUI desktop** – full framebuffer windowing, widgets, and integration with the existing shell.
- **PenScript** – a tiny scripting language for automation and demos.

These larger features will land behind `feature/*` branches, each accompanied by:

- A commit doc (branch name, commit message, summary, architecture, code tour).
- A coverage report (implemented vs partial vs TODO).
- A small follow‑up prompt describing how to keep extending the system.

PenOS is already more than “print("Hello") on bare metal” – but it’s still small enough that you can read it in a weekend and grow it in public.
