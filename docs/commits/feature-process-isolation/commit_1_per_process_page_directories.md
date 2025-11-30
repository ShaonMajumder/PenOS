# Feature: Process Isolation

## Commit Details
**Title:** mem/sched: Implement per-process page directories for memory isolation
**Date:** 2025-11-30
**Author:** Antigravity (Assistant)

## Description
This commit implements true process isolation by giving each process its own page directory. Processes now have separate virtual address spaces, preventing them from accessing each other's memory while sharing the kernel space.

## Key Changes

### 1. Page Directory Management (`src/mem/paging.c`, `include/mem/paging.h`)
- **`paging_create_directory()`**: Allocates a new page directory and copies kernel mappings (0xC0000000+)
- **`paging_clone_directory()`**: Clones a page directory for fork(), copying user space and sharing kernel space
- **`paging_switch_directory()`**: Switches CR3 to a new page directory (TLB flush)
- **`paging_destroy_directory()`**: Frees a page directory and all its user-space pages
- **`paging_get_kernel_directory()`**: Returns the kernel page directory physical address

### 2. Scheduler Integration (`src/sched/sched.c`)
- **Task Structure**: Added `page_directory_phys` field to `task_entry_t`
- **`sched_init()`**: Stores kernel page directory in task 0 (main kernel task)
- **`sched_spawn_user()`**: 
  - Creates new page directory for each user process
  - Temporarily switches to new directory to map user memory
  - Switches back to kernel directory after setup
- **`sched_tick()`**: Switches to new process's page directory on context switch
- **`destroy_task()`**: Frees process's page directory on termination

## Memory Layout

### Kernel Space (Shared across all processes)
```
0xC0000000 - 0xFFFFFFFF  Kernel (shared)
├── 0xC0000000  Identity mapped: 0-16MB
├── 0xC0100000  Kernel code/data
├── Kernel heap
└── 0xFFC00000  Recursive mapping
```

### User Space (Per-process, isolated)
```
0x00000000 - 0xBFFFFFFF  User space (isolated)
├── 0x08048000  User code
├── 0x08049000  User data
└── 0xBFFFF000  User stack
```

## Implementation Details

### Page Directory Creation
Each new user process gets its own page directory with:
- **Kernel mappings copied** from the kernel directory (entries 768-1023)
- **User space empty** initially (entries 0-767)
- **Recursive mapping** at entry 1023 for easy page table access

### Context Switching
On every context switch (`sched_tick`):
1. Save current task's state
2. Pick next task to run
3. Update TSS ESP0 (kernel stack)
4. **Switch CR3** to new task's page directory
5. Return to new task

### Memory Isolation
- Each process sees its own user space (0x00000000 - 0xBFFFFFFF)
- All processes share kernel space (0xC0000000 - 0xFFFFFFFF)
- Kernel accessible from all processes (for syscalls)
- User processes cannot access other processes' memory

## Performance Impact

### TLB Flush
- Every context switch flushes the TLB (unavoidable on x86-32)
- Performance cost: ~100-1000 cycles per switch
- Acceptable for a demo OS with low context switch frequency

### Memory Overhead
- **+4KB per process** for page directory
- Kernel page tables shared (no duplication)
- User page tables allocated on-demand

## Testing

### Test 1: Separate Address Spaces
```c
// Spawn two user processes
// Each writes to same virtual address (e.g., 0x08048000)
// Verify they see different physical pages
```

### Test 2: Kernel Access
```c
// User process makes syscall
// Kernel code accessible from all processes
// Syscall succeeds
```

### Test 3: Memory Protection
```c
// Process A tries to access Process B's memory
// Should get Page Fault (protection violation)
```

## Benefits
- ✅ **Security**: Processes cannot interfere with each other
- ✅ **Stability**: Process crash doesn't affect others
- ✅ **Foundation**: Enables fork/exec implementation
- ✅ **Standard**: Matches Unix/Linux process model

## Limitations
- ❌ **No Copy-on-Write**: fork() will be slow (full copy)
- ❌ **TLB Flush**: Performance cost on context switch
- ❌ **No ASID/PCID**: x86-32 limitation (x86-64 has PCID)

## Future Enhancements
- **Copy-on-Write (COW)**: Share pages until written
- **Lazy Allocation**: Allocate pages on-demand (page faults)
- **Shared Memory**: IPC via shared pages
- **Memory-Mapped Files**: Map files into address space

## Files Modified
- `include/mem/paging.h` - Added page directory management API
- `src/mem/paging.c` - Implemented directory create/clone/switch/destroy
- `src/sched/sched.c` - Added `page_directory_phys` to tasks, integrated with scheduler

## Verification
**Status:** ✅ Implemented, pending testing

**To Test:**
1. Build: `make clean && make iso`
2. Run: `sudo make run`
3. Execute: `usermode` command
4. Spawn multiple user processes
5. Verify separate address spaces

## References
- [Intel SDM Vol 3A: Paging](https://www.intel.com/content/www/us/en/architecture-and-technology/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.html)
- [OSDev Wiki: Paging](https://wiki.osdev.org/Paging)
- [Linux Process Address Space](https://www.kernel.org/doc/html/latest/vm/process_addrs.html)
