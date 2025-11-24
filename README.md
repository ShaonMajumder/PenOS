# <img src="penos-favicon.svg" alt="PenOS" style="height: 1em; vertical-align: middle;"> PenOS

PenOS is a compact yet real 32-bit x86 operating system intended for learning and portfolio demonstration. It boots through GRUB, sets up protected mode infrastructure (GDT, IDT, PIC), installs timer + keyboard interrupts, initializes a trivial physical memory manager, identity-maps the first 4 MiB, and exposes a text console plus a blocking shell with a few diagnostic commands.

![JobGenie.ai Demo](penos-boot-splash.svg)

## Key Features

- Multiboot-compliant loader, custom linker script, and freestanding build with `gcc -m32`.
- Early CPU init: GDT, IDT, PIC remapping, and interrupt dispatch layer.
- PIT-driven tick counter driving a preemptive round-robin scheduler with demo counter/spinner threads.
- Bitmap-backed physical memory allocator, dynamic paging, and higher-half kernel heap with freelist `kmalloc`/`kfree`.
- PS/2 keyboard driver feeding a command shell.
- `int 0x80` syscall dispatcher with `sys_write`/`sys_ticks` entry points for future user tasks.
- Heap exposes `heap_bytes_in_use/free` stats and trims idle pages back to the PMM.
- Text-mode console, system info demo app, and branding assets (`penos-boot-splash.svg`, `penos-favicon.svg`).

## Building

Requirements: `gcc` with 32-bit support, `ld`, `grub-mkrescue`, `xorriso`, and `qemu-system-i386`.

```
make            # builds build/kernel.bin
make iso        # builds PenOS.iso via grub-mkrescue
make run        # boots ISO in QEMU
```

## Running

Boot `PenOS.iso` in QEMU/VirtualBox/VMware or dd it to a USB stick (BIOS mode). The shell supports `help`, `clear`, `echo <text>`, `ticks`, `sysinfo`, and `halt`.

## Documentation

- `docs/architecture.md` - overview of boot -> kernel -> interrupts -> memory -> scheduler -> UI.
- `docs/commits/feature-scheduler/1_preemptive_rr.md` - scheduler design notes.
- `docs/commits/feature-syscall/1_int80.md` - syscall dispatcher details.
- `docs/versions/v0.5.0.md` - latest release summary (older versions also retained).

Future work (filesystem, networking, GUI, PenScript, etc.) can extend the structured directories already in place.
