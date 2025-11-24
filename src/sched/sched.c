#include "sched/sched.h"
#include "mem/heap.h"
#include "ui/console.h"
#include <string.h>

#define MAX_TASKS   8
#define STACK_SIZE  4096

typedef struct task_entry {
    uint32_t id;
    char name[32];
    task_state_t state;
    interrupt_frame_t context;
    void (*entry)(void);
    uint8_t *stack;
} task_entry_t;

static task_entry_t tasks[MAX_TASKS];
static task_entry_t *current_task = NULL;
static uint32_t total_tasks = 0;
static uint32_t next_task_id = 1;
static uint32_t current_index = 0;

static void task_trampoline(void);
static void task_counter(void);
static void task_spinner(void);

static task_entry_t *pick_next_task(void)
{
    if (total_tasks == 0) {
        return NULL;
    }
    for (uint32_t i = 0; i < total_tasks; ++i) {
        current_index = (current_index + 1) % total_tasks;
        task_entry_t *candidate = &tasks[current_index];
        if (candidate->state == TASK_READY || candidate->state == TASK_RUNNING) {
            return candidate;
        }
    }
    return current_task;
}

void sched_init(void)
{
    memset(tasks, 0, sizeof(tasks));
    current_task = &tasks[0];
    current_task->id = 0;
    current_task->state = TASK_RUNNING;
    strncpy(current_task->name, "main", sizeof(current_task->name) - 1);
    total_tasks = 1;
    current_index = 0;
    console_write("Scheduler initialized.\n");

    sched_spawn(task_counter, "counter");
    sched_spawn(task_spinner, "spinner");
}

void sched_spawn(void (*entry)(void), const char *name)
{
    if (total_tasks >= MAX_TASKS) {
        console_write("Scheduler: max tasks reached\n");
        return;
    }
    uint8_t *stack = kmalloc(STACK_SIZE);
    if (!stack) {
        console_write("Scheduler: stack allocation failed\n");
        return;
    }
    task_entry_t *task = &tasks[total_tasks++];
    memset(task, 0, sizeof(*task));
    task->id = next_task_id++;
    task->state = TASK_READY;
    task->entry = entry;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->stack = stack;

    uint32_t top = (uint32_t)stack + STACK_SIZE;
    interrupt_frame_t *ctx = &task->context;
    memset(ctx, 0, sizeof(*ctx));
    ctx->eip = (uint32_t)task_trampoline;
    ctx->cs = 0x08;
    ctx->ds = ctx->es = ctx->fs = ctx->gs = 0x10;
    ctx->ss = 0x10;
    ctx->eflags = 0x202;
    ctx->esp = top;
    ctx->ebp = top;
    ctx->useresp = top;
}

void sched_tick(interrupt_frame_t *frame)
{
    if (!current_task) {
        return;
    }
    current_task->context = *frame;

    task_entry_t *next = pick_next_task();
    if (!next || next == current_task) {
        return;
    }

    if (current_task->state == TASK_RUNNING) {
        current_task->state = TASK_READY;
    }
    current_task = next;
    current_task->state = TASK_RUNNING;
    *frame = current_task->context;
}

uint32_t sched_task_count(void)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < total_tasks; ++i) {
        if (tasks[i].state != TASK_UNUSED) {
            ++count;
        }
    }
    return count;
}

static void task_trampoline(void)
{
    __asm__ volatile ("sti");
    if (current_task && current_task->entry) {
        current_task->entry();
    }
    console_write("[sched] task finished\n");
    current_task->state = TASK_FINISHED;
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void task_counter(void)
{
    uint32_t counter = 0;
    while (1) {
        console_write("[task counter] tick ");
        console_write_dec(counter++);
        console_write("\n");
        for (volatile uint32_t i = 0; i < 500000; ++i) {
            __asm__ volatile ("nop");
        }
    }
}

static void task_spinner(void)
{
    const char glyphs[] = "|/-\\";
    uint32_t idx = 0;
    while (1) {
        console_write("[spinner] ");
        char buf[2] = { glyphs[idx++ % 4], '\0' };
        console_write(buf);
        console_write("\r");
        for (volatile uint32_t i = 0; i < 500000; ++i) {
            __asm__ volatile ("nop");
        }
    }
}
