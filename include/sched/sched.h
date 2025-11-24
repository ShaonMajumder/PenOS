#ifndef SCHED_SCHED_H
#define SCHED_SCHED_H
#include <stdint.h>
#include "arch/x86/interrupts.h"

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_FINISHED
} task_state_t;

void sched_init(void);
void sched_spawn(void (*entry)(void), const char *name);
void sched_tick(interrupt_frame_t *frame);
uint32_t sched_task_count(void);

#endif
