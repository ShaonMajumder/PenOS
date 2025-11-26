#include "mem/paging.h"
#include "ui/console.h"
#include <string.h>

#define PAGE_TABLE_ENTRIES 1024
#define PAGE_DIRECTORY_ENTRIES 1024
#define IDENTITY_PD_ENTRIES     PAGE_DIRECTORY_ENTRIES /* map the full 4 GiB */
#define IDENTITY_LIMIT_BYTES    (IDENTITY_PD_ENTRIES * 4 * 1024 * 1024U)

/* One global page directory + page tables for a full identity map. */
static uint32_t page_directory[PAGE_DIRECTORY_ENTRIES] __attribute__((aligned(4096)));
static uint32_t page_tables[IDENTITY_PD_ENTRIES][PAGE_TABLE_ENTRIES] __attribute__((aligned(4096)));

static inline void invlpg(uint32_t addr)
{
    __asm__ volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

static void load_page_directory(uint32_t phys)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(phys));
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U; /* set PG bit */
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

static uint32_t *get_page_table(uint32_t virt)
{
    uint32_t dir_index = virt >> 22;
    if (dir_index >= PAGE_DIRECTORY_ENTRIES) {
        return NULL;
    }
    return page_tables[dir_index];
}

void paging_map(uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t *table = get_page_table(virt);
    if (!table) {
        return;
    }
    uint32_t idx = (virt >> 12) & 0x3FFU;
    table[idx] = (phys & ~0xFFFU) | PAGE_PRESENT | (flags & 0xFFFU);
    invlpg(virt);
}

void paging_unmap(uint32_t virt)
{
    uint32_t *table = get_page_table(virt);
    if (!table) {
        return;
    }
    uint32_t idx = (virt >> 12) & 0x3FFU;
    table[idx] = 0;
    invlpg(virt);
}

uint32_t paging_virt_to_phys(uint32_t virt)
{
    uint32_t *table = get_page_table(virt);
    if (!table) {
        return 0;
    }
    uint32_t idx = (virt >> 12) & 0x3FFU;
    uint32_t entry = table[idx];
    if (!(entry & PAGE_PRESENT)) {
        return 0;
    }
    return (entry & ~0xFFFU) | (virt & 0xFFFU);
}

void paging_init(void)
{
    /* Build an identity-mapped page directory covering the full 4 GiB space. */
    memset(page_directory, 0, sizeof(page_directory));
    memset(page_tables, 0, sizeof(page_tables));

    for (uint32_t pd = 0; pd < PAGE_DIRECTORY_ENTRIES; ++pd) {
        page_directory[pd] = ((uint32_t)page_tables[pd]) | PAGE_PRESENT | PAGE_RW;
        for (uint32_t pt = 0; pt < PAGE_TABLE_ENTRIES; ++pt) {
            uint32_t phys = (pd << 22) | (pt << 12);
            page_tables[pd][pt] = phys | PAGE_PRESENT | PAGE_RW;
        }
    }

    load_page_directory((uint32_t)page_directory);
    console_write("Paging enabled with identity map.\n");
}
