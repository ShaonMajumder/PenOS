#include <sched/sched.h>
#include <ui/console.h>
#include <mem/heap.h>
#include <mem/paging.h>
#include <mem/pmm.h>
#include <fs/elf.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#ifdef SCHED_DEBUG
#define SCHED_LOG(msg) console_write(msg)
#else
#define SCHED_LOG(msg) ((void)0)
#endif

#define MAX_TASKS   8
#define STACK_SIZE  4096

typedef struct task_entry {
    uint32_t id;
    char name[32];
    task_state_t state;
    interrupt_frame_t *frame;
    void (*entry)(void);
    uint8_t *stack;
    uint32_t kernel_stack; // ESP0 for TSS
    uint32_t page_directory_phys; // Physical address of page directory
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

// Defined in tss.c
#include "arch/x86/tss.h"

void sched_init(void)
{
    memset(tasks, 0, sizeof(tasks));
    current_task = &tasks[0];
    current_task->id = 0;
    current_task->state = TASK_RUNNING;
    current_task->frame = NULL;
    current_task->entry = NULL;
    current_task->stack = NULL;
    current_task->page_directory_phys = paging_get_kernel_directory();
    strncpy(current_task->name, "main", sizeof(current_task->name) - 1);
    active_tasks = 1;
    current_index = 0;
    SCHED_LOG("Scheduler initialized.\n");
}

static int32_t spawn_task(void (*entry)(void), const char *name)
{
    int slot = find_free_slot();
    if (slot < 0) {
        SCHED_LOG("Scheduler: max tasks reached\n");
        return -1;
    }

    uint8_t *stack = (uint8_t *)kmalloc(STACK_SIZE);
    if (!stack) {
        SCHED_LOG("Scheduler: stack allocation failed\n");
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

    /* Set up an initial interrupt frame that will jump into task_trampoline */
    frame->gs = frame->fs = frame->es = frame->ds = 0x10;
    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = stack_top;
    frame->esp = stack_top;
    frame->ebx = 0;
    frame->edx = 0;
    frame->ecx = 0;
    frame->eax = 0;
    frame->int_no = 0;
    frame->err_code = 0;
    frame->eip = (uint32_t)task_trampoline;
    frame->cs = 0x08;
    frame->eflags = 0x202; /* IF=1 */

    task->frame = frame;
    task->kernel_stack = stack_top; // For kernel tasks, ESP0 is the same as initial stack

    active_tasks++;
    return (int32_t)task->id;
}

int32_t sched_spawn_user(void (*entry)(void), const char *name)
{
    int slot = find_free_slot();
    if (slot < 0) return -1;

    // 1. Create new page directory for this process
    uint32_t new_pd_phys = paging_create_directory();
    if (!new_pd_phys) return -1;

    // 2. Allocate Kernel Stack (for syscalls/interrupts)
    uint8_t *kstack = (uint8_t *)kmalloc(STACK_SIZE);
    if (!kstack) {
        paging_destroy_directory(new_pd_phys);
        return -1;
    }
    uint32_t kstack_top = ((uint32_t)kstack + STACK_SIZE) & ~0xF;

    // 3. Allocate User Stack
    uint8_t *ustack = (uint8_t *)kmalloc(STACK_SIZE);
    if (!ustack) {
        kfree(kstack);
        paging_destroy_directory(new_pd_phys);
        return -1;
    }
    
    // 4. Temporarily switch to new page directory to map user memory
    uint32_t old_pd = paging_get_kernel_directory();
    paging_switch_directory(new_pd_phys);
    
    // Remap user stack pages as User-accessible (Ring 3)
    uint32_t ustack_phys = paging_virt_to_phys((uint32_t)ustack);
    if (ustack_phys) {
        paging_map((uint32_t)ustack, ustack_phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }
    
    // Also map the entry point code as User-accessible
    uint32_t entry_phys = paging_virt_to_phys((uint32_t)entry);
    if (entry_phys) {
        // Map the page containing the entry point
        paging_map((uint32_t)entry & 0xFFFFF000, entry_phys & 0xFFFFF000, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }
    
    // Switch back to kernel page directory
    paging_switch_directory(old_pd);

    uint32_t ustack_top = ((uint32_t)ustack + STACK_SIZE) & ~0xF;

    task_entry_t *task = &tasks[slot];
    memset(task, 0, sizeof(*task));
    task->id = next_task_id++;
    task->state = TASK_READY;
    task->entry = entry;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->stack = kstack; // We track kernel stack for cleanup
    task->kernel_stack = kstack_top; // ESP0
    task->page_directory_phys = new_pd_phys; // Store page directory

    // Set up interrupt frame on KERNEL stack
    interrupt_frame_t *frame = (interrupt_frame_t *)(kstack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(*frame));

    frame->gs = frame->fs = frame->es = frame->ds = 0x23; // User Data (0x20 | 3)
    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = ustack_top;
    frame->user_esp = ustack_top; // User ESP
    frame->ebx = 0;
    frame->edx = 0;
    frame->ecx = 0;
    frame->eax = 0;
    frame->int_no = 0;
    frame->err_code = 0;
    frame->eip = (uint32_t)entry;
    frame->cs = 0x1B; // User Code (0x18 | 3)
    frame->eflags = 0x202; /* IF=1 */
    frame->ss = 0x23; // User Data (0x20 | 3) - Stack Segment

    task->frame = frame;

    active_tasks++;
    return (int32_t)task->id;
}

int32_t sched_spawn_elf(const char *path)
{
    int slot = find_free_slot();
    if (slot < 0) return -1;

    // 1. Create new page directory
    uint32_t new_pd_phys = paging_create_directory();
    if (!new_pd_phys) return -1;

    // 2. Allocate Kernel Stack
    uint8_t *kstack = (uint8_t *)kmalloc(STACK_SIZE);
    if (!kstack) {
        paging_destroy_directory(new_pd_phys);
        return -1;
    }
    uint32_t kstack_top = ((uint32_t)kstack + STACK_SIZE) & ~0xF;

    // 3. Switch to new PD to load ELF
    uint32_t old_pd = paging_get_kernel_directory();
    paging_switch_directory(new_pd_phys);
    
    // 4. Load ELF
    uint32_t entry_point = 0;
    uint32_t result = elf_load(path, &entry_point);
    
    if (result == 0) {
        // Failed to load
        paging_switch_directory(old_pd);
        kfree(kstack);
        paging_destroy_directory(new_pd_phys);
        return -1;
    }
    
    // 5. Allocate User Stack
    // Map 16KB of stack at 0xBFFFF000
    uint32_t ustack_top = 0xC0000000 - 0x1000;
    for (int i = 0; i < 4; i++) {
        uint32_t page_vaddr = ustack_top - (i * 0x1000);
        uint32_t phys = pmm_alloc_frame();
        paging_map(page_vaddr, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }
    
    // Switch back
    paging_switch_directory(old_pd);

    // 6. Setup Task
    task_entry_t *task = &tasks[slot];
    memset(task, 0, sizeof(*task));
    task->id = next_task_id++;
    task->state = TASK_READY;
    task->entry = (void (*)(void))entry_point;
    strncpy(task->name, path, sizeof(task->name) - 1);
    task->stack = kstack;
    task->kernel_stack = kstack_top;
    task->page_directory_phys = new_pd_phys;

    // 7. Setup Interrupt Frame
    interrupt_frame_t *frame = (interrupt_frame_t *)(kstack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(*frame));

    frame->gs = frame->fs = frame->es = frame->ds = 0x23; // User Data
    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = 0;
    frame->user_esp = ustack_top + 0x1000 - 16; // Top of stack
    frame->ebx = 0;
    frame->edx = 0;
    frame->ecx = 0;
    frame->eax = 0;
    frame->int_no = 0;
    frame->err_code = 0;
    frame->eip = entry_point;
    frame->cs = 0x1B; // User Code
    frame->eflags = 0x202; // IF=1
    frame->ss = 0x23; // User Data

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

void sched_yield(void)
{
    // Trigger a schedule by calling the interrupt handler?
    // Or just wait for the next timer tick?
    // Ideally we want to give up the rest of our timeslice.
    // We can force a task switch if we were in kernel mode, but we are likely called from syscall handler.
    // The syscall handler returns to `isr_common_stub` which does `iret`.
    // If we want to switch, we need to call `sched_tick` manually?
    // But `sched_tick` expects an interrupt frame.
    
    // For now, a simple yield implementation is to just wait for the next tick.
    // But that's not really yielding.
    // A better way is to set a flag "yield_requested" and check it in the timer handler?
    // Or, since we are in a syscall (interrupt), we can just call the scheduler?
    // But `sched_tick` takes `frame` from the stack.
    
    // Let's just use `__asm__ volatile ("int $0x20")` to force a timer interrupt?
    // Or better, just wait.
    // Actually, for this simple OS, `hlt` until next interrupt is fine for now.
    __asm__ volatile ("hlt");
}

uint32_t sched_get_current_pid(void)
{
    if (current_task) {
        return current_task->id;
    }
    return 0;
}

/*
 * Main scheduler entry point called from the timer interrupt.
 * It returns the interrupt_frame_t* that the CPU should resume with.
 */
interrupt_frame_t *sched_tick(interrupt_frame_t *frame)
{
    if (!current_task) {
        return frame;
    }

    /* Fast path: avoid scheduler overhead when only one task is active */
    if (active_tasks <= 1) {
        if (current_task->state == TASK_ZOMBIE) {
            destroy_task(current_task);
            current_task = NULL;
            return frame;
        }
        current_task->frame = frame;
        current_task->state = TASK_RUNNING;
        return frame;
    }

    /* Save state of the currently running task and mark it ready */
    if (current_task->state == TASK_RUNNING) {
        current_task->frame = frame;
        current_task->state = TASK_READY;
    }

    if (current_task->state == TASK_ZOMBIE) {
        destroy_task(current_task);
        current_task = NULL;
    }

    /* Clean up any completed tasks */
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

    // Update TSS ESP0 for the new task
    tss_set_stack(current_task->kernel_stack);

    // Switch to the new task's page directory
    if (current_task->page_directory_phys) {
        paging_switch_directory(current_task->page_directory_phys);
    }

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
        sched_task_info_t info;
        info.id = tasks[i].id;
        info.state = tasks[i].state;
        memset(info.name, 0, sizeof(info.name));
        strncpy(info.name, tasks[i].name, sizeof(info.name) - 1);
        cb(&info);
    }
}

const char *sched_state_name(task_state_t state)
{
    switch (state) {
    case TASK_READY:    return "READY";
    case TASK_RUNNING:  return "RUNNING";
    case TASK_SLEEPING: return "SLEEP";
    case TASK_ZOMBIE:   return "ZOMBIE";
    case TASK_UNUSED:   return "UNUSED";
    default:            return "??";
    }
}

/* --- internal helpers ---------------------------------------------------- */

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
    // Free page directory if it's not the kernel directory
    if (task->page_directory_phys && task->page_directory_phys != paging_get_kernel_directory()) {
        paging_destroy_directory(task->page_directory_phys);
        task->page_directory_phys = 0;
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

/* --- task trampoline and demo tasks -------------------------------------- */

static void task_trampoline(void)
{
    __asm__ volatile ("sti");
    if (current_task && current_task->entry) {
        current_task->entry();
    }
    current_task->state = TASK_ZOMBIE;
    SCHED_LOG("[sched] task finished\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void task_counter(void)
{
    uint32_t counter = 0;
    while (1) {
        SCHED_LOG("[counter] tick ");
        console_write_dec(counter++);
        SCHED_LOG("\n");
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
        SCHED_LOG("[spinner] ");
        char buf[2] = { glyphs[idx++ % 4], '\0' };
        SCHED_LOG(buf);
        SCHED_LOG("\r");
        for (volatile uint32_t i = 0; i < 500000; ++i) {
            __asm__ volatile ("nop");
        }
    }
}
