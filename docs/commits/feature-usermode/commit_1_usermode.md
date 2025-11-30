# Feature: User Mode (Ring 3) Support

## Commit Details
**Title:** arch/x86: Implement User Mode (Ring 3) execution and syscalls
**Date:** 2025-11-30
**Author:** Antigravity (Assistant)

## Description
This commit introduces the infrastructure required to execute code in User Mode (Ring 3). It implements the Task State Segment (TSS), updates the Global Descriptor Table (GDT) with user segments, and enhances the scheduler to support spawning user tasks.

## Key Changes

### 1. Architecture Support (`src/arch/x86/`)
- **TSS (`tss.c`, `tss.h`)**: Implemented TSS structure and initialization. Added `tss_set_stack` to update ESP0 on context switches.
- **GDT (`gdt.c`)**: Added User Code (0x18) and User Data (0x20) segments. Added TSS descriptor (0x28).
- **Interrupts (`interrupts.h`)**: Updated `interrupt_frame_t` to include `user_esp` and `ss` fields pushed by the CPU during privilege level changes.

### 2. Scheduler (`src/sched/sched.c`)
- **Task Structure**: Added `kernel_stack` to `task_entry_t` to track the kernel stack pointer for TSS.
- **Context Switch**: Updated `sched_tick` to call `tss_set_stack` when switching tasks.
- **`sched_spawn_user`**: New function to create a task with Ring 3 privileges:
  - Allocates a separate kernel stack for syscalls/interrupts.
  - Allocates a user stack and maps it as User-accessible (`PAGE_USER`).
  - Sets up an interrupt frame with CS=0x1B (User Code) and DS/SS=0x23 (User Data).

### 3. Memory Management (`src/mem/paging.c`)
- **Page Directory**: Updated `get_page_table` to propagate the `PAGE_USER` flag to the Page Directory Entry (PDE). This ensures that if a page is mapped as User-accessible, the containing 4MB region also allows User access, preventing Page Faults (0x7) when accessing User pages in Kernel regions (like the heap).

### 4. Shell Integration (`src/shell/shell.c`)
- **`usermode` command**: Spawns a test task that executes in Ring 3.
- **Test Task**: Performs a system call (`int 0x80`) to print a message, verifying that:
  - Code is running in Ring 3.
  - Syscalls are correctly handled and return to User Mode.

## Testing
- **Verification**: Run `usermode` in the shell.
- **Expected Output**:
  ```
  Spawning User Mode task...
  Spawned user task PID: <id>
  [User] Hello from Ring 3!
  ```

## Limitations
- **Memory Protection**: User stack is allocated from kernel heap and remapped. Proper process isolation and virtual memory management are future work.
- **Binary Loading**: Currently only supports spawning internal functions as user tasks. ELF loading is not yet implemented.
