## Combined Git Commit

### Commit message

```text
core: stabilize build, multiboot, and interrupt-driven scheduler on WSL toolchain
```

### Commit description

This change set brings PenOS from a non-booting prototype to a stable, preemptively scheduled kernel on the WSL GCC/binutils toolchain. On the build side it fixes the C/ASM interface for `isr_stub_table`, adds proper rules for all assembly sources (including `gdt_flush.s`), introduces a tiny freestanding string library, and exposes the `end` symbol from the linker script so PMM and paging can size memory. The Multiboot header is now emitted in an allocatable `.multiboot` section placed first in the image and marked `KEEP`, so GRUB reliably finds the magic within the first 8 KB when loading `/boot/kernel.bin`. At runtime the interrupt frame definition was corrected to match what `isr_common_stub` actually pushes, invalid `iretd` was replaced with `iret`, and the panic path was extended to print the vector number, error code, EIP/CS/EFLAGS, ESP, and CR2 for page faults. Most importantly, the interrupt/dispatcher contract was redesigned so `isr_dispatch` returns the frame to resume and the scheduler maintains a separate `kmalloc`’d stack and frame for each task, with the timer callback requesting stack switches explicitly. With these pieces in place, enabling interrupts no longer causes #GP or random #PF/#DE faults, and the counter/spinner demo tasks now run indefinitely on their own stacks while the rest of the kernel remains stable.

---

# PenOS WSL Bring-Up & Interrupt/Scheduler Troubleshooting

This document captures the end-to-end debugging path for getting PenOS to build and run reliably under a GCC/binutils toolchain on WSL, from the first compile error through to a stable preemptive scheduler.

---

## 1. Initial build failure: “array of voids” in IDT

### Symptom

```text
src/arch/x86/idt.c:9:13: error: declaration of ‘isr_stub_table’ as array of voids
    extern void isr_stub_table[];
```

### Root cause

The assembly ISR stub table (`src/arch/x86/isr_stubs.S`) emits a list of 32-bit handler addresses using `.long handler`. Declaring it in C as `extern void isr_stub_table[];` asks the compiler for an “array of voids,” which is illegal.

### Fix

Changed the declaration to match the actual data:

```c
extern uint32_t isr_stub_table[];
```

Adjusted IDT setup code to treat table entries as `uint32_t` addresses.

### Outcome

The IDT module compiled cleanly and the C/ASM interface for ISR entrypoints became type-correct.

---

## 2. Makefile gaps: missing rules for gdt_flush & assembly sources

### Symptoms

Missing source target:

```text
No rule to make target 'build/arch/x86/gdt_flush.s'
```

Then, missing object target:

```text
No rule to make target 'build/arch/x86/gdt_flush.o'
```

### Root cause

The build system only handled some assembly patterns and ignored lowercase `.s` files. `gdt_flush.s` was listed as a dependency of the kernel but never actually compiled.

### Fixes

Updated the `Makefile` to:

- Discover both `.S` and `.s` sources under `src/arch/x86/`.
- Add explicit pattern rules for compiling `.s → .o` into `build/arch/x86/`.
- Ensure `gdt_flush.s` participates in that pipeline.

### Outcome

All required assembly sources (`gdt_flush.s`, `isr_stubs.S`, `boot.s`) were compiled to objects and the kernel could link, moving the failure from the build phase to the link/runtime phase.

---

## 3. Assembler incompatibility: `iretd` vs `iret`

### Symptom

```text
src/arch/x86/isr_stubs.S:120: Error: no such instruction: `iretd`
```

### Root cause

Some assemblers accept `iretd` as a 32-bit “interrupt return” mnemonic. The 32-bit GNU assembler under WSL only accepts `iret`. Using `iretd` made the ISR common stub impossible to assemble.

### Fix

Replaced `iretd` with standard 32-bit `iret` in `isr_common_stub`.

### Outcome

ISR stubs assembled successfully across toolchains.

---

## 4. Linker failures: missing `end` symbol & libc dependencies

### Symptom

Linker errors while building `kernel.bin`:

**Undefined reference to `end` from paging and PMM:**

```text
paging.c:(.text+0x...): undefined reference to `end`
pmm.c:(.text+0x...): undefined reference to `end`
```

**Undefined references to `strlen`, `strcmp`, `strncmp`, `strncpy` from scheduler/shell.**

### Root causes

