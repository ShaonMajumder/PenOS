#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "ui/console.h"

#define HEAP_START (KERNEL_VIRT_BASE + 0x01000000) /* 0xC1000000 */
#define HEAP_SIZE  (16 * 1024 * 1024)

static uint32_t heap_curr = HEAP_START;
static uint32_t heap_mapped_end = HEAP_START;
static const uint32_t heap_end = HEAP_START + HEAP_SIZE;

static inline uint32_t align_up(uint32_t value, uint32_t align)
{
    return (value + align - 1U) & ~(align - 1U);
}

static void map_new_page(uint32_t virt)
{
    uint32_t frame = pmm_alloc_frame();
    if (frame == 0) {
        console_write("kmalloc: out of frames\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
    paging_map(virt, frame, PAGE_RW);
}

static void ensure_space(uint32_t target_end)
{
    while (heap_mapped_end < target_end) {
        map_new_page(heap_mapped_end);
        heap_mapped_end += PAGE_SIZE;
    }
}

void heap_init(void)
{
    heap_curr = HEAP_START;
    heap_mapped_end = HEAP_START;
    console_write("Kernel heap ready.\n");
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    uint32_t aligned = align_up(heap_curr, sizeof(uint32_t));
    uint32_t new_end = aligned + (uint32_t)size;
    if (new_end > heap_end) {
        return NULL;
    }
    ensure_space(new_end);
    void *ptr = (void *)aligned;
    heap_curr = new_end;
    return ptr;
}

void kfree(void *ptr)
{
    (void)ptr;
    /* TODO: implement a free list */
}
