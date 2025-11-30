#include <stdint.h>
#include "arch/x86/gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#include "arch/x86/tss.h"

static struct gdt_entry gdt[6];
static struct gdt_ptr gdtp;

extern void gdt_flush(uint32_t);

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

void gdt_init(void)
{
    gdtp.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gdtp.base = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code (0x08)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data (0x10)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code (0x18) - Present, Ring 3, Exec/Read
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data (0x20) - Present, Ring 3, Read/Write
    
    // TSS Segment (0x28)
    // Base and limit will be set by tss_init
    // We pass index 5, kernel SS (0x10), and initial kernel stack (0)
    tss_init(5, 0x10, 0);

    gdt_flush((uint32_t)&gdtp);
    tss_flush();
}
