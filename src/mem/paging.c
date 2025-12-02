#include "mem/paging.h"
#include "mem/pmm.h"
#include "ui/console.h"
#include <string.h>
#include "arch/x86/interrupts.h"

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

static uint32_t evict_pd_idx = 0;
static uint32_t evict_pt_idx = 0;

static uint32_t get_virt_from_indices(uint32_t pd_index, uint32_t pt_index) {
    return (pd_index << 22) | (pt_index << 12);
}

static int paging_evict_page(void) {
    int pages_checked = 0;
    // We only scan user space (0 to 0xC0000000), which is PD entries 0 to 767.
    // 768 entries * 1024 pages = 786432 pages max.
    int max_checks = 768 * 1024 * 2; // 2 passes
    
    while (pages_checked < max_checks) {
        // Advance indices
        evict_pt_idx++;
        if (evict_pt_idx >= 1024) {
            evict_pt_idx = 0;
            evict_pd_idx++;
            if (evict_pd_idx >= 768) {
                evict_pd_idx = 0;
            }
        }
        
        if (current_pd[evict_pd_idx] & PAGE_PRESENT) {
            uint32_t *pt = phys_to_ptr(current_pd[evict_pd_idx] & ~0xFFF);
            if (pt[evict_pt_idx] & PAGE_PRESENT) {
                 // Check Accessed bit (Bit 5)
                 if (pt[evict_pt_idx] & 0x20) {
                     pt[evict_pt_idx] &= ~0x20; // Clear accessed bit
                     invlpg(get_virt_from_indices(evict_pd_idx, evict_pt_idx));
                 } else {
                     // Found victim (Accessed bit is 0)
                     uint32_t virt = get_virt_from_indices(evict_pd_idx, evict_pt_idx);
                     // Try to swap out. If successful, we freed a frame.
                     if (paging_swap_out(virt) == 0) {
                         return 1;
                     }
                 }
            }
        } else {
            // Optimization: If PD entry is empty, skip all its PT entries
            evict_pt_idx = 1023; // Next loop iteration will roll over to next PD
        }
        pages_checked++;
    }
    return 0;
}

