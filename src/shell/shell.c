#include "shell/shell.h"
#include "drivers/keyboard.h"
#include "ui/console.h"
#include "arch/x86/timer.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "mem/heap.h"
#include "apps/sysinfo.h"
#include "sched/sched.h"
#include "sys/power.h"
#include <string.h>
#include "fs/fs.h"
#include "fs/9p.h"
#include "drivers/virtio.h"
#include "drivers/mouse.h"
#include "drivers/mouse.h"
#include "lib/syscall.h"
#include "arch/x86/rtc.h"

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

/* ---------- Tab Completion Helpers ---------- */

static void redraw_prompt_with_buffer(const char *buffer) {
    console_putc('\r');
    // Clear line
    for (int i = 0; i < 80; i++) console_putc(' ');
    console_putc('\r');
    console_write("PenOS:");
    console_write(p9_getcwd());
    console_write("> ");
    console_write(buffer);
}

static int complete_command(char *buffer, int current_len) {
    const char *commands[] = {
        "help", "clear", "echo", "ticks", "sysinfo", "ps", "spawn", "kill",
        "halt", "shutdown", "pwd", "cd", "ls", "cat", NULL
    };
    
    char matches[16][32];
    int match_count = 0;
    
    for (int i = 0; commands[i]; i++) {
        if (strncmp(buffer, commands[i], current_len) == 0) {
            if (match_count < 16) {
                strcpy(matches[match_count++], commands[i]);
            }
        }
    }
    
    if (match_count == 1) {
        strcpy(buffer, matches[0]);
        return strlen(buffer);
    } else if (match_count > 1) {
        console_putc('\n');
        for (int i = 0; i < match_count; i++) {
            console_write(matches[i]);
            console_write("  ");
        }
        console_putc('\n');
    }
    
    return current_len;
}

