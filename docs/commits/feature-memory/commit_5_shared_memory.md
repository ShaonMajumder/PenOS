# Commit 5: Shared Memory IPC

## Overview
This commit implements a Shared Memory (SHM) mechanism for Inter-Process Communication (IPC), allowing processes to share physical memory pages while maintaining page-level protection.

## Changes
- **`include/mem/shm.h`**: Defined the Shared Memory interface (`shm_get`, `shm_attach`).
- **`src/mem/shm.c`**: Implemented the SHM manager.
  - **`shm_get(key, size, flags)`**: Creates or retrieves a shared memory region identified by a key. It allocates physical pages from the PMM.
  - **`shm_attach(id, addr, flags)`**: Maps the physical pages of the region into the current process's virtual address space.
- **`src/kernel.c`**: Initialized the SHM subsystem.
- **`Makefile`**: Added `shm.o` to the build.
- **Version Bump**: Updated PenOS version to **0.10.0** to reflect this major feature addition.

## Page-Level Protection
The implementation strictly adheres to page-level protection:
- Shared pages are mapped with `PAGE_USER | PAGE_RW` permissions only in the specific virtual address range requested by the attaching process.
- These pages are distinct from the process's private heap/stack and the kernel's memory.
- The physical frames are managed centrally by the SHM manager, ensuring no accidental overlaps.

## Usage
(Future) User programs will use syscalls wrapping these functions to share data:
1. Process A calls `shm_get(1234, 4096, SHM_CREAT)` -> Returns ID 1.
2. Process A calls `shm_attach(1, NULL, 0)` -> Returns virtual address `0xA0000000`.
3. Process B calls `shm_get(1234, 4096, 0)` -> Returns ID 1.
4. Process B calls `shm_attach(1, NULL, 0)` -> Returns virtual address `0xA0000000` (or different).
5. Both processes can now read/write to that address to communicate.
