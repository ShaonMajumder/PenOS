#include <string.h>
#include "arch/x86/idt.h"

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idtp;

extern void (*isr_stub_table[])(void);

void idt_set_entry(uint8_t vec, void (*handler)(void), uint16_t selector, uint8_t flags)
{
    uint32_t addr = (uint32_t)handler;
    idt[vec].offset_low = addr & 0xFFFF;
    idt[vec].selector = selector;
    idt[vec].zero = 0;
    idt[vec].type_attr = flags;
    idt[vec].offset_high = (addr >> 16) & 0xFFFF;
}

void idt_init(void)
{
    idtp.base = (uint32_t)&idt;
    idtp.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    memset(idt, 0, sizeof(idt));
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}
