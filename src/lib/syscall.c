#include "lib/syscall.h"
#include "sys/syscall_nums.h"

static inline int32_t syscall0(int num)
{
    int32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (num)
        : "memory"
    );
    return ret;
}

static inline int32_t syscall1(int num, uint32_t arg1)
{
    int32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (num), "b" (arg1)
        : "memory"
    );
    return ret;
}

void exit(void)
{
    syscall0(SYS_EXIT);
    // Should not return
    for(;;);
}

void write(const char *str)
{
    syscall1(SYS_WRITE, (uint32_t)str);
}

uint32_t ticks(void)
{
    return (uint32_t)syscall0(SYS_TICKS);
}

void yield(void)
{
    syscall0(SYS_YIELD);
}

uint32_t getpid(void)
{
    return (uint32_t)syscall0(SYS_GETPID);
}
