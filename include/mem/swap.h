#ifndef MEM_SWAP_H
#define MEM_SWAP_H

#include <stdint.h>

// Initialize the swap subsystem
void swap_init(void);

// Write a page from memory to swap space
// Returns 0 on success, -1 on failure
// *swap_slot is updated with the index of the slot used
int swap_out(void *buffer, uint32_t *swap_slot);

// Read a page from swap space into memory
// Returns 0 on success, -1 on failure
int swap_in(uint32_t swap_slot, void *buffer);

// Free a swap slot
void swap_free(uint32_t swap_slot);

// Check if swap is available
int swap_available(void);

#endif
