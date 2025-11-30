#include "kernel.h"
#include "ui/console.h"
#include "ui/framebuffer.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "arch/x86/interrupts.h"
#include "arch/x86/timer.h"
#include "arch/x86/pic.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/pci.h"
#include "drivers/virtio.h"
#include "drivers/virtio_console.h"
#include "drivers/ahci.h"
#include "drivers/speaker.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "mem/heap.h"
#include "mem/swap.h"
#include "sched/sched.h"
#include "sys/syscall.h"
#include "shell/shell.h"
#include "fs/fs.h"

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
    if (magic == 0x2BADB002) {
        framebuffer_init(mb_info);
        framebuffer_console_configure(80, 25);
    }

    console_init();
    if (magic != 0x2BADB002) {
        console_write("Invalid multiboot magic!\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    console_show_boot_splash(PENOS_VERSION);
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
    
    // Initialize PC speaker and play startup sound
    speaker_init();
    console_write("[Speaker] Playing startup melody (C-E-G)...\n");
    speaker_startup_sound();
    console_write("[Speaker] Startup sound complete!\n");
    
    // Initialize drivers
    keyboard_init();
    mouse_init();
    pci_init();
    
    // Initialize VirtIO Console early for logging
    virtio_console_init();
    
    ahci_init();
    swap_init();
    
    // Initialize other VirtIO drivers
    virtio_input_init();
    
    syscall_init();
    sched_init();
    

    // Initialize filesystem
    fs_init();

    console_write("Initialization complete. Enabling interrupts...\n");
    __asm__ volatile("sti");

    shell_run();

    console_write("Shell exited. Halting.\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
