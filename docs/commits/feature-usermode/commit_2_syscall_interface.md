# Feature: System Call Interface

## Commit Details
**Title:** sys: Implement user-space syscall library and kernel handlers
**Date:** 2025-11-30
**Author:** Antigravity (Assistant)

## Description
This commit introduces a proper system call interface for user-mode applications. It provides a C-callable library that wraps the low-level `int 0x80` mechanism, making it easy to write user programs without inline assembly. New syscalls for process management (`exit`, `yield`, `getpid`) are added alongside the existing `write` and `ticks` syscalls.

## Key Changes

### 1. Syscall Numbers (`include/sys/syscall_nums.h`)
- **NEW**: Centralized syscall number definitions
- Defines: `SYS_EXIT`, `SYS_WRITE`, `SYS_TICKS`, `SYS_YIELD`, `SYS_GETPID`

### 2. Kernel Syscall Handlers (`src/sys/syscall.c`)
- **`sys_exit`**: Terminates the calling task by calling `sched_kill(sched_get_current_pid())` and yielding
- **`sys_yield`**: Allows task to voluntarily give up CPU time
- **`sys_getpid`**: Returns the current task's process ID
- Updated syscall dispatch table to use designated initializers for clarity

### 3. Scheduler Support (`src/sched/sched.c`, `include/sched/sched.h`)
- **`sched_yield()`**: Halts CPU until next interrupt (simple cooperative scheduling)
- **`sched_get_current_pid()`**: Returns current task's ID for syscall handlers

### 4. User Library
#### [NEW] [lib/syscall.h](file:///e:/Projects/Robist-Ventures/PenOS/include/lib/syscall.h)
- User-facing API declarations for all syscalls
- Functions: `exit()`, `write()`, `ticks()`, `yield()`, `getpid()`

#### [NEW] [lib/syscall.c](file:///e:/Projects/Robist-Ventures/PenOS/src/lib/syscall.c)
- Generic `syscall0()` and `syscall1()` helpers using inline assembly
- Wrapper functions that call the appropriate syscall with proper arguments
- Clean C interface hiding the `int 0x80` mechanism

### 5. Shell Integration (`src/shell/shell.c`)
- Updated `user_mode_test_task` to use the new syscall library
- Demonstrates: `getpid()`, `write()`, `yield()`, and `exit()`
- Task now terminates cleanly instead of spinning forever
- **Important**: All string literals are declared as stack arrays to ensure they're in user-accessible memory (not in kernel's read-only data section)

### 6. Build System (`Makefile`)
- Added `build/lib/syscall.o` to kernel binary link command

## Testing
- **Verification**: Run `usermode` in the shell
- **Expected Output**:
  ```
  Spawning User Mode task...
  Spawned user task PID: <id>
  [User] Hello from Ring 3! PID=<id>
  [User] Yielding...
  [User] Back from yield. Exiting...
  ```

## Benefits
- **Cleaner Code**: User programs no longer need inline assembly
- **Extensible**: Easy to add new syscalls by updating the table
- **Standard Interface**: Familiar C function calls for user applications
- **Process Control**: Tasks can now exit cleanly and cooperate with the scheduler

## Future Work
- Implement more syscalls (read, open, close, etc.)
- Add error handling and return codes
- Implement proper process cleanup on exit
- Add syscall argument validation