- The linker script used `end` conceptually as the symbol after `.bss`, but never actually defined it.
- The kernel is freestanding and does not link libc, but higher-level code used standard string functions without freestanding replacements.

### Fixes

#### Linker script (`linker.ld`)

Defined `end = .;` at the end of the `.bss` section to expose the symbol used by PMM and paging initialisation.

#### Freestanding string library

Added `src/lib/string.c` with minimal, freestanding implementations of:

- `strlen`
- `strcmp`
- `strncmp`
- `strncpy`

Ensured this object is linked into the kernel.

### Outcome

The kernel linked without libc and both paging/PMM could size memory using the `end` marker.

---

## 5. GRUB: “no multiboot header found”

### Symptom

When booting `PenOS.iso` in QEMU:

```text
Booting from DVD/CD...
error: no multiboot header found.
error: you need to load the kernel first.
```

### Root cause

The Multiboot header was emitted, but it lived in a non-allocatable/incorrectly placed section. As a result, the first 8 KB of `kernel.bin` did not contain the Multiboot magic that GRUB searches for. `grub.cfg` itself was correct; the problem was the image layout.

### Fixes

#### `src/boot.s`

Declared the Multiboot header in an allocatable section:

```asm
.set ALIGN,  1<<0
.set MEMINFO,1<<1
.set FLAGS,  ALIGN | MEMINFO
.set MAGIC,  0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot, "a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
```

Ensured this block appears before `.text` and the start symbol.

#### `linker.ld`

Placed `.multiboot` as the first loadable section at 1 MiB:

```ld
ENTRY(start)
SECTIONS {
  . = 1M;
  .multiboot ALIGN(4K) : { KEEP(*(.multiboot)) }
  .text      ALIGN(4K) : { *(.text*) }
  .rodata    ALIGN(4K) : { *(.rodata*) }
  .data      ALIGN(4K) : { *(.data*) }
  .bss       ALIGN(4K) : { *(COMMON) *(.bss*) }
  end = .;
}
```

Marked `.multiboot` with `KEEP` so the header cannot be discarded.

### Outcome

The Multiboot header is now in a loadable, allocatable section within the first 8 KB of the image. GRUB’s `multiboot /boot/kernel.bin` entry successfully loads and transfers control to `start`.

---

## 6. Early runtime: General Protection fault after `sti`

### Symptom

Right after “Enabling interrupts…”, the kernel died:

```text
Initialization complete. Enabling interrupts...
Exception: General Protection
System halted.
```

### Root cause

The scheduler and interrupt code assumed a particular interrupt frame layout that did not match what the ISR stubs actually pushed.

`src/arch/x86/isr_stubs.S`:

- `pusha`
- `push ds/es/fs/gs`
- push `int_no` and `err_code`
- CPU-supplied `eip`, `cs`, `eflags` are already on the stack

`include/arch/x86/interrupts.h`:

- `interrupt_frame_t` initially had registers in a different order, and even phantom fields (`useresp`, `ss`) the stubs never push.

The scheduler (`sched_tick`) copied this mismatched struct to and from the stack, corrupting the IRET frame (especially `cs/eip`). The very first timer IRQ after `sti` tried to return via a bogus descriptor and triggered #GP.

### Fixes

#### Align the C structure with the actual stack layout

Final `interrupt_frame_t`:

```c
typedef struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
} interrupt_frame_t;
```

Removed `useresp/ss` and ensured field order matches pushes in `isr_common_stub`.

#### Improve diagnostics

Introduced `panic_with_frame()` that prints:

- `int_no`, `err_code`
- `eip`, `cs`, `eflags`
- `esp` at IRQ time and `esp` in the handler

For page faults, captured `cr2` and decoded error bits (present/write/user) for future debugging.

### Outcome

The kernel could enable interrupts without immediately taking a #GP; timer IRQs fired, and the demo scheduler tasks began to run, though deeper issues remained.

---

## 7. Invalid Opcode at tiny EIP (stack corruption from frame size mismatch)

### Symptom

After some ticks:

```text
[counter] tick 0 .. tick 9
[panic] Invalid Opcode (int=6, err=0x00000000, eip=0x00000060)
System halted.
```

### Root cause

