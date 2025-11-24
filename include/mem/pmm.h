#ifndef MEM_PMM_H
#define MEM_PMM_H
#include <stddef.h>
#include <stdint.h>
#include "multiboot.h"

void pmm_init(multiboot_info_t *mb_info);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t frame);
uint32_t pmm_total_memory(void);

#endif
