#ifndef KERNEL_H
#define KERNEL_H
#include "common.h"
#include "multiboot.h"

void kernel_main(uint32_t magic, multiboot_info_t *mb_info);

#endif
