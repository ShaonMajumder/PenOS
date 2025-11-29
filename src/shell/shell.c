#include "shell/shell.h"
#include "drivers/keyboard.h"
#include "ui/console.h"
#include "arch/x86/timer.h"
#include "mem/pmm.h"
#include "apps/sysinfo.h"
#include "sched/sched.h"
#include "sys/power.h"
#include <string.h>

typedef struct
{
    uint32_t *ids;
    size_t capacity;
    size_t count;
} demo_kill_ctx_t;

static demo_kill_ctx_t *demo_ctx = NULL;

static void collect_demo_tasks(const sched_task_info_t *info)
{
    if (!demo_ctx || !info)
    {
        return;
    }
    if (!strcmp(info->name, "spinner") || !strcmp(info->name, "counter"))
    {
        if (demo_ctx->count < demo_ctx->capacity)
        {
            demo_ctx->ids[demo_ctx->count++] = info->id;
        }
    }
}

static size_t shell_stop_demo_tasks(void)
{
    uint32_t ids[8];
    demo_kill_ctx_t ctx = {
        .ids = ids,
        .capacity = sizeof(ids) / sizeof(ids[0]),
        .count = 0};
    demo_ctx = &ctx;
    sched_for_each(collect_demo_tasks);
    demo_ctx = NULL;
    for (size_t i = 0; i < ctx.count; ++i)
    {
        sched_kill(ctx.ids[i]);
    }
    return ctx.count;
}

static void shell_handle_break(const char *label)
{
    if (label && *label)
    {
        console_write(label);
    }
    console_putc('\n');
    size_t stopped = shell_stop_demo_tasks();
    if (stopped > 0)
    {
        console_write("[shell] stopped ");
        console_write_dec((uint32_t)stopped);
        console_write(" demo task(s)\n");
    }
    else
    {
        console_write("[shell] no demo tasks to stop\n");
    }
}

static int getline_block(char *buffer, int max)
{
    int len = 0;
    while (len < max - 1)
    {
        int c;
        while ((c = keyboard_read_char()) == -1)
        {
            /* busy-wait for keypress */
        }
        char ch = (char)c;
        if (ch == '\r' || ch == '\n')
        {
            console_putc('\n');
            break;
        }
        else if (ch == 8 || ch == 127)
        { /* backspace (8 for some layouts, 127 for others) */
            if (len > 0)
            {
                len--;
                console_write("\b \b");
            }
        }
        else if (ch == 27)
        { /* ESC */
            buffer[0] = '\0';
            shell_handle_break("[ESC]");
            return -1;
        }
        else if (ch == 3)
        { /* Ctrl+C */
            buffer[0] = '\0';
            shell_handle_break("^C");
            return -1;
        }
        else
        {
            buffer[len++] = ch;
            console_putc(ch);
        }
    }
    buffer[len] = '\0';
    return len;
}

/* ---------- Commands ---------- */

static void cmd_help(void)
{
    console_write("\nPenOS shell - available commands:\n");
    console_write("  help              Show this help message\n");
    console_write("  clear             Clear the screen\n");
    console_write("  echo <text>       Print <text> back to you\n");
    console_write("  ticks             Show system tick counter\n");
    console_write("  sysinfo           Show simple system information\n");
    console_write("  ps                List running tasks\n");
    console_write("  spawn <name>      Start a demo task (counter|spinner)\n");
    console_write("  kill <pid>        Stop a task by PID\n");
    console_write("  halt              Exit the shell (CPU will halt)\n");
    console_write("  shutdown          Try to power off the machine\n");
#ifdef FS_FS_H
    console_write("  ls                List files in the in-memory filesystem\n");
    console_write("  cat <file>        Show contents of a file\n");
#endif
    console_putc('\n');
    console_write("Examples:\n");
    console_write("  echo hello world\n");
    console_write("  spawn counter\n");
    console_write("  ps\n");
    console_write("  kill 3\n");
#ifdef FS_FS_H
    console_write("  ls\n");
    console_write("  cat hello.txt\n");
#endif
    console_putc('\n');
}

static void cmd_echo(const char *args)
{
    console_write(args);
    console_putc('\n');
}

static void cmd_ticks(void)
{
    console_write("Ticks: ");
    console_write_dec((uint32_t)timer_ticks());
    console_putc('\n');
}

