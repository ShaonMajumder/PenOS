#include <lib/syscall.h>

void main(void) {
    write("Hello from userspace ELF!\n");
    write("My PID is: ");
    
    // Simple integer to string
    uint32_t pid = getpid();
    char buf[16];
    int i = 0;
    if (pid == 0) buf[i++] = '0';
    else {
        uint32_t temp = pid;
        while (temp > 0) { temp /= 10; i++; }
        temp = pid;
        int len = i;
        while (temp > 0) { buf[--len] = (temp % 10) + '0'; temp /= 10; }
    }
    buf[i++] = '\n';
    buf[i] = 0;
    write(buf);
    
    exit();
}
