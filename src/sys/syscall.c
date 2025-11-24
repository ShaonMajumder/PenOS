#include "sys/syscall.h"
#include "ui/console.h"
#include "arch/x86/timer.h"
#include "arch/x86/interrupts.h"

typedef int32_t (*syscall_fn)(interrupt_frame_t *frame);

enum {
    SYSCALL_WRITE = 0,
    SYSCALL_TICKS = 1,
    SYSCALL_MAX
};

static int32_t sys_write(interrupt_frame_t *frame)
{
    const char *str = (const char *)frame->ebx;
    if (!str) {
        return -1;
    }
    console_write(str);
    return 0;
}

static int32_t sys_ticks(interrupt_frame_t *frame)
{
    (void)frame;
    return (int32_t)timer_ticks();
}

static syscall_fn syscall_table[SYSCALL_MAX] = {
    sys_write,
    sys_ticks
};

static void syscall_handler(interrupt_frame_t *frame)
{
    uint32_t num = frame->eax;
    int32_t ret = -1;
    if (num < SYSCALL_MAX && syscall_table[num]) {
        ret = syscall_table[num](frame);
    }
    frame->eax = (uint32_t)ret;
}

void syscall_init(void)
{
    register_interrupt_handler(128, syscall_handler);
}
