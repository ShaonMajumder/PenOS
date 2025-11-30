#ifndef DRIVERS_BLOCK_H
#define DRIVERS_BLOCK_H

#include <stdint.h>
#include <stddef.h>

typedef struct block_device {
    char name[32];
    uint64_t sector_count;
    uint32_t sector_size;
    
    int (*read)(struct block_device *dev, uint64_t sector, uint32_t count, void *buffer);
    int (*write)(struct block_device *dev, uint64_t sector, uint32_t count, const void *buffer);
    
    void *driver_data; // Private driver data (e.g. port index)
} block_device_t;

#endif
