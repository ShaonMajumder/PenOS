#include "drivers/keyboard.h"
#include "arch/x86/io.h"
#include "arch/x86/interrupts.h"
#include "ui/console.h"

#define BUFFER_SIZE 64

static char buffer[BUFFER_SIZE];
static int head = 0;
static int tail = 0;

static const char scancode_set1[] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
    'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
    'b','n','m',',','.','/',0,'*',0,' ',0
};

static void buffer_put(char c)
{
    int next = (head + 1) % BUFFER_SIZE;
    if (next != tail) {
        buffer[head] = c;
        head = next;
    }
}

static int buffer_get(void)
{
    if (tail == head) {
        return -1;
    }
    char c = buffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    return c;
}

static void keyboard_callback(interrupt_frame_t *frame)
{
    (void)frame;
    uint8_t sc = inb(0x60);
    if (sc & 0x80) {
        return;
    }
    char c = 0;
    if (sc < sizeof(scancode_set1)) {
        c = scancode_set1[sc];
    }
    if (c) {
        buffer_put(c);
    }
}

void keyboard_init(void)
{
    register_interrupt_handler(33, keyboard_callback);
}

int keyboard_read_char(void)
{
    return buffer_get();
}
