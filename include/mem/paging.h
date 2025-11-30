#ifndef MEM_PAGING_H
#define MEM_PAGING_H
#include <stdint.h>

#define PAGE_SIZE        0x1000
#define PAGE_PRESENT     0x00000001
#define PAGE_RW          0x00000002
#define PAGE_USER        0x00000004
#define KERNEL_VIRT_BASE 0xC0000000

void paging_init(void);
void paging_map(uint32_t virt, uint32_t phys, uint32_t flags);
void paging_unmap(uint32_t virt);
uint32_t paging_virt_to_phys(uint32_t virt);

// Per-process page directory management
uint32_t paging_create_directory(void);
uint32_t paging_clone_directory(uint32_t src_pd_phys);
void paging_switch_directory(uint32_t pd_phys);
void paging_destroy_directory(uint32_t pd_phys);
uint32_t paging_get_kernel_directory(void);

// Manually swap out a page (for testing)
int paging_swap_out(uint32_t virt);

#endif
