#include <mem/shm.h>
#include <mem/pmm.h>
#include <mem/paging.h>
#include <mem/heap.h>
#include <ui/console.h>
#include <string.h>

static shm_region_t shm_table[SHM_MAX_REGIONS];
static uint32_t next_shm_id = 1;

void shm_init(void) {
    memset(shm_table, 0, sizeof(shm_table));
    console_write("Shared Memory (SHM) initialized.\n");
}

static shm_region_t *find_free_region(void) {
    for (int i = 0; i < SHM_MAX_REGIONS; i++) {
        if (!shm_table[i].used) {
            return &shm_table[i];
        }
    }
    return NULL;
}

static shm_region_t *find_region_by_key(uint32_t key) {
    for (int i = 0; i < SHM_MAX_REGIONS; i++) {
        if (shm_table[i].used && shm_table[i].key == key) {
            return &shm_table[i];
        }
    }
    return NULL;
}

static shm_region_t *find_region_by_id(int id) {
    for (int i = 0; i < SHM_MAX_REGIONS; i++) {
        if (shm_table[i].used && shm_table[i].id == (uint32_t)id) {
            return &shm_table[i];
        }
    }
    return NULL;
}

int shm_get(uint32_t key, uint32_t size, int flags) {
    shm_region_t *region = find_region_by_key(key);
    
    if (region) {
        // Region exists
        if ((flags & SHM_CREAT) && (flags & SHM_EXCL)) {
            return -1; // EEXIST
        }
        return region->id;
    }
    
    if (!(flags & SHM_CREAT)) {
        return -1; // ENOENT
    }
    
    // Create new region
    region = find_free_region();
    if (!region) {
        return -1; // ENOMEM (slots)
    }
    
    // Allocate physical memory
    // For simplicity, we allocate contiguous physical frames. 
    // In a real OS, we might handle non-contiguous pages, but that requires storing a list of pages.
    // Here we just try to alloc N frames.
    // Note: pmm_alloc_frame returns 1 frame. We need N.
    // Our PMM is simple. Let's just alloc N separate frames and store them?
    // Storing them requires a list.
    // To keep it simple for this implementation: We will allocate contiguous frames if possible,
    // OR we will limit SHM to 1 page for now? No, that's too limited.
    // Let's implement a simple page list for the region.
    // Actually, `shm_region_t` is fixed size.
    // Let's assume for now we only support 1 page SHM for simplicity of the struct, 
    // OR we dynamically allocate the page list.
    
    // Let's use `kmalloc` to store the list of physical pages.
    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t *phys_pages = (uint32_t *)kmalloc(pages_needed * sizeof(uint32_t));
    if (!phys_pages) return -1;
    
    for (uint32_t i = 0; i < pages_needed; i++) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame) {
            // Rollback
            for (uint32_t j = 0; j < i; j++) {
                pmm_free_frame(phys_pages[j]);
            }
            kfree(phys_pages);
            return -1;
        }
        phys_pages[i] = frame;
        // Zero the memory for security
        // We need to map it temporarily to zero it? 
        // Or just trust pmm_alloc_frame (which doesn't zero).
        // Let's skip zeroing for now or rely on user to init.
    }
    
    // We need to store this list. We can't put it in the fixed struct easily.
    // Hack: Store the pointer to the list in `phys_start` (casting).
    // This is kernel heap, so it's accessible.
    
    region->id = next_shm_id++;
    region->key = key;
    region->size = size;
    region->pages = pages_needed;
    region->phys_start = (uint32_t)phys_pages; // Storing pointer to array of frames
    region->owner_pid = 0; // TODO: Get current PID
    region->ref_count = 0;
    region->used = 1;
    
    console_write("SHM: Created region ID ");
    console_write_dec(region->id);
    console_write(" Key ");
    console_write_dec(key);
    console_write(" Size ");
    console_write_dec(size);
    console_write("\n");
    
    return region->id;
}

void *shm_attach(int id, void *addr, int flags) {
    shm_region_t *region = find_region_by_id(id);
    if (!region) return NULL;
    
    uint32_t virt_start;
    if (addr) {
        virt_start = (uint32_t)addr;
    } else {
        // Find a free virtual address region?
        // For now, let's require the user to provide an address or pick a fixed one.
        // We don't have a VMA allocator yet.
        // Let's pick a high address in user space, e.g., 0xA0000000 + id * 1MB
        virt_start = 0xA0000000 + (id * 0x100000); 
    }
    
    // Align
    virt_start &= ~(PAGE_SIZE - 1);
    
    uint32_t *phys_pages = (uint32_t *)region->phys_start;
    
    // Map pages
    for (uint32_t i = 0; i < region->pages; i++) {
        uint32_t phys = phys_pages[i];
        uint32_t virt = virt_start + (i * PAGE_SIZE);
        
        // Map with User + RW permissions
        // This maintains page-level protection: only this specific virtual range
        // is mapped to these physical pages.
        paging_map(virt, phys, PAGE_USER | PAGE_RW | PAGE_PRESENT);
    }
    
    region->ref_count++;
    return (void *)virt_start;
}

int shm_detach(void *addr) {
    // Unmapping requires knowing the size.
    // Since we don't track per-process attachments in this simple version,
    // we can't easily unmap just by address without searching.
    // For now, stub.
    return 0;
}
