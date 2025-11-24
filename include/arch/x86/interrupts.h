#ifndef ARCH_X86_INTERRUPTS_H
#define ARCH_X86_INTERRUPTS_H
#include <stdint.h>

typedef struct interrupt_frame {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t gs, fs, es, ds;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} interrupt_frame_t;

typedef void (*isr_t)(interrupt_frame_t *frame);

void interrupt_init(void);
void register_interrupt_handler(uint8_t n, isr_t handler);
void isr_dispatch(interrupt_frame_t *frame);

#endif
