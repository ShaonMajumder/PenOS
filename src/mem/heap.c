#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "ui/console.h"
#include <string.h>

#define HEAP_START (KERNEL_VIRT_BASE + 0x01000000) /* 0xC1000000 */
#define HEAP_SIZE  (16 * 1024 * 1024)

typedef struct heap_block {
    struct heap_block *next;
    struct heap_block *prev;
    size_t size;
    int free;
} heap_block_t;

#define BLOCK_OVERHEAD (sizeof(heap_block_t))
#define MIN_SPLIT       32

static uint32_t heap_curr = HEAP_START;
static uint32_t heap_mapped_end = HEAP_START;
static const uint32_t heap_end = HEAP_START + HEAP_SIZE;
static heap_block_t *heap_head = NULL;

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
    memset((void *)virt, 0, PAGE_SIZE);
}

static void ensure_space(uint32_t target_end)
{
    while (heap_mapped_end < target_end) {
        map_new_page(heap_mapped_end);
        heap_mapped_end += PAGE_SIZE;
    }
}

static heap_block_t *request_block(size_t size)
{
    uint32_t start = align_up(heap_curr, sizeof(uint32_t));
    uint32_t total = start + (uint32_t)size;
    if (total + BLOCK_OVERHEAD > heap_end) {
        return NULL;
    }
    ensure_space(total + BLOCK_OVERHEAD);
    heap_block_t *block = (heap_block_t *)start;
    block->next = NULL;
    block->prev = NULL;
    block->size = size;
    block->free = 0;
    heap_curr = total + BLOCK_OVERHEAD;
    return block;
}

static void split_block(heap_block_t *block, size_t requested)
{
    size_t remaining = block->size - requested;
    if (remaining <= BLOCK_OVERHEAD + MIN_SPLIT) {
        return;
    }
    uint32_t new_addr = (uint32_t)((uint8_t *)block + BLOCK_OVERHEAD + requested);
    heap_block_t *new_block = (heap_block_t *)new_addr;
    new_block->size = remaining - BLOCK_OVERHEAD;
    new_block->free = 1;
    new_block->next = block->next;
    new_block->prev = block;
    if (new_block->next) {
        new_block->next->prev = new_block;
    }
    block->next = new_block;
    block->size = requested;
}

static void coalesce(heap_block_t *block)
{
    if (block->next && block->next->free) {
        heap_block_t *next = block->next;
        block->size += BLOCK_OVERHEAD + next->size;
        block->next = next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    if (block->prev && block->prev->free) {
        block->prev->size += BLOCK_OVERHEAD + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

void heap_init(void)
{
    heap_curr = HEAP_START;
    heap_mapped_end = HEAP_START;
    heap_head = NULL;
    console_write("Kernel heap ready.\n");
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    size = align_up((uint32_t)size, sizeof(uint32_t));
    heap_block_t *block = heap_head;
    while (block) {
        if (block->free && block->size >= size) {
            block->free = 0;
            split_block(block, size);
            return (uint8_t *)block + BLOCK_OVERHEAD;
        }
        block = block->next;
    }
    block = request_block(size);
    if (!block) {
        return NULL;
    }
    if (!heap_head) {
        heap_head = block;
    } else {
        heap_block_t *tail = heap_head;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = block;
        block->prev = tail;
    }
    return (uint8_t *)block + BLOCK_OVERHEAD;
}

void kfree(void *ptr)
{
    if (!ptr) {
        return;
    }
    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - BLOCK_OVERHEAD);
    if (block->free) {
        console_write("kfree: double free detected\n");
        return;
    }
    block->free = 1;
    coalesce(block);
}
