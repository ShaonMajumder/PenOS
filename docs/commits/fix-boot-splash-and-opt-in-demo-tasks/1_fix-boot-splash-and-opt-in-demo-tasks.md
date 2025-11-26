fix/boot-splash-and-opt-in-demo-tasks

commit message : ui: add boot splash and make demo tasks opt-in

commit description :

The noisy [counter] tick N spam was coming from the demo worker threads in src/sched/sched.c (task_counter and task_spinner), which were being auto-spawned during sched_init(). Because the PIT is configured at 100 Hz, the scheduler immediately started running these tasks on every timer interrupt, flooding the console before the user even reached the shell prompt. This change removes the automatic spawning so that only the main kernel task exists after initialization, and the demo workers are started exclusively via spawn counter / spawn spinner from the shell. To improve the first impression, a branded ASCII splash inspired by penos-boot-splash.svg is rendered once at boot via console_show_boot_splash() (in src/ui/console.c), which is called from kernel_main() before the standard init banner and logs. The README and docs/architecture.md were updated to describe the new flow (splash → init log → quiet PenOS> prompt) and to highlight that demo tasks are now optional, user-driven proof of preemptive scheduling rather than constant background noise.

What is done in this commit

Scheduler:

Removed automatic spawning of counter and spinner tasks from sched_init() so only the main task exists after boot.

Left sched_spawn_named() unchanged so spawn counter / spawn spinner still create the same demo threads on demand.

UI / Console:

Added console_show_boot_splash() in src/ui/console.c and exported it via include/ui/console.h.

kernel_main() now calls console_show_boot_splash() right after console_init() to show a one-shot ASCII PenOS splash before the usual banner and init messages.

Docs:

Updated README.md to describe the new splash-first boot experience and clarify that demo tasks are opt-in via shell commands.

Updated docs/architecture.md to mention the ASCII splash in the bring-up sequence and note that the console is quiet by default until the user spawns demo threads.

Behavioral result:

After make iso and booting with QEMU, you now see: splash → init log → calm PenOS> prompt, with no [counter] spam until you explicitly spawn counter or spawn spinner.
