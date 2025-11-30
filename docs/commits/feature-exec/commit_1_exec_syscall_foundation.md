# Feature: exec() Syscall & ELF Loading

## Commit Details
**Title:** sys/fs: Implement exec() syscall and ELF binary loading
**Date:** 2025-11-30
**Status:** ✅ Implemented

## Description
This commit implements the `exec()` syscall, enabling PenOS to load and execute standard ELF binaries from the filesystem. It integrates the VirtIO-9P driver for binary file reading, updates the ELF loader to handle binary data, and adds the necessary kernel infrastructure to replace a process's image with a new program.

## Key Changes

### 1. Syscall Interface (`src/sys/syscall.c`, `include/sys/syscall_nums.h`)
- **`SYS_EXEC` (5)**: New syscall for executing programs.
- **`sys_exec()`**: Kernel handler that:
  1. Reads the ELF binary from the filesystem using `p9_read_file`.
  2. Parses and loads the ELF segments using `elf_load`.
  3. Allocates a new user stack (16KB at `0xBFFFF000`).
  4. Updates the interrupt frame to jump to the new entry point.
  5. Resets general-purpose registers.

### 2. ELF Loader (`src/fs/elf.c`)
- **Binary Loading**: Updated `elf_load` to use `p9_read_file` instead of text-based `fs_find`.
- **Memory Mapping**: Allocates and maps pages for `PT_LOAD` segments with correct permissions (`PAGE_USER`, `PAGE_RW`).
- **BSS Handling**: Zero-initializes the BSS section (where `memsz > filesz`).
- **Cleanup**: Frees the temporary file buffer after loading.

### 3. Filesystem Integration (`src/fs/9p.c`)
- **`p9_read_file()`**: New function to read an entire file into a kernel buffer.
- **`p9_get_file_size()`**: Helper to determine file size (currently estimates, pending full `TGETATTR`).
- **Binary Support**: Handles arbitrary binary data, not just null-terminated strings.

### 4. Scheduler (`src/sched/sched.c`)
- **`sched_spawn_elf()`**: New function to spawn a process directly from an ELF file.
  - Creates a new page directory.
  - Loads the ELF binary.
  - Allocates kernel and user stacks.
  - Sets up the task structure and interrupt frame.

### 5. Shell (`src/shell/shell.c`)
- **`exec <path>`**: New command to execute a program.
  - Spawns a new process using `sched_spawn_elf`.
  - Prints the PID of the new process.

## User Space Support
- **`user/hello.c`**: Test program that prints "Hello from userspace ELF!" and its PID.
- **`user/Makefile`**: Build script to compile user programs as static ELF binaries (`-static -nostdlib`).

## Memory Layout
```
0xC0000000  Kernel Base
    |
0xBFFFF000  User Stack (grows down)
    |
    ...     (Gap)
    |
0x08048000  ELF Load Address (.text, .data, .bss)
    |
0x00000000  Bottom of memory
```

## Verification
**Status:** ✅ Implemented & Ready for Testing

**Test Plan:**
1. Build the OS: `make clean && make iso`
2. Build the user program: `cd user && make`
3. Run in QEMU: `make run`
4. In the shell:
   ```bash
   exec /mnt/user/hello
   ```
5. Expected Output:
   ```
   [exec] Loading: /mnt/user/hello
   [ELF] Entry point: 0x080480XX
   [ELF] LOAD segment...
   [ELF] Load complete
   Started process 2: /mnt/user/hello
   Hello from userspace ELF!
   My PID is: 2
   ```

## Limitations
- **No Arguments**: `exec` does not yet pass command-line arguments (argc/argv).
- **No Environment**: No environment variables support.
- **Static Only**: Only supports statically linked binaries.
- **No Fork**: `fork()` is not yet implemented, so `exec` is currently used to spawn new independent processes via the shell.

## Future Work
- Implement `fork()` to allow process cloning.
- Add argument passing (`argc`, `argv`) to `sys_exec`.
- Implement `wait()` for parent processes to wait for children.
- Support dynamic linking (shared libraries).
