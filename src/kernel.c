#include "kernel.h"
#include "ui/console.h"
#include "ui/fb.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "arch/x86/interrupts.h"
#include "arch/x86/timer.h"
#include "arch/x86/pic.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "mem/heap.h"
#include "sched/sched.h"
#include "sys/syscall.h"
#include "shell/shell.h"
#include "apps/sysinfo.h"
#include "ui/desktop.h"

#ifndef PENOS_VERSION
#define PENOS_VERSION "dev"
#endif

static void banner(void)
{
    console_write("PenOS :: Minimal 32-bit kernel\n");
    console_write("--------------------------------\n");
}

void kernel_main(uint32_t magic, multiboot_info_t *mb_info)
{
    if (magic == 0x2BADB002)
    {
        fb_init(mb_info);
    }

    console_init();
    if (magic != 0x2BADB002)
    {
        console_write("Invalid multiboot magic!\n");
        for (;;)
        {
            __asm__ volatile("hlt");
        }
    }

    console_show_boot_splash(PENOS_VERSION);
    banner();

    gdt_init();
    idt_init();
    pic_remap();
    interrupt_init();
    timer_init(100);
    pmm_init(mb_info);
    paging_init();
    heap_init();
    keyboard_init();
    mouse_init();
    syscall_init();
    sched_init();

    console_write("Initialization complete. Enabling interrupts...\n");
    __asm__ volatile("sti");

    if (fb_present() && desktop_autostart_enabled())
    {
        desktop_start();
    }

    shell_run();

    console_write("Shell exited. Halting.\n");
    for (;;)
    {
        __asm__ volatile("hlt");
    }
}
