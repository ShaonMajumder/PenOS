# Commit 1 - Preemptive round-robin tasks
**Branch:** feature/scheduler-roundrobin  
**Commit:** "Enable preemptive kernel threads"  
**Summary:** Timer ISR snapshots the running thread, chooses the next ready task, and patches the interrupt frame to resume it on its own stack; demo counter/spinner threads prove the kernel now multitasks.

Problem
: The scheduler merely counted tasks and never switched context, so the PIT interrupt hook was idle and no concurrent workloads could run.

Solution
: Added per-task contexts built from `interrupt_frame_t`, allocated dedicated stacks via the kernel heap, and taught the timer interrupt to save/restore registers before `iret`. The scheduler now seeds two demo tasks to showcase preemption.

Architecture
```
timer IRQ -> save frame into current task
           -> round-robin pick next runnable task
           -> copy target context into ISR frame
           -> iret resumes selected task on its own stack
```

Data structures
- `task_entry_t`: holds metadata, saved frame, stack pointer, and entry callback.
- Global `tasks[]` array plus `current_task` pointer for round-robin traversal.

Tradeoffs
- Finished tasks simply park in a halted loop; future work should reclaim stacks and remove them from the run queue.
- Maximum of eight tasks keeps bookkeeping simple for now.

Interactions
- Kernel heap provides stacks; console/timer APIs power the demo worker threads.
- Timer interrupt now calls `sched_tick(frame)` so the ISR can swap contexts before returning.

Scientific difficulty
: Ensuring the interrupt frame patching restored the correct stack/register image without extra assembly demanded careful reasoning about the ISR prologue/epilogue.

What to learn
: You can build a simple yet real scheduler entirely in C by reusing the interrupt frame-no separate context-switch assembly is necessary when preemption is timer-driven.
