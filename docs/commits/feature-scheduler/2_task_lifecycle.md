# Commit 2 - Task lifecycle + shell controls
**Branch:** feature/task-lifecycle  \
**Commit:** "Reap zombie threads and add shell task controls"  \
**Summary:** Scheduler now cleans up ZOMBIE tasks (freeing their stacks) and exposes ps/spawn/kill plumbing so the shell can inspect and manage kernel threads.

Problem
: Round-robin scheduling worked, but once a thread exited (or needed to be killed) its stack leaked forever, stayed on the run queue, and there was no interface to inspect or control tasks.

Solution
: Added task states (`TASK_UNUSED`, `TASK_READY`, `TASK_RUNNING`, `TASK_ZOMBIE`), per-slot bookkeeping, and a cleanup path that frees stacks once a thread hits ZOMBIE. The shell gained `ps`, `spawn <counter|spinner>`, and `kill <pid>` commands, all backed by new scheduler APIs to iterate tasks, spawn extra demo workers, and mark threads for destruction.

Architecture
```
Shell cmd --> sched_spawn_named / sched_kill
Scheduler state machine:
 RUNNING --return--> ZOMBIE --timer--> destroy_task() -> UNUSED
 RUNNING --timer--> READY --> next RUNNING task resumes via frame patch
```

Data structures
- Tasks remain in a fixed `MAX_TASKS` array, but each slot tracks stack pointer, saved frame, and current state. `sched_for_each` fills a tiny info struct so `ps` can render a table.

Tradeoffs
- Zombie reaping only occurs on timer ticks, which is fine for kernel threads but not millisecond-precise.
- Shell spawn is limited to built-in demo workers for now; future tasks will need argument passing or ELF loading.

Interactions
- `shell/shell.c` parses new commands and uses scheduler iterators/state names for display.
- `kmalloc/kfree` reuse ensures destroyed threads immediately return their stacks (and the heap's trimming logic can reclaim pages later).

Scientific difficulty
: Careful ordering was required so we never freed the current stack before switching contexts. The timer ISR now saves the running frame, marks it READY, reaps zombies, and only then swaps in the next runnable task.

What to learn
: Even simple cooperative shells benefit from basic task management. By keeping scheduler state explicit and exposing clean iteration/spawn/kill hooks, adding user-facing tooling becomes trivial.
