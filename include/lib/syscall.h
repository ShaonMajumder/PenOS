#ifndef LIB_SYSCALL_H
#define LIB_SYSCALL_H

#include <stdint.h>

void exit(void);
void write(const char *str);
uint32_t ticks(void);
void yield(void);
uint32_t getpid(void);

#endif