static int complete_path(char *buffer, int current_len, int path_start) {
    // Extract the path to complete
    char path_to_complete[128];
    strcpy(path_to_complete, buffer + path_start);
    
    // Split into directory and prefix
    char dir_path[256];
    char prefix[128];
    char *last_slash = NULL;
    
    for (int i = 0; path_to_complete[i]; i++) {
        if (path_to_complete[i] == '/') {
            last_slash = &path_to_complete[i];
        }
    }
    
    if (last_slash) {
        int dir_len = last_slash - path_to_complete + 1;
        memcpy(dir_path, path_to_complete, dir_len);
        dir_path[dir_len] = '\0';
        strcpy(prefix, last_slash + 1);
    } else {
        strcpy(dir_path, ".");
        strcpy(prefix, path_to_complete);
    }
    
    // Read directory and find matches
    uint32_t fid = 1000 + (uint32_t)timer_ticks() % 1000;
    const char *target = (strcmp(dir_path, ".") == 0) ? p9_getcwd() : dir_path;
    
    const char *walk_path = target;
    if (walk_path[0] == '/') walk_path++;
    
    if (p9_walk(0, fid, walk_path) == 0 && p9_open(fid, 0) == 0) {
        uint8_t dir_buf[4096];
        int count = p9_readdir(fid, 0, sizeof(dir_buf), dir_buf);
        
        char matches[16][128];
        int match_count = 0;
        int prefix_len = strlen(prefix);
        
        if (count > 0) {
            int offset = 0;
            while (offset < count && match_count < 16) {
                if (offset + 24 > count) break;
                
                uint8_t type = dir_buf[offset + 21];
                uint16_t name_len = dir_buf[offset + 22] | (dir_buf[offset + 23] << 8);
                
                if (offset + 24 + name_len > count) break;
                
                char name[256];
                if (name_len > 255) name_len = 255;
                memcpy(name, dir_buf + offset + 24, name_len);
                name[name_len] = '\0';
                
                // Skip hidden files unless prefix starts with .
                if (name[0] != '.' || (prefix_len > 0 && prefix[0] == '.')) {
                    if (prefix_len == 0 || strncmp(name, prefix, prefix_len) == 0) {
                        strcpy(matches[match_count], name);
                        if (type == 4) {
                            strcat(matches[match_count], "/");
                        }
                        match_count++;
                    }
                }
                
                offset += 24 + name_len;
            }
        }
        
        p9_clunk(fid);
        
        if (match_count == 1) {
            // Complete it
            buffer[path_start] = '\0';
            if (last_slash) {
                strcat(buffer, dir_path);
            }
            strcat(buffer, matches[0]);
            return strlen(buffer);
        } else if (match_count > 1) {
            console_putc('\n');
            for (int i = 0; i < match_count; i++) {
                console_write(matches[i]);
                console_write("  ");
            }
            console_putc('\n');
        }
    }
    
    return current_len;
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
            virtio_input_poll();
            __asm__ volatile("hlt");
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
        else if (ch == '\t')
        { /* TAB - autocomplete */
            buffer[len] = '\0';
            
            // Check if buffer has spaces (command vs path completion)
            int has_space = 0;
            int last_space = -1;
            for (int i = 0; i < len; i++) {
                if (buffer[i] == ' ') {
                    has_space = 1;
                    last_space = i;
                }
            }
            
            if (!has_space && len > 0) {
                // Command completion
                len = complete_command(buffer, len);
                redraw_prompt_with_buffer(buffer);
            } else if (has_space) {
                // Path completion - find start of path argument
                int path_start = last_space + 1;
                while (path_start < len && buffer[path_start] == ' ') {
                    path_start++;
                }
                
                if (path_start < len) {
                    len = complete_path(buffer, len, path_start);
                    redraw_prompt_with_buffer(buffer);
                }
            }
        }
        else if (ch >= 32 && ch < 127)
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
    console_write("  date [+/-offset]  Show current date/time. Optional: set timezone (e.g. +6)\n");
    console_write("  ps                List running tasks\n");
    console_write("  spawn <name>      Start a demo task (counter|spinner)\n");
    console_write("  kill <pid>        Stop a task by PID\n");
    console_write("  halt              Exit the shell (CPU will halt)\n");
    console_write("  shutdown          Try to power off the machine\n");
#ifdef FS_FS_H
    console_write("  pwd               Print current working directory\n");
    console_write("  cd <dir>          Change directory (currently only / supported)\n");
    console_write("  ls                List files in the in-memory filesystem\n");
    console_write("  mouse             Display mouse position and button states\n");
    console_write("  cat <file>        Show contents of a file\n");
#endif
    console_write("  satastatus        Show SATA port status\n");
    console_write("  satarescan        Rescan SATA ports\n");
    console_write("  swaptest          Test swap space functionality\n");
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

/* Optional: filesystem commands (pwd / cd / ls / cat) */

static void cmd_pwd(void)
{
    console_write(p9_getcwd());
    console_putc('\n');
}

static void cmd_cd(const char *args)
{
    while (*args == ' ')
    {
        ++args;
    }
    if (*args == '\0')
    {
        args = "/";
    }
    if (p9_change_directory(args) != 0)
    {
        console_write("cd: '");
        console_write(args);
        console_write("' not found\n");
        if (args[0] != '/') {
            console_write("(Relative to ");
            console_write(p9_getcwd());
            console_write(")\n");
        }
    }
}

static void cmd_ls(void)
{
    if (p9_list_directory(NULL) != 0) {
        console_write("Failed to list directory.\n");
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

static void cmd_mouse(void)
{
    int x, y;
    mouse_get_position(&x, &y);
    int buttons = mouse_get_buttons();
    
    console_write("Mouse Position: X=");
    console_write_dec(x);
    console_write(" Y=");
    console_write_dec(y);
    console_write(" Buttons=[");
    if (buttons & 1) console_write("L");
    if (buttons & 2) console_write("R");
    if (buttons & 4) console_write("M");
    console_write("]\n");
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

static void cmd_exec(const char *args)
{
    while (*args == ' ') args++;
    if (*args == '\0') {
        console_write("Usage: exec <path>\n");
        return;
    }
    
    int32_t pid = sched_spawn_elf(args);
    if (pid < 0) {
        console_write("Failed to execute: ");
        console_write(args);
        console_write("\n");
    } else {
        console_write("Started process ");
        console_write_dec(pid);
        console_write(": ");
        console_write(args);
        console_write("\n");
    }
}

#include <drivers/ahci.h>
#include <mem/heap.h>

static void cmd_sata(void) {
    console_write("Testing SATA Disk I/O...\n");
    
    // 1. Identify
    uint16_t *id_buf = (uint16_t*)kmalloc(512);
    if (ahci_identify(0, id_buf) == 0) {
        console_write("Identify: Success\n");
        // Print model (words 27-46)
        console_write("Model: ");
        for (int i = 27; i < 47; i++) {
            char c1 = (id_buf[i] >> 8) & 0xFF;
            char c2 = id_buf[i] & 0xFF;
            console_putc(c2); console_putc(c1);
        }
        console_write("\n");
        
        // Print capacity (LBA48 words 100-103)
        uint64_t sectors = *(uint64_t*)&id_buf[100];
        console_write("Sectors: ");
        console_write_dec((uint32_t)sectors); // Truncated for print
        console_write("\n");
    } else {
        console_write("Identify: Failed\n");
    }
    kfree(id_buf);
    
    // 2. Write Test
    console_write("Writing to sector 0...\n");
    char *write_buf = (char*)kmalloc(512);
    strcpy(write_buf, "Hello, SATA World! This is a test sector.");
    if (ahci_write(0, 0, 1, write_buf) == 0) {
        console_write("Write: Success\n");
    } else {
        console_write("Write: Failed\n");
    }
    
    // 3. Read Test
    console_write("Reading from sector 0...\n");
    char *read_buf = (char*)kmalloc(512);
    memset(read_buf, 0, 512);
    if (ahci_read(0, 0, 1, read_buf) == 0) {
        console_write("Read: Success\n");
        console_write("Data: ");
        console_write(read_buf);
        console_write("\n");
        
        if (strcmp(write_buf, read_buf) == 0) {
            console_write("Verification: PASSED\n");
        } else {
            console_write("Verification: FAILED\n");
        }
    } else {
        console_write("Read: Failed\n");
    }
    
    kfree(write_buf);
    kfree(read_buf);
    kfree(write_buf);
    kfree(read_buf);
}

static void cmd_satastatus(void) {
    console_write("SATA Port Status:\n");
    for (int i = 0; i < 32; i++) {
        if (ahci_port_is_connected(i)) {
            console_write("Port ");
            console_write_dec(i);
            console_write(": CONNECTED\n");
            
            // Try to identify the drive to get more info
            uint16_t *id_buf = (uint16_t*)kmalloc(512);
            if (ahci_identify(i, id_buf) == 0) {
                console_write("  Model: ");
                for (int j = 27; j < 47; j++) {
                    char c1 = (id_buf[j] >> 8) & 0xFF;
                    char c2 = id_buf[j] & 0xFF;
                    console_putc(c2); console_putc(c1);
                }
                console_write("\n");
                
                uint64_t sectors = *(uint64_t*)&id_buf[100];
                console_write("  Size: ");
                console_write_dec((uint32_t)(sectors * 512 / 1024 / 1024));
                console_write(" MB\n");
            }
            kfree(id_buf);
        }
    }
}

static void cmd_satarescan(void) {
    ahci_scan_ports();
}

static void cmd_swaptest(void) {
    console_write("Swap: Allocating test page...\n");
    
    // Allocate a page-aligned buffer
    uint32_t *page = (uint32_t*)kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
    if (!page) {
        console_write("Swap: Failed to allocate page\n");
        return;
    }
    
    console_write("Swap: Writing data to ");
    console_write_hex((uint32_t)page);
    console_write("\n");
    
    // Write pattern
    for (int i = 0; i < 1024; i++) {
        page[i] = i;
    }
    
    console_write("Swap: Swapping out page...\n");
    if (paging_swap_out((uint32_t)page) != 0) {
        console_write("Swap: Failed to swap out (is swap enabled?)\n");
        kfree(page);
        return;
    }
    
    console_write("Swap: Page swapped out. Accessing to trigger fault...\n");
    
    // Read and verify
    int errors = 0;
    for (int i = 0; i < 1024; i++) {
        if (page[i] != (uint32_t)i) {
            errors++;
        }
    }
    
    if (errors == 0) {
        console_write("Swap: Test PASSED! Data verified.\n");
    } else {
        console_write("Swap: Test FAILED! Data mismatch.\n");
    }
    
    kfree(page);
}

static void user_mode_test_task(void) {
    // ALL strings must be on stack to be in user-accessible memory
    char msg1[] = "[User] Hello from Ring 3! PID=";
    char msg2[] = "[User] Yielding...\n";
    char msg3[] = "[User] Back from yield. Exiting...\n";
    
    write(msg1);
    
    // Print PID using a simple conversion
    uint32_t pid = getpid();
    char pid_str[16];
    int i = 0;
    if (pid == 0) {
        pid_str[i++] = '0';
    } else {
        uint32_t temp = pid;
        while (temp > 0) {
            temp /= 10;
            i++;
        }
        temp = pid;
        int len = i;
        while (temp > 0) {
            pid_str[--len] = (temp % 10) + '0';
            temp /= 10;
        }
    }
    pid_str[i++] = '\n';
    pid_str[i] = '\0';
    write(pid_str);
    
    write(msg2);
    yield();
    write(msg3);
    exit();
}

static int timezone_offset = 0;

static int is_leap_year(uint32_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int get_days_in_month(int month, uint32_t year) {
    static const int days_per_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) return 29;
    return days_per_month[month];
}

static void cmd_date(const char *args) {
    // Parse arguments if present to set timezone
    while (*args == ' ') args++;
    if (*args != '\0') {
        // Simple parsing: expect "+N" or "-N"
        int sign = 1;
        if (*args == '-') {
            sign = -1;
            args++;
        } else if (*args == '+') {
            args++;
        }
        
        uint32_t val;
        if (parse_uint(args, &val) == 0) {
            if (val > 14) {
                console_write("Invalid timezone offset (must be between -14 and +14)\n");
                return;
            }
            timezone_offset = sign * (int)val;
            console_write("Timezone set to UTC");
            if (timezone_offset >= 0) console_putc('+');
            console_write_dec(timezone_offset);
            console_putc('\n');
        } else {
            console_write("Usage: date [+/-offset]\n");
            return;
        }
    }

    rtc_time_t t;
    rtc_get_time(&t);
    
    // Apply timezone offset
    int hour = (int)t.hour + timezone_offset;
    int day = t.day;
    int month = t.month;
    uint32_t year = t.year;
    
    // Handle rollover
    if (hour >= 24) {
        hour -= 24;
        day++;
        if (day > get_days_in_month(month, year)) {
            day = 1;
            month++;
            if (month > 12) {
                month = 1;
                year++;
            }
        }
    } else if (hour < 0) {
        hour += 24;
        day--;
        if (day < 1) {
            month--;
            if (month < 1) {
                month = 12;
                year--;
            }
            day = get_days_in_month(month, year);
        }
    }
    
    console_write("Date: ");
    console_write_dec(year);
    console_putc('-');
    if (month < 10) console_putc('0');
    console_write_dec(month);
    console_putc('-');
    if (day < 10) console_putc('0');
    console_write_dec(day);
    
    console_write(" Time: ");
    if (hour < 10) console_putc('0');
    console_write_dec(hour);
    console_putc(':');
    if (t.minute < 10) console_putc('0');
    console_write_dec(t.minute);
    console_putc(':');
    if (t.second < 10) console_putc('0');
    console_write_dec(t.second);
    
    if (timezone_offset != 0) {
        console_write(" (UTC");
        if (timezone_offset > 0) console_putc('+');
        console_write_dec(timezone_offset);
        console_write(")");
    } else {
        console_write(" (UTC)");
    }
    console_putc('\n');
}

static void cmd_usermode(void) {
    console_write("Spawning User Mode task...\n");
    int32_t pid = sched_spawn_user(user_mode_test_task, "user_test");
    if (pid > 0) {
        console_write("Spawned user task PID: ");
        console_write_dec(pid);
        console_write("\n");
    } else {
        console_write("Failed to spawn user task.\n");
    }
}

/* ---------- main shell loop ---------- */

void shell_run(void)
{
    char input[128];

    console_write("PenOS shell. Type 'help' for a list of commands.\n");

    while (1)
    {
        console_write("PenOS:");
        console_write(p9_getcwd());
        console_write("> ");
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
        else if (!strncmp(input, "date", 4))
        {
            cmd_date(input + 4);
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
        }
        else if (!strncmp(input, "exec ", 5))
        {
            cmd_exec(input + 5);
        }
        else if (!strcmp(input, "pwd"))
        {
            cmd_pwd();
        }
        else if (!strncmp(input, "cd ", 3))
        {
            cmd_cd(input + 3);
        }
        else if (!strcmp(input, "cd"))
        {
            cmd_cd("");
        }
        else if (!strcmp(input, "ls"))
        {
            cmd_ls();
        }
        else if (!strncmp(input, "cat ", 4))
        {
            cmd_cat(input + 4);
        }
        else if (!strcmp(input, "halt"))
        {
            break;
        }
        else if (!strcmp(input, "mouse"))
        {
            cmd_mouse();
        }
        else if (!strcmp(input, "sata"))
        {
            cmd_sata();
        }
        else if (!strcmp(input, "satastatus"))
        {
            cmd_satastatus();
        }
        else if (!strcmp(input, "satarescan"))
        {
            cmd_satarescan();
        }
        else if (!strcmp(input, "swaptest"))
        {
            cmd_swaptest();
        }
        else if (!strcmp(input, "usermode"))
        {
            cmd_usermode();
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