static uint32_t alloc_frame_zero(void)
{
    uint32_t phys = pmm_alloc_frame();
    if (phys == 0) {
        // Try to evict a page to free up memory
        if (paging_evict_page()) {
            phys = pmm_alloc_frame();
        }
        
        // Check again
        if (phys == 0) {
            console_write("PMM exhausted. Eviction failed or no swap space.\n");
            for (;;) {
                __asm__ volatile ("cli; hlt");
            }
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
        // Map as User-accessible for demo purposes (allows Ring 3 to execute kernel code)
        // In a real OS, this would be a security issue
        paging_map(phys, phys, PAGE_RW | PAGE_USER);
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

#include <mem/swap.h>

// ...

void page_fault_handler(interrupt_frame_t *frame)
{
    uint32_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));

    int present = frame->err_code & 0x1;
    int rw = frame->err_code & 0x2;
    int user = frame->err_code & 0x4;
    int reserved = frame->err_code & 0x8;

    uint32_t page_aligned_virt = faulting_address & ~0xFFF;

    // Check if page is swapped out
    uint32_t *table = get_page_table(page_aligned_virt, 0, 0);
    if (table) {
        uint32_t pt_index = (page_aligned_virt >> 12) & 0x3FFU;
        uint32_t entry = table[pt_index];
        
        // If entry is not present but has PAGE_SWAPPED bit, it's a swap slot
        if (!(entry & PAGE_PRESENT) && (entry & PAGE_SWAPPED)) {
            uint32_t swap_slot = entry >> 12;
            
            console_write("Swap: Page fault on swapped page. Slot: ");
            console_write_dec(swap_slot);
            console_write("\n");
            
            // Allocate new frame
            uint32_t phys = alloc_frame_zero();
            
            // Map it first so we can write to it
            paging_map(page_aligned_virt, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
            
            // Read from swap
            if (swap_in(swap_slot, (void*)page_aligned_virt) != 0) {
                console_write("Swap: Failed to read from swap!\n");
                // Handle error...
            }
            
            // Free swap slot
            swap_free(swap_slot);
            return;
        }
    }

    // Demand paging: if page is not present and it's a user access, allocate it
    if (!present && user) {
        uint32_t phys = alloc_frame_zero();
        paging_map(page_aligned_virt, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
        return;
    }

    console_write("Page Fault! (");
    if (present) console_write("present ");
    if (rw) console_write("read-only ");
    if (user) console_write("user-mode ");
    if (reserved) console_write("reserved ");
    console_write(") at ");
    console_write_hex(faulting_address);
    console_write("\n");
    
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

int paging_swap_out(uint32_t virt) {
    uint32_t page_aligned_virt = virt & ~0xFFF;
    uint32_t *table = get_page_table(page_aligned_virt, 0, 0);
    if (!table) return -1;
    
    uint32_t pt_index = (page_aligned_virt >> 12) & 0x3FFU;
    uint32_t entry = table[pt_index];
    
    if (!(entry & PAGE_PRESENT)) return -1; // Not present
    
    uint32_t phys = entry & ~0xFFF;
    uint32_t swap_slot;
    
    // Write to swap
    if (swap_out((void*)page_aligned_virt, &swap_slot) != 0) {
        return -1;
    }
    
    // Update PTE: Not Present, store swap slot in bits 12-31, set PAGE_SWAPPED
    table[pt_index] = (swap_slot << 12) | PAGE_SWAPPED; // Present bit is 0
    invlpg(page_aligned_virt);
    
    // Free physical frame
    pmm_free_frame(phys);
    
    console_write("Swap: Swapped out page ");
    console_write_hex(page_aligned_virt);
    console_write(" to slot ");
    console_write_dec(swap_slot);
    console_write("\n");
    
    return 0;
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
    
    register_interrupt_handler(14, page_fault_handler);
    console_write("Paging enabled. Kernel mapped at higher half.\n");
}

// Get kernel page directory physical address
uint32_t paging_get_kernel_directory(void)
{
    return current_pd_phys;
}

// Create a new page directory for a process
uint32_t paging_create_directory(void)
{
    // Allocate new page directory
    uint32_t new_pd_phys = alloc_frame_zero();
    uint32_t *new_pd = phys_to_ptr(new_pd_phys);
    
    // Copy kernel mappings (upper half: 0xC0000000+)
    // Kernel occupies page directory entries 768-1023 (0xC0000000 - 0xFFFFFFFF)
    for (uint32_t i = 768; i < 1024; i++) {
        new_pd[i] = current_pd[i];
    }
    
    // Set up recursive mapping for new directory
    new_pd[1023] = new_pd_phys | PAGE_PRESENT | PAGE_RW;
    
    return new_pd_phys;
}

// Clone a page directory (for fork)
uint32_t paging_clone_directory(uint32_t src_pd_phys)
{
    uint32_t *src_pd = phys_to_ptr(src_pd_phys);
    uint32_t new_pd_phys = alloc_frame_zero();
    uint32_t *new_pd = phys_to_ptr(new_pd_phys);
    
    // Copy all entries
    for (uint32_t i = 0; i < 1024; i++) {
        if (i >= 768) {
            // Kernel space: share page tables
            new_pd[i] = src_pd[i];
        } else {
            // User space: clone page tables
            if (src_pd[i] & PAGE_PRESENT) {
                uint32_t src_pt_phys = src_pd[i] & ~0xFFF;
                uint32_t *src_pt = phys_to_ptr(src_pt_phys);
                
                // Allocate new page table
                uint32_t new_pt_phys = alloc_frame_zero();
                uint32_t *new_pt = phys_to_ptr(new_pt_phys);
                
                // Copy page table entries
                for (uint32_t j = 0; j < 1024; j++) {
                    if (src_pt[j] & PAGE_PRESENT) {
                        // Allocate new physical page
                        uint32_t new_page_phys = alloc_frame_zero();
                        
                        // Copy page content
                        uint32_t src_page_phys = src_pt[j] & ~0xFFF;
                        memcpy(phys_to_ptr(new_page_phys), phys_to_ptr(src_page_phys), PAGE_SIZE);
                        
                        // Set up new page table entry with same flags
                        new_pt[j] = new_page_phys | (src_pt[j] & 0xFFF);
                    }
                }
                
                // Set up new page directory entry with same flags
                new_pd[i] = new_pt_phys | (src_pd[i] & 0xFFF);
            }
        }
    }
    
    // Set up recursive mapping
    new_pd[1023] = new_pd_phys | PAGE_PRESENT | PAGE_RW;
    
    return new_pd_phys;
}

// Switch to a different page directory
void paging_switch_directory(uint32_t pd_phys)
{
    if (pd_phys == current_pd_phys) {
        return; // Already using this directory
    }
    
    current_pd_phys = pd_phys;
    current_pd = phys_to_ptr(pd_phys);
    
    // Load CR3 with new page directory
    __asm__ volatile ("mov %0, %%cr3" :: "r"(pd_phys) : "memory");
}

// Destroy a page directory and free its memory
void paging_destroy_directory(uint32_t pd_phys)
{
    if (pd_phys == current_pd_phys) {
        console_write("[Paging] Cannot destroy current page directory\n");
        return;
    }
    
    uint32_t *pd = phys_to_ptr(pd_phys);
    
    // Free user space page tables and pages
    for (uint32_t i = 0; i < 768; i++) {
        if (pd[i] & PAGE_PRESENT) {
            uint32_t pt_phys = pd[i] & ~0xFFF;
            uint32_t *pt = phys_to_ptr(pt_phys);
            
            // Free all pages in this page table
            for (uint32_t j = 0; j < 1024; j++) {
                if (pt[j] & PAGE_PRESENT) {
                    uint32_t page_phys = pt[j] & ~0xFFF;
                    pmm_free_frame(page_phys);
                }
            }
            
            // Free the page table itself
            pmm_free_frame(pt_phys);
        }
    }
    
    // Free the page directory
    pmm_free_frame(pd_phys);
}
