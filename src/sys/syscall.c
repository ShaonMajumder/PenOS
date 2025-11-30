#include "sys/syscall.h"
#include "ui/console.h"
#include "arch/x86/timer.h"
#include "arch/x86/interrupts.h"

#include "sys/syscall_nums.h"
#include "sched/sched.h"

typedef int32_t (*syscall_fn)(interrupt_frame_t *frame);

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

static int32_t sys_exit(interrupt_frame_t *frame)
{
    (void)frame;
    // Get current task ID to kill
    // We don't have a direct "get current" exposed easily here without including sched internals
    // But sched_kill(0) could imply "self" if we implemented it that way, 
    // or we can expose a "sched_exit()" function.
    // For now, let's assume sched_kill(current_id) is what we want.
    // But wait, sched.c tracks current_task.
    // Let's add sched_exit() to sched.h/c or just use sched_kill with a special ID?
    // Actually, sched_kill(0) returns -1.
    // Let's implement sched_exit() in sched.c or just call sched_kill(sched_get_pid()).
    
    // We need to know who called us.
    // sched.c has `current_task`.
    // Let's add a helper in sched.c: sched_get_current_pid() and sched_exit().
    
    // For now, let's just do:
    sched_kill(sched_get_current_pid());
    
    // We should not return from here if we killed ourselves.
    // The scheduler will switch away.
    // But we are in an interrupt handler.
    // When we return from this handler, we go back to `isr_common_stub` then `iret`.
    // If the task is ZOMBIE, sched_tick (called by timer) will clean it up?
    // Wait, syscall is int 0x80, not timer.
    // We need to trigger a schedule.
    // `sched_yield()` would be good.
    
    sched_yield();
    return 0;
}

static int32_t sys_yield(interrupt_frame_t *frame)
{
    (void)frame;
    sched_yield();
    return 0;
}

static int32_t sys_getpid(interrupt_frame_t *frame)
{
    (void)frame;
    return (int32_t)sched_get_current_pid();
}

static syscall_fn syscall_table[SYSCALL_MAX] = {
    [SYS_EXIT]   = sys_exit,
    [SYS_WRITE]  = sys_write,
    [SYS_TICKS]  = sys_ticks,
    [SYS_YIELD]  = sys_yield,
    [SYS_GETPID] = sys_getpid,
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
