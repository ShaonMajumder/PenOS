#include "apps/sysinfo.h"
#include "ui/console.h"
#include "mem/pmm.h"
#include "arch/x86/timer.h"
#include "sched/sched.h"

void app_sysinfo(void)
{
    console_write("-- PenOS System Info --\n");
    console_write("Total memory: ");
    console_write_dec(pmm_total_memory() / 1024);
    console_write(" KB\nTicks: ");
    console_write_dec((uint32_t)timer_ticks());
    console_write("\nTasks: ");
    console_write_dec(sched_task_count());
    console_putc('\n');
}
