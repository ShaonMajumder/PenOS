#include <mem/swap.h>
#include <drivers/ahci.h>
#include <ui/console.h>
#include <string.h>

#define SWAP_START_LBA  409600 // Start at 200MB
#define SWAP_SIZE_PAGES 4096   // 16MB swap space
#define SECTORS_PER_PAGE 8     // 4096 / 512 = 8

static uint8_t swap_bitmap[SWAP_SIZE_PAGES / 8];
static int swap_port = -1;

void swap_init(void) {
    for (int i = 0; i < 32; i++) {
        if (ahci_port_is_connected(i)) {
            swap_port = i;
            break;
        }
    }
    
    if (swap_port == -1) {
        console_write("Swap: No disk found, swap disabled.\n");
        return;
    }
    
    memset(swap_bitmap, 0, sizeof(swap_bitmap));
    console_write("Swap: Initialized on port ");
    console_write_dec(swap_port);
    console_write(" (16MB)\n");
}

int swap_available(void) {
    return swap_port != -1;
}

static int find_free_slot(void) {
    for (int i = 0; i < SWAP_SIZE_PAGES; i++) {
        if (!(swap_bitmap[i / 8] & (1 << (i % 8)))) {
            return i;
        }
    }
    return -1;
}

static void mark_slot(int slot, int used) {
    if (used) {
        swap_bitmap[slot / 8] |= (1 << (slot % 8));
    } else {
        swap_bitmap[slot / 8] &= ~(1 << (slot % 8));
    }
}

int swap_out(void *buffer, uint32_t *swap_slot) {
    if (swap_port == -1) return -1;
    
    int slot = find_free_slot();
    if (slot == -1) {
        console_write("Swap: No free slots!\n");
        return -1;
    }
    
    uint64_t lba = SWAP_START_LBA + (uint64_t)slot * SECTORS_PER_PAGE;
    if (ahci_write(swap_port, lba, SECTORS_PER_PAGE, buffer) != 0) {
        console_write("Swap: Write failed\n");
        return -1;
    }
    
    mark_slot(slot, 1);
    *swap_slot = (uint32_t)slot;
    return 0;
}

int swap_in(uint32_t swap_slot, void *buffer) {
    if (swap_port == -1) return -1;
    
    if (swap_slot >= SWAP_SIZE_PAGES) return -1;
    
    uint64_t lba = SWAP_START_LBA + (uint64_t)swap_slot * SECTORS_PER_PAGE;
    if (ahci_read(swap_port, lba, SECTORS_PER_PAGE, buffer) != 0) {
        console_write("Swap: Read failed\n");
        return -1;
    }
    
    return 0;
}

void swap_free(uint32_t swap_slot) {
    if (swap_slot < SWAP_SIZE_PAGES) {
        mark_slot(swap_slot, 0);
    }
}
