#include "shell/shell.h"
#include "drivers/keyboard.h"
#include "ui/console.h"
#include "arch/x86/timer.h"
#include "mem/pmm.h"
#include "apps/sysinfo.h"
#include <string.h>

static int getline_block(char *buffer, int max)
{
    int len = 0;
    while (len < max - 1) {
        int c;
        while ((c = keyboard_read_char()) == -1) {
        }
        char ch = (char)c;
        if (ch == '\r' || ch == '\n') {
            console_putc('\n');
            break;
        } else if (ch == 8) {
            if (len > 0) {
                len--;
                console_write("\b \b");
            }
        } else {
            buffer[len++] = ch;
            console_putc(ch);
        }
    }
    buffer[len] = '\0';
    return len;
}

static void cmd_help(void)
{
    console_write("Commands: help, clear, echo, ticks, sysinfo, halt\n");
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

void shell_run(void)
{
    char input[128];
    while (1) {
        console_write("PenOS> ");
        getline_block(input, sizeof(input));
        if (input[0] == '\0') {
            continue;
        }
        if (!strcmp(input, "help")) {
            cmd_help();
        } else if (!strcmp(input, "clear")) {
            console_clear();
        } else if (!strncmp(input, "echo ", 5)) {
            cmd_echo(input + 5);
        } else if (!strcmp(input, "ticks")) {
            cmd_ticks();
        } else if (!strcmp(input, "sysinfo")) {
            app_sysinfo();
        } else if (!strcmp(input, "halt")) {
            break;
        } else {
            console_write("Unknown command.\n");
        }
    }
}
