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
    interrupt_frame_t *frame;
    void (*entry)(void);
    uint8_t *stack;
} task_entry_t;

static task_entry_t tasks[MAX_TASKS];
static task_entry_t *current_task = NULL;
static uint32_t next_task_id = 1;
static uint32_t active_tasks = 0;
static uint32_t current_index = 0;

static void task_trampoline(void);
static void task_counter(void);
static void task_spinner(void);

static int find_free_slot(void);
static task_entry_t *find_task_by_id(uint32_t id);
static void destroy_task(task_entry_t *task);
static void reap_zombies(void);
static task_entry_t *pick_next_task(void);

void sched_init(void)
{
    memset(tasks, 0, sizeof(tasks));
    current_task = &tasks[0];
    current_task->id = 0;
    current_task->state = TASK_RUNNING;
    current_task->frame = NULL;
    current_task->stack = NULL;
    strncpy(current_task->name, "main", sizeof(current_task->name) - 1);
    active_tasks = 1;
    current_index = 0;
    console_write("Scheduler initialized.\n");

    sched_spawn_named("counter");
    sched_spawn_named("spinner");
}

static int32_t spawn_task(void (*entry)(void), const char *name)
{
    int slot = find_free_slot();
    if (slot < 0) {
        console_write("Scheduler: max tasks reached\n");
        return -1;
    }
    uint8_t *stack = kmalloc(STACK_SIZE);
    if (!stack) {
        console_write("Scheduler: stack allocation failed\n");
        return -1;
    }
    task_entry_t *task = &tasks[slot];
    memset(task, 0, sizeof(*task));
    task->id = next_task_id++;
    task->state = TASK_READY;
    task->entry = entry;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->stack = stack;

    uint32_t stack_top = ((uint32_t)stack + STACK_SIZE) & ~0xF;
    interrupt_frame_t *frame = (interrupt_frame_t *)(stack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(*frame));
    frame->gs = frame->fs = frame->es = frame->ds = 0x10;
    frame->edi = frame->esi = 0;
    frame->ebp = stack_top;
    frame->esp = stack_top;
    frame->ebx = frame->edx = frame->ecx = frame->eax = 0;
    frame->int_no = 0;
    frame->err_code = 0;
    frame->eip = (uint32_t)task_trampoline;
    frame->cs = 0x08;
    frame->eflags = 0x202;
    task->frame = frame;

    active_tasks++;
    return (int32_t)task->id;
}

int32_t sched_spawn_named(const char *name)
{
    if (!name) {
        return -1;
    }
    if (!strcmp(name, "spinner")) {
        return spawn_task(task_spinner, "spinner");
    }
    if (!strcmp(name, "counter")) {
        return spawn_task(task_counter, "counter");
    }
    return -1;
}

int sched_kill(uint32_t id)
{
    if (id == 0) {
        return -1;
    }
    task_entry_t *task = find_task_by_id(id);
    if (!task || task->state == TASK_UNUSED) {
        return -1;
    }
    if (task == current_task) {
        task->state = TASK_ZOMBIE;
        return 0;
    }
    if (task->state == TASK_ZOMBIE) {
        return 0;
    }
    destroy_task(task);
    return 0;
}

interrupt_frame_t *sched_tick(interrupt_frame_t *frame)
{
    if (!current_task) {
        return frame;
    }

    if (current_task->state == TASK_RUNNING) {
        current_task->frame = frame;
        current_task->state = TASK_READY;
    }

    if (current_task->state == TASK_ZOMBIE) {
        destroy_task(current_task);
        current_task = NULL;
    }

    reap_zombies();

    task_entry_t *next = pick_next_task();
    if (!next) {
        if (!current_task) {
            current_task = &tasks[0];
        }
        if (current_task->state != TASK_RUNNING) {
            current_task->state = TASK_RUNNING;
        }
        if (!current_task->frame) {
            current_task->frame = frame;
        }
        return frame;
    }

    current_task = next;
    current_task->state = TASK_RUNNING;
    if (!current_task->frame) {
        current_task->frame = frame;
        return frame;
    }
    return current_task->frame;
}

uint32_t sched_task_count(void)
{
    return active_tasks;
}

void sched_for_each(sched_iter_cb cb)
{
    if (!cb) {
        return;
    }
    for (uint32_t i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state == TASK_UNUSED) {
            continue;
        }
        sched_task_info_t info = {
            .id = tasks[i].id,
            .state = tasks[i].state
        };
        strncpy(info.name, tasks[i].name, sizeof(info.name) - 1);
        cb(&info);
    }
}

const char *sched_state_name(task_state_t state)
{
    switch (state) {
    case TASK_READY:   return "READY";
    case TASK_RUNNING: return "RUNNING";
    case TASK_SLEEPING:return "SLEEP";
    case TASK_ZOMBIE:  return "ZOMBIE";
    case TASK_UNUSED:  return "UNUSED";
    default:           return "??";
    }
}

static int find_free_slot(void)
{
    for (uint32_t i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state == TASK_UNUSED) {
            return (int)i;
        }
    }
    return -1;
}

static task_entry_t *find_task_by_id(uint32_t id)
{
    for (uint32_t i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state != TASK_UNUSED && tasks[i].id == id) {
            return &tasks[i];
        }
    }
    return NULL;
}

static void destroy_task(task_entry_t *task)
{
    if (!task || task->state == TASK_UNUSED) {
        return;
    }
    if (task->stack) {
        kfree(task->stack);
        task->stack = NULL;
    }
    memset(task->name, 0, sizeof(task->name));
    task->state = TASK_UNUSED;
    task->entry = NULL;
    task->frame = NULL;
    if (active_tasks) {
        --active_tasks;
    }
}

static void reap_zombies(void)
{
    for (uint32_t i = 0; i < MAX_TASKS; ++i) {
        task_entry_t *task = &tasks[i];
        if (task == current_task) {
            continue;
        }
        if (task->state == TASK_ZOMBIE) {
            destroy_task(task);
        }
    }
}

static task_entry_t *pick_next_task(void)
{
    for (uint32_t i = 0; i < MAX_TASKS; ++i) {
        current_index = (current_index + 1) % MAX_TASKS;
        task_entry_t *candidate = &tasks[current_index];
        if (candidate->state == TASK_READY) {
            return candidate;
        }
    }
    if (current_task && current_task->state == TASK_READY) {
        return current_task;
    }
    return NULL;
}

static void task_trampoline(void)
{
    __asm__ volatile ("sti");
    if (current_task && current_task->entry) {
        current_task->entry();
    }
    current_task->state = TASK_ZOMBIE;
    console_write("[sched] task finished\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void task_counter(void)
{
    uint32_t counter = 0;
    while (1) {
        console_write("[counter] tick ");
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
