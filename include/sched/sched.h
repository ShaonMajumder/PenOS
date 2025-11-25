#ifndef SCHED_SCHED_H
#define SCHED_SCHED_H
#include <stdint.h>
#include "arch/x86/interrupts.h"

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_ZOMBIE
} task_state_t;

typedef struct {
    uint32_t id;
    task_state_t state;
    char name[32];
} sched_task_info_t;

typedef void (*sched_iter_cb)(const sched_task_info_t *info);

void sched_init(void);
int32_t sched_spawn_named(const char *name);
int sched_kill(uint32_t id);
void sched_tick(interrupt_frame_t *frame);
uint32_t sched_task_count(void);
void sched_for_each(sched_iter_cb cb);
const char *sched_state_name(task_state_t state);

#endif
