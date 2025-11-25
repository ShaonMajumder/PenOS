#include <stdint.h>
#include "arch/x86/interrupts.h"
#include "arch/x86/idt.h"
#include "arch/x86/io.h"
#include "arch/x86/pic.h"
#include "ui/console.h"
#include "drivers/mouse.h"

static isr_t handlers[256];

#define DECL_ISR(n) extern void isr##n(void)
DECL_ISR(0);
DECL_ISR(1);
DECL_ISR(2);
DECL_ISR(3);
DECL_ISR(4);
DECL_ISR(5);
DECL_ISR(6);
DECL_ISR(7);
DECL_ISR(8);
DECL_ISR(9);
DECL_ISR(10);
DECL_ISR(11);
DECL_ISR(12);
DECL_ISR(13);
DECL_ISR(14);
DECL_ISR(15);
DECL_ISR(16);
DECL_ISR(17);
DECL_ISR(18);
DECL_ISR(19);
DECL_ISR(32);
DECL_ISR(33);
DECL_ISR(34);
DECL_ISR(35);
DECL_ISR(36);
DECL_ISR(37);
DECL_ISR(38);
DECL_ISR(39);
DECL_ISR(40);
DECL_ISR(41);
DECL_ISR(42);
DECL_ISR(43);
DECL_ISR(44);
DECL_ISR(45);
DECL_ISR(46);
DECL_ISR(47);
DECL_ISR(128);
#undef DECL_ISR

static void set_gate(uint8_t vec, void (*handler)(void))
{
    idt_set_entry(vec, handler, 0x08, 0x8E);
}

void interrupt_init(void)
{
    set_gate(0, isr0);
    set_gate(1, isr1);
    set_gate(2, isr2);
    set_gate(3, isr3);
    set_gate(4, isr4);
    set_gate(5, isr5);
    set_gate(6, isr6);
    set_gate(7, isr7);
    set_gate(8, isr8);
    set_gate(9, isr9);
    set_gate(10, isr10);
    set_gate(11, isr11);
    set_gate(12, isr12);
    set_gate(13, isr13);
    set_gate(14, isr14);
    set_gate(15, isr15);
    set_gate(16, isr16);
    set_gate(17, isr17);
    set_gate(18, isr18);
    set_gate(19, isr19);

    set_gate(32, isr32);
    set_gate(33, isr33);
    set_gate(34, isr34);
    set_gate(35, isr35);
    set_gate(36, isr36);
    set_gate(37, isr37);
    set_gate(38, isr38);
    set_gate(39, isr39);
    set_gate(40, isr40);
    set_gate(41, isr41);
    set_gate(42, isr42);
    set_gate(43, isr43);
    set_gate(44, isr44);
    set_gate(45, isr45);
    set_gate(46, isr46);
    set_gate(47, isr47);
    set_gate(128, isr128);

    mouse_init();
}

void register_interrupt_handler(uint8_t n, isr_t handler)
{
    handlers[n] = handler;
}

static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved"};

void isr_dispatch(interrupt_frame_t *frame)
{
    uint8_t int_no = frame->int_no;

    if (int_no < 20)
    {
        console_write("Exception: ");
        console_write(exception_messages[int_no]);
        console_write("\nSystem halted.\n");
        for (;;)
        {
            __asm__ volatile("cli; hlt");
        }
    }

    if (handlers[int_no])
    {
        handlers[int_no](frame);
    }

    if (int_no >= 32 && int_no <= 47)
    {
        pic_send_eoi(int_no - 32);
    }
}
