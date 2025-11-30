#include "arch/x86/tss.h"
#include "arch/x86/gdt.h"
#include <string.h>

static tss_entry_t tss_entry;

// Defined in gdt.c
extern void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

void tss_init(uint32_t idx, uint32_t ss0, uint32_t esp0)
{
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry) - 1;

    // Add the TSS descriptor to the GDT
    // Access: Present (0x80) | Ring 0 (0x00) | Executable (0x08) | Accessed (0x01) = 0x89
    // We use DPL 0 because we don't want Ring 3 to be able to switch tasks via TSS.
    gdt_set_gate(idx, base, limit, 0x89, 0x00);

    memset(&tss_entry, 0, sizeof(tss_entry));

    tss_entry.ss0 = ss0;
    tss_entry.esp0 = esp0;

    // Setting IOPL to 0 (kernel mode only I/O) or 3 (user mode I/O)
    // Here we set cs, ss, ds, es, fs, gs to kernel segments for now
    tss_entry.cs = 0x08 | 0x3; // Kernel code segment | RPL 3
    tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x10 | 0x3; // Kernel data segment | RPL 3
    
    // I/O Map Base Address
    // The I/O map base address field contains a 16-bit offset from the base of the TSS 
    // to the I/O permission bit map.
    // If it's greater than or equal to the TSS limit, there is no I/O permission map.
    tss_entry.iomap_base = sizeof(tss_entry);
}

void tss_set_stack(uint32_t esp0)
{
    tss_entry.esp0 = esp0;
}

void tss_flush(void)
{
    // Load the Task Register (TR)
    // Index 5 = 0x28. RPL 0.
    __asm__ volatile("ltr %%ax" : : "a" (0x28));
}
