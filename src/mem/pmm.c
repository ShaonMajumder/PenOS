#include "mem/pmm.h"
#include "ui/console.h"

static uint32_t total_frames = 0;
static uint32_t next_frame = 0;

void pmm_init(multiboot_info_t *mb_info)
{
    uint64_t mem_bytes = ((uint64_t)mb_info->mem_lower + (uint64_t)mb_info->mem_upper) * 1024ULL;
    total_frames = (uint32_t)(mem_bytes / 4096);
    next_frame = 0x100000 / 4096; // start after first MB
    console_write("PMM initialized. Frames: ");
    console_write_dec(total_frames);
    console_write("\n");
}

uint32_t pmm_alloc_frame(void)
{
    if (next_frame >= total_frames) {
        return 0;
    }
    return (next_frame++) * 4096;
}

void pmm_free_frame(uint32_t frame)
{
    (void)frame; // TODO: implement free list
}

uint32_t pmm_total_memory(void)
{
    return total_frames * 4096;
}