static void ps_callback(const sched_task_info_t *info)
{
    console_write_dec(info->id);
    if (info->id < 10)
    {
        console_write("   ");
    }
    else if (info->id < 100)
    {
        console_write("  ");
    }
    else
    {
        console_write(" ");
    }
    console_write(sched_state_name(info->state));
    size_t len = strlen(sched_state_name(info->state));
    while (len++ < 8)
    {
        console_putc(' ');
    }
    console_write(info->name);
    console_putc('\n');
}

static void cmd_ps(void)
{
    console_write("PID  STATE    NAME\n");
    sched_for_each(ps_callback);
}

/* Optional: filesystem commands (ls / cat) */

static void cmd_ls(void)
{
    size_t count = fs_file_count();
    if (count == 0)
    {
        console_write("[fs] no files\n");
        return;
    }
    console_write("[fs] files:\n");
    for (size_t i = 0; i < count; ++i)
    {
        const char *name = fs_file_name(i);
        uint32_t size = (uint32_t)fs_file_size(i);
        console_write("  ");
        console_write(name ? name : "(null)");
        console_write("  (");
        console_write_dec(size);
        console_write(" bytes)\n");
    }
}

static void cmd_cat(const char *args)
{
    while (*args == ' ')
    {
        ++args;
    }
    if (*args == '\0')
    {
        console_write("Usage: cat <file>\n");
        return;
    }
    const char *data = fs_find(args);
    if (!data)
    {
        console_write("[fs] file not found: ");
        console_write(args);
        console_putc('\n');
        return;
    }
    console_write(data);
    size_t len = strlen(data);
    if (len > 0 && data[len - 1] != '\n')
    {
        console_putc('\n');
    }
}

/* ---------- spawn / kill helpers ---------- */

static int parse_uint(const char *str, uint32_t *out)
{
    if (!str || !out)
    {
        return -1;
    }
    uint32_t value = 0;
    if (*str == '\0')
    {
        return -1;
    }
    while (*str)
    {
        if (*str < '0' || *str > '9')
        {
            return -1;
        }
        value = value * 10 + (uint32_t)(*str - '0');
        ++str;
    }
    *out = value;
    return 0;
}

static void cmd_spawn(const char *args)
{
    while (*args == ' ')
    {
        ++args;
    }
    if (*args == '\0')
    {
        console_write("Usage: spawn <counter|spinner>\n");
        return;
    }
    int32_t id = sched_spawn_named(args);
    if (id < 0)
    {
        console_write("spawn failed\n");
    }
    else
    {
        console_write("Spawned task ");
        console_write_dec((uint32_t)id);
        console_putc('\n');
    }
}

static void cmd_kill(const char *args)
{
    while (*args == ' ')
    {
        ++args;
    }
    uint32_t pid;
    if (parse_uint(args, &pid) != 0)
    {
        console_write("Usage: kill <pid>\n");
        return;
    }
    if (sched_kill(pid) != 0)
    {
        console_write("kill: no such task or protected task\n");
    }
}

/* ---------- main shell loop ---------- */

void shell_run(void)
{
    char input[128];

    console_write("PenOS shell. Type 'help' for a list of commands.\n");

    while (1)
    {
        console_write("PenOS> ");
        int len = getline_block(input, sizeof(input));
        if (len < 0)
        {
            /* ESC or Ctrl+C pressed: line was cancelled */
            continue;
        }
        if (input[0] == '\0')
        {
            continue;
        }

        if (!strcmp(input, "help"))
        {
            cmd_help();
        }
        else if (!strcmp(input, "clear"))
        {
            console_clear();
        }
        else if (!strncmp(input, "echo ", 5))
        {
            cmd_echo(input + 5);
        }
        else if (!strcmp(input, "ticks"))
        {
            cmd_ticks();
        }
        else if (!strcmp(input, "sysinfo"))
        {
            app_sysinfo();
        }
        else if (!strcmp(input, "ps"))
        {
            cmd_ps();
        }
        else if (!strncmp(input, "spawn ", 6))
        {
            cmd_spawn(input + 6);
        }
        else if (!strncmp(input, "kill ", 5))
        {
            cmd_kill(input + 5);
#ifdef FS_FS_H
        }
        else if (!strcmp(input, "ls"))
        {
            cmd_ls();
        }
        else if (!strncmp(input, "cat ", 4))
        {
            cmd_cat(input + 4);
#endif
        }
        else if (!strcmp(input, "halt"))
        {
            break;
        }
        else if (!strcmp(input, "shutdown"))
        {
            console_write("Powering off...\n");
            power_shutdown();
        }
        else
        {
            console_write("Unknown command. Type 'help' for a list of commands.\n");
        }
    }
}
