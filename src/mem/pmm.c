#include "mem/pmm.h"
#include "multiboot.h"
#include "ui/console.h"
#include <stdint.h>
#include <stddef.h>

#define FRAME_SIZE      4096U
#define PMM_MAX_FRAMES  (1024U * 1024U) /* cover up to 4 GiB */
#define BITMAP_SIZE     (PMM_MAX_FRAMES / 8)

static uint32_t total_frames = 0;
static uint32_t free_frames = 0;
static uint32_t base_usable_frame = 0;
static uint32_t search_hint = 0;
static uint8_t frame_bitmap[BITMAP_SIZE];

extern uint8_t end; /* provided by linker */

static inline void set_frame(uint32_t frame)
{
    frame_bitmap[frame >> 3] |= (uint8_t)(1U << (frame & 7U));
}

static inline void clear_frame(uint32_t frame)
{
    frame_bitmap[frame >> 3] &= (uint8_t)~(1U << (frame & 7U));
}

static inline int test_frame(uint32_t frame)
{
    return frame_bitmap[frame >> 3] & (uint8_t)(1U << (frame & 7U));
}

static void bitmap_fill(uint8_t value)
{
    for (size_t i = 0; i < BITMAP_SIZE; ++i) {
        frame_bitmap[i] = value;
    }
}

static uint32_t align_frame_up(uint32_t addr)
{
    return (addr + FRAME_SIZE - 1U) / FRAME_SIZE;
}

static void release_region(uint64_t base, uint64_t length)
{
    if (length == 0 || total_frames == 0) {
        return;
    }

    uint64_t start = base / FRAME_SIZE;
    uint64_t end = (base + length + FRAME_SIZE - 1ULL) / FRAME_SIZE;
    if (start >= end) {
        return;
    }

    if (start >= total_frames) {
        return;
    }
    if (end > total_frames) {
        end = total_frames;
    }

    for (uint32_t frame = (uint32_t)start; frame < (uint32_t)end; ++frame) {
        if (frame < base_usable_frame) {
            continue; /* keep firmware/kernel/paging frames reserved */
        }
        if (!test_frame(frame)) {
            continue;
        }
        clear_frame(frame);
        ++free_frames;
    }
}

static uint64_t highest_address_from_mmap(multiboot_info_t *mb_info)
{
    if (!mb_info || !mb_info->mmap_length) {
        return 0;
    }

    uint64_t max_addr = 0;
    uint32_t offset = 0;
    while (offset < mb_info->mmap_length) {
        multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)((uintptr_t)mb_info->mmap_addr + offset);
        uint64_t entry_end = entry->addr + entry->len;
        if (entry_end > max_addr) {
            max_addr = entry_end;
        }
        offset += entry->size + sizeof(entry->size);
    }
    return max_addr;
}

static void process_available_regions(multiboot_info_t *mb_info)
{
    if (!mb_info || !mb_info->mmap_length) {
        release_region((uint64_t)base_usable_frame * FRAME_SIZE,
                       ((uint64_t)total_frames - base_usable_frame) * FRAME_SIZE);
        return;
    }

    uint32_t offset = 0;
    while (offset < mb_info->mmap_length) {
        multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)((uintptr_t)mb_info->mmap_addr + offset);
        if (entry->type == 1) {
            release_region(entry->addr, entry->len);
        }
        offset += entry->size + sizeof(entry->size);
    }
}

uint32_t pmm_alloc_frame(void)
{
    if (free_frames == 0 || total_frames == 0) {
        return 0;
    }

    uint32_t start = search_hint;
    for (int pass = 0; pass < 2; ++pass) {
        uint32_t begin = (pass == 0) ? start : base_usable_frame;
        uint32_t end = (pass == 0) ? total_frames : start;
        for (uint32_t frame = begin; frame < end; ++frame) {
            if (!test_frame(frame)) {
                set_frame(frame);
                --free_frames;
                search_hint = frame;
                return frame * FRAME_SIZE;
            }
        }
    }
    return 0;
}

void pmm_free_frame(uint32_t addr)
{
    if (addr == 0) {
        return;
    }
    uint32_t frame = addr / FRAME_SIZE;
    if (frame < base_usable_frame || frame >= total_frames) {
        return;
    }
    if (!test_frame(frame)) {
        return;
    }
    clear_frame(frame);
    ++free_frames;
    if (frame < search_hint) {
        search_hint = frame;
    }
}

uint32_t pmm_total_memory(void)
{
    return total_frames * FRAME_SIZE;
}

void pmm_init(multiboot_info_t *mb_info)
{
    uint32_t kernel_end_phys = (uint32_t)(uintptr_t)&end;
    uint32_t reserve_floor = align_frame_up(kernel_end_phys);
    uint32_t minimum_bootstrap = align_frame_up(0x00100000U); /* keep first MiB reserved */
    if (reserve_floor < minimum_bootstrap) {
        reserve_floor = minimum_bootstrap;
    }
    base_usable_frame = reserve_floor;
    search_hint = base_usable_frame;

    uint64_t max_addr = highest_address_from_mmap(mb_info);
    if (max_addr == 0) {
        if (mb_info) {
            max_addr = ((uint64_t)mb_info->mem_lower + (uint64_t)mb_info->mem_upper) * 1024ULL;
        }
    }
    total_frames = (uint32_t)(max_addr / FRAME_SIZE);
    if (total_frames > PMM_MAX_FRAMES) {
        total_frames = PMM_MAX_FRAMES;
    }
    if (total_frames < base_usable_frame) {
        total_frames = base_usable_frame;
    }

    bitmap_fill(0xFF); /* mark everything reserved */
    free_frames = 0;

    process_available_regions(mb_info);

    console_write("PMM initialized. Frames: ");
    console_write_dec(total_frames);
    console_write("\n");
}