Even after correcting the field order, the first iteration of `interrupt_frame_t` still included extra fields the assembly didn’t push. That made `sizeof(interrupt_frame_t)` larger than the real frame. When the scheduler saved/restored using this oversized struct, it overwrote 8 bytes beneath the true stack frame. Eventually `iret` returned to address `0x60`, where no valid code existed, producing `#UD`.

### Fix

- Shrunk `interrupt_frame_t` to exactly the fields the ISR stubs and CPU push, removing phantom tail fields.
- Updated scheduler code to use the new struct size, avoiding over-writes.

### Outcome

No more invalid opcodes due to struct over-size; the remaining crashes were due to how stacks were shared.

---

## 8. Random #PF / #DE during long runs: broken context switching

### Symptoms

Under load, crashes alternated between page faults and division-by-zero at random addresses:

**Page fault at low/odd EIP:**

```text
[panic] Page Fault
  int=14 err=0x00000000 eip=0x00005FF1 ...
```

**Division by zero at tiny EIP:**

```text
[panic] Division By Zero
  int=0 err=0x00000000 eip=0x0000002D ...
```

**Long-run page fault with seemingly “reasonable” kernel EIP:**

```text
[spinner] [panic] Page Fault
  int=14 err=0x00000000 eip=0x00103295 ...
```

### Root cause

The fundamental design bug: **no real stack switch on context switches.**

- `isr_common_stub` always returned using the current `%esp`.
- The scheduler stored register snapshots for each task in plain structs and then copied them into/out of the live interrupt frame instead of switching stacks.
- Multiple tasks therefore “shared” whatever stack happened to be in use when the timer fired. Their call frames overlapped and gradually corrupted each other’s return addresses and saved state.

This explains the non-deterministic nature: corruption depended on interleaving between counter, spinner, and kernel code.

### Fixes

#### Change the interrupt/dispatcher contract

- `isr_dispatch()` now returns an `interrupt_frame_t*` representing the frame to resume.
- `isr_common_stub` was updated to:

  - push registers as before
  - `push %esp; call isr_dispatch; add $4, %esp`
  - load `%esp` from the returned pointer in `%eax`
  - restore registers and `iret` from that frame

This gives the C side full control over which stack/frame will be resumed.

#### Introduce `interrupt_request_frame_switch()`

- A small helper that lets code (e.g., the timer callback) request a different frame to resume after the handler.

#### Rebuild the scheduler around per-task stacks

Each task now owns:

- a `kmalloc`’d stack
- an `interrupt_frame_t*` placed near the top of that stack

Task creation:

- Pre-builds a synthetic frame with:

  - `eip = task_trampoline`
  - code/data segments set to kernel selectors
  - `eflags` with IF set
  - `esp/ebp` set to the aligned top of the stack

`sched_tick()`:

- Saves the current task’s frame pointer.
- Picks the next READY task.
- Returns that task’s frame pointer so the interrupt machinery can switch `%esp` to that stack.

#### Timer integration

- `timer_callback()` calls `sched_tick(frame)` and, if it gets back a different frame pointer, calls `interrupt_request_frame_switch(next)`.

### Outcome

Each kernel thread (main, counter, spinner) now runs on its own stack, and the ISR stubs resume via the correct saved frame. The random `#PF/#DE` exceptions disappear; counter and spinner can run for hundreds of thousands of ticks without corrupting one another.

---

## 9. Minor tidy: `<stddef.h>` for `NULL`

### Symptom

```text
error: ‘NULL’ undeclared here (not in a function)
    static interrupt_frame_t *next_frame_override = NULL;
```

### Root cause

`interrupts.c` used `NULL` without including `<stddef.h>` in this freestanding environment.

### Fix

Added `#include <stddef.h>` to `src/arch/x86/interrupts.c`.

### Outcome

Compilation warning disappeared; no functional change, but the file is now self-contained.

---

## Final State

After all of the above:

- The kernel builds and links cleanly with a freestanding toolchain.
- `PenOS.iso` boots under GRUB; the Multiboot header is valid and placed correctly.
- GDT/IDT/ISR stubs and `interrupt_frame_t` are consistent.
- Interrupts can be enabled immediately after init without causing #GP.
- The timer, scheduler, and demo tasks (counter, spinner) run indefinitely on dedicated stacks with no random page faults, division-by-zero, or invalid opcodes.
- Panic output now includes rich context (vector, error code, EIP/CS/EFLAGS, ESP and CR2 for page faults) to support future low-level debugging.
