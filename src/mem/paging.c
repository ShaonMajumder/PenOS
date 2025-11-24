#include "mem/paging.h"
#include "mem/pmm.h"

static uint32_t *page_directory = (uint32_t *)0x9C000; // arbitrary free page
static uint32_t *first_table = (uint32_t *)0x9D000;

void paging_init(void)
{
    for (int i = 0; i < 1024; ++i) {
        first_table[i] = (i * 0x1000) | 3; // present, rw
    }
    for (int i = 0; i < 1024; ++i) {
        page_directory[i] = 0x00000002;
    }
    page_directory[0] = ((uint32_t)first_table) | 3;

    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory));
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}
