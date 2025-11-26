#ifndef ARCH_X86_TIMER_H
#define ARCH_X86_TIMER_H
#include <stdint.h>

void timer_init(uint32_t frequency);
uint64_t timer_ticks(void);

#endif
