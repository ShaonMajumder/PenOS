#include "sys/power.h"
#include "arch/x86/io.h"

void power_shutdown(void)
{
    outw(0x604, 0x2000);  // QEMU/VirtualBox
    outw(0xB004, 0x2000); // Bochs
    outw(0x4004, 0x3400); // VirtualBox legacy

    __asm__ volatile ("cli");
    while (1) {
        __asm__ volatile ("hlt");
    }
}
