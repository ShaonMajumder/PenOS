#include "sched/sched.h"
#include "ui/console.h"

#define MAX_TASKS 4

static task_t tasks[MAX_TASKS];
static uint32_t task_count = 0;

void sched_init(void)
{
    task_count = 0;
    console_write("Scheduler ready (stub).\n");
}

void sched_add(void (*entry)(void), const char *name)
{
    (void)entry;
    if (task_count >= MAX_TASKS) {
        return;
    }
    task_t *t = &tasks[task_count];
    t->id = task_count;
    t->esp = 0;
    t->state = TASK_READY;
    for (int i = 0; i < 31 && name[i]; ++i) {
        t->name[i] = name[i];
        t->name[i+1] = '\0';
    }
    task_count++;
}

void sched_tick(void)
{
    // TODO: implement context switching
}

uint32_t sched_task_count(void)
{
    return task_count;
}
