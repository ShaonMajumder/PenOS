#include "kernel.h"
#include "ui/console.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "arch/x86/interrupts.h"
#include "arch/x86/timer.h"
#include "arch/x86/pic.h"
#include "drivers/keyboard.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "mem/heap.h"
#include "sched/sched.h"
#include "sys/syscall.h"
#include "shell/shell.h"
#include "apps/sysinfo.h"

static void banner(void)
{
    console_write("PenOS :: Minimal 32-bit kernel\n");
    console_write("--------------------------------\n");
}

void kernel_main(uint32_t magic, multiboot_info_t *mb_info)
{
    console_init();
    banner();

    if (magic != 0x2BADB002) {
        console_write("Invalid multiboot magic!\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    gdt_init();
    idt_init();
    pic_remap();
    interrupt_init();
    timer_init(100);
    pmm_init(mb_info);
    paging_init();
    heap_init();
    keyboard_init();
    syscall_init();
    sched_init();

    console_write("Initialization complete. Enabling interrupts...\n");
    __asm__ volatile ("sti");

    shell_run();

    console_write("Shell exited. Halting.\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
