#include "arch/x86/timer.h"
#include "arch/x86/io.h"
#include "arch/x86/interrupts.h"
#include "sched/sched.h"

static uint64_t ticks = 0;

static void timer_callback(interrupt_frame_t *frame)
{
    ticks++;
    sched_tick(frame);
}

void timer_init(uint32_t frequency)
{
    uint32_t divisor = 1193180 / frequency;
    register_interrupt_handler(32, timer_callback);

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint64_t timer_ticks(void)
{
    return ticks;
}
