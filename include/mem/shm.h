#ifndef MEM_SHM_H
#define MEM_SHM_H

#include <stdint.h>
#include <stddef.h>

// Maximum number of shared memory regions
#define SHM_MAX_REGIONS 128

// Shared memory flags
#define SHM_CREAT 0x01
#define SHM_EXCL  0x02

// Structure representing a shared memory region
typedef struct {
    uint32_t id;            // Unique ID
    uint32_t key;           // User-provided key
    uint32_t phys_start;    // Physical starting address
    uint32_t size;          // Size in bytes
    uint32_t pages;         // Number of pages
    int owner_pid;          // Creator PID
    int ref_count;          // Number of attached processes
    int used;               // Is this slot used?
} shm_region_t;

void shm_init(void);

// Create or get a shared memory region
// Returns ID on success, -1 on failure
int shm_get(uint32_t key, uint32_t size, int flags);

// Attach a shared memory region to the current process's address space
// Returns virtual address on success, NULL on failure
void *shm_attach(int id, void *addr, int flags);

// Detach (unmap) a shared memory region
int shm_detach(void *addr);

#endif
