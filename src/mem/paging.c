#include "mem/paging.h"
#include "mem/pmm.h"
#include "ui/console.h"
#include <string.h>

#define PAGE_TABLE_ENTRIES 1024
#define PAGE_DIRECTORY_ENTRIES 1024
#define PAGE_TABLE_SIZE (PAGE_TABLE_ENTRIES * sizeof(uint32_t))

static uint32_t current_pd_phys = 0;
static uint32_t *current_pd = 0;

static inline uint32_t align_up(uint32_t value, uint32_t align)
{
    return (value + align - 1U) & ~(align - 1U);
}

static inline void invlpg(uint32_t addr)
{
    __asm__ volatile ("invlpg (%0)" :: "r"(addr) : "memory");
}

static uint32_t *phys_to_ptr(uint32_t phys)
{
    return (uint32_t *)(phys);
}

static uint32_t alloc_frame_zero(void)
{
    uint32_t phys = pmm_alloc_frame();
    if (phys == 0) {
        console_write("PMM exhausted while building paging.\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    memset(phys_to_ptr(phys), 0, PAGE_SIZE);
    return phys;
}

static uint32_t *get_page_table(uint32_t virt, int create, uint32_t flags)
{
    uint32_t pd_index = virt >> 22;
    uint32_t entry = current_pd[pd_index];
    
    if (!(entry & PAGE_PRESENT)) {
        if (!create) {
            return NULL;
        }
        uint32_t table_phys = alloc_frame_zero();
        uint32_t pd_flags = PAGE_PRESENT | PAGE_RW;
        if (flags & PAGE_USER) {
            pd_flags |= PAGE_USER;
        }
        current_pd[pd_index] = table_phys | pd_flags;
        entry = current_pd[pd_index];
    } else {
        // If PDE exists but we need User access, update it
        if ((flags & PAGE_USER) && !(entry & PAGE_USER)) {
            current_pd[pd_index] |= PAGE_USER;
        }
    }
    
    uint32_t table_phys = entry & ~0xFFFU;
    return phys_to_ptr(table_phys);
}

void paging_map(uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t *table = get_page_table(virt, 1, flags);
    uint32_t pt_index = (virt >> 12) & 0x3FFU;
    table[pt_index] = (phys & ~0xFFFU) | PAGE_PRESENT | (flags & 0xFFFU);
    invlpg(virt);
}

void paging_unmap(uint32_t virt)
{
    uint32_t *table = get_page_table(virt, 0, 0);
    if (!table) {
        return;
    }
    uint32_t pt_index = (virt >> 12) & 0x3FFU;
    table[pt_index] = 0;
    invlpg(virt);
}

uint32_t paging_virt_to_phys(uint32_t virt)
{
    uint32_t *table = get_page_table(virt, 0, 0);
    if (!table) {
        return 0;
    }
    uint32_t pt_index = (virt >> 12) & 0x3FFU;
    uint32_t entry = table[pt_index];
    if (!(entry & PAGE_PRESENT)) {
        return 0;
    }
    return (entry & ~0xFFFU) | (virt & 0xFFFU);
}

static void load_page_directory(uint32_t phys)
{
    __asm__ volatile ("mov %0, %%cr3" :: "r"(phys));
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U;
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));
}

static void map_identity_region(uint32_t length)
{
    uint32_t pages = align_up(length, PAGE_SIZE) / PAGE_SIZE;
    for (uint32_t i = 0; i < pages; ++i) {
        uint32_t phys = i * PAGE_SIZE;
        paging_map(phys, phys, PAGE_RW);
    }
}

static void map_kernel_higher_half(void)
{
    extern uint8_t end;
    uint32_t kernel_phys_start = 0x00100000U; /* linker places kernel at 1 MiB */
    uint32_t kernel_phys_end = align_up((uint32_t)(uintptr_t)&end, PAGE_SIZE);
    for (uint32_t phys = kernel_phys_start; phys < kernel_phys_end; phys += PAGE_SIZE) {
        uint32_t virt = KERNEL_VIRT_BASE + phys;
        paging_map(virt, phys, PAGE_RW);
    }
}

void paging_init(void)
{
    current_pd_phys = alloc_frame_zero();
    current_pd = phys_to_ptr(current_pd_phys);

    /* Recursive mapping for easy PD/PT access later */
    current_pd[1023] = current_pd_phys | PAGE_PRESENT | PAGE_RW;

    map_identity_region(16 * 1024 * 1024); /* identity-map first 16 MiB */
    map_kernel_higher_half();

    load_page_directory(current_pd_phys);
    console_write("Paging enabled. Kernel mapped at higher half.\n");
}
