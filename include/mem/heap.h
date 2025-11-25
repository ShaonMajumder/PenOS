#ifndef MEM_HEAP_H
#define MEM_HEAP_H
#include <stddef.h>
#include <stdint.h>

void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t heap_bytes_in_use(void);
size_t heap_bytes_free(void);

#endif
