#ifndef SCHED_SCHED_H
#define SCHED_SCHED_H
#include <stdint.h>

typedef enum { TASK_READY, TASK_RUNNING, TASK_SLEEPING } task_state_t;

typedef struct task {
    uint32_t id;
    uint32_t esp;
    task_state_t state;
    char name[32];
} task_t;

void sched_init(void);
void sched_add(void (*entry)(void), const char *name);
void sched_tick(void);
uint32_t sched_task_count(void);

#endif
