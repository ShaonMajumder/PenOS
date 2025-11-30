# Feature: ELF Loader (Work in Progress)

## Commit Details
**Title:** fs: Add ELF binary loader infrastructure
**Date:** 2025-11-30
**Status:** 🚧 In Progress - Foundation Complete

## Description
This commit introduces the foundation for loading and executing ELF (Executable and Linkable Format) binaries in PenOS. The ELF loader enables the OS to load external programs from the filesystem, a critical step toward supporting user applications.

**Current Status:** ELF structures and basic parser implemented. Full exec() and fork() implementation pending.

## Key Changes

### 1. ELF Structures (`include/fs/elf.h`)
- **ELF32 Header**: Complete `Elf32_Ehdr` structure with all fields
- **Program Header**: `Elf32_Phdr` for LOAD segments
- **Section Header**: `Elf32_Shdr` for section information
- **Constants**: ELF magic numbers, types, machine codes, segment types, flags
- **API**: `elf_validate()`, `elf_load()`, `elf_unload()` function prototypes

### 2. ELF Parser (`src/fs/elf.c`)
- **Validation**: Checks ELF magic number, class (32-bit), endianness (little-endian), type (executable), and machine (x86)
- **Loader**: Parses program headers, allocates memory for LOAD segments, maps pages as user-accessible
- **Memory Mapping**: Respects segment flags (read/write/execute), handles BSS (zero-initialized data)

## Implementation Details

### ELF Validation
```c
int elf_validate(const void *data, uint32_t size)
```
- Verifies ELF magic number (`\x7FELF`)
- Ensures 32-bit x86 executable
- Checks little-endian encoding

### ELF Loading
```c
uint32_t elf_load(const char *path, uint32_t *entry_point)
```
- Reads ELF file from filesystem
- Parses program headers
- Allocates and maps memory for each LOAD segment
- Copies file data and zeros BSS
- Returns entry point address

## Current Limitations

### ⚠️ Work in Progress
- **No exec() syscall yet** - Loader exists but not integrated with scheduler
- **No fork() implementation** - Requires per-process page tables
- **Binary file reading** - Current `fs_find()` API designed for text files
- **Memory cleanup** - `elf_unload()` not yet implemented
- **No process isolation** - Needs per-process page directories

### 📋 Pending Work
1. **Filesystem Integration**: Add binary file reading to VirtIO-9P
2. **exec() Syscall**: Replace current process with loaded ELF
3. **Process Isolation**: Per-process page tables
4. **fork() Syscall**: Clone address space (15-20 hours)
5. **Memory Management**: Proper cleanup on process exit

## Testing
**Status:** Not yet testable - requires exec() syscall integration

**Planned Tests:**
1. Create simple "Hello World" ELF binary
2. Load via `exec /mnt/hello`
3. Verify execution in user mode
4. Test segment permissions (read-only code, writable data)

## Architecture Notes

### ELF Binary Structure
```
ELF Header (52 bytes)
├── Magic: 0x7F 'E' 'L' 'F'
├── Class: 32-bit
├── Encoding: Little-endian
├── Type: Executable
├── Machine: x86 (EM_386)
└── Entry Point: 0x08048000 (typical)

Program Headers (32 bytes each)
├── PT_LOAD (Code): vaddr=0x08048000, flags=R-X
└── PT_LOAD (Data): vaddr=0x08049000, flags=RW-

Segments
├── .text (code)
├── .rodata (read-only data)
├── .data (initialized data)
└── .bss (zero-initialized data)
```

### Memory Layout
```
User Space:
0x08048000 - Code segment (R-X)
0x08049000 - Data segment (RW-)
0xBFFFF000 - User stack (RW-)

Kernel Space:
0xC0000000 - Kernel code/data
```

## Next Steps

### Phase 1: exec() Only (4-6 hours)
1. Add binary file reading to filesystem
2. Implement `sys_exec()` syscall
3. Integrate with scheduler (`sched_exec()`)
4. Create test ELF binary
5. Test loading and execution

### Phase 2: fork() Support (15-20 hours)
1. Per-process page directories
2. Address space cloning
3. `sys_fork()` implementation
4. Parent-child relationships
5. `sys_wait()` for process synchronization

## Files Modified
- `include/fs/elf.h` - New file (ELF structures)
- `src/fs/elf.c` - New file (ELF parser/loader)

## Files Pending
- `src/sys/syscall.c` - Add `sys_exec()`, `sys_fork()`, `sys_wait()`
- `src/sched/sched.c` - Add `sched_exec()`, `sched_fork()`
- `src/mem/paging.c` - Add `paging_clone_directory()`
- `src/lib/syscall.c` - Add user-space wrappers

## References
- [ELF Specification](http://www.skyfree.org/linux/references/ELF_Format.pdf)
- [OSDev Wiki: ELF](https://wiki.osdev.org/ELF)
- [System V ABI](https://refspecs.linuxfoundation.org/elf/elf.pdf)
