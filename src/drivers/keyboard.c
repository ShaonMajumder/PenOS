#include "drivers/keyboard.h"
#include "arch/x86/io.h"
#include "arch/x86/interrupts.h"
#include "ui/console.h"

#define BUFFER_SIZE 64

static char buffer[BUFFER_SIZE];
static int head = 0;
static int tail = 0;
static int shift_down = 0;
static int ctrl_down = 0;

static const char scancode_set1[128] = {
    [1] = 27,
    [2] = '1',
    [3] = '2',
    [4] = '3',
    [5] = '4',
    [6] = '5',
    [7] = '6',
    [8] = '7',
    [9] = '8',
    [10] = '9',
    [11] = '0',
    [12] = '-',
    [13] = '=',
    [14] = 8,
    [15] = 9,
    [16] = 'q',
    [17] = 'w',
    [18] = 'e',
    [19] = 'r',
    [20] = 't',
    [21] = 'y',
    [22] = 'u',
    [23] = 'i',
    [24] = 'o',
    [25] = 'p',
    [26] = '[',
    [27] = ']',
    [28] = '\n',
    [29] = 0,
    [30] = 'a',
    [31] = 's',
    [32] = 'd',
    [33] = 'f',
    [34] = 'g',
    [35] = 'h',
    [36] = 'j',
    [37] = 'k',
    [38] = 'l',
    [39] = ';',
    [40] = '\'',
    [41] = '`',
    [42] = 0,
    [43] = '\\',
    [44] = 'z',
    [45] = 'x',
    [46] = 'c',
    [47] = 'v',
    [48] = 'b',
    [49] = 'n',
    [50] = 'm',
    [51] = ',',
    [52] = '.',
    [53] = '/',
    [54] = 0,
    [55] = '*',
    [56] = 0,
    [57] = ' ',
};

static const char scancode_set1_shift[128] = {
    [1] = 27,
    [2] = '!',
    [3] = '@',
    [4] = '#',
    [5] = '$',
    [6] = '%',
    [7] = '^',
    [8] = '&',
    [9] = '*',
    [10] = '(',
    [11] = ')',
    [12] = '_',
    [13] = '+',
    [14] = 8,
    [15] = 9,
    [16] = 'Q',
    [17] = 'W',
    [18] = 'E',
    [19] = 'R',
    [20] = 'T',
    [21] = 'Y',
    [22] = 'U',
    [23] = 'I',
    [24] = 'O',
    [25] = 'P',
    [26] = '{',
    [27] = '}',
    [28] = '\n',
    [29] = 0,
    [30] = 'A',
    [31] = 'S',
    [32] = 'D',
    [33] = 'F',
    [34] = 'G',
    [35] = 'H',
    [36] = 'J',
    [37] = 'K',
    [38] = 'L',
    [39] = ':',
    [40] = '\"',
    [41] = '~',
    [42] = 0,
    [43] = '|',
    [44] = 'Z',
    [45] = 'X',
    [46] = 'C',
    [47] = 'V',
    [48] = 'B',
    [49] = 'N',
    [50] = 'M',
    [51] = '<',
    [52] = '>',
    [53] = '?',
    [54] = 0,
    [55] = '*',
    [56] = 0,
    [57] = ' ',
};

static char translate_scancode(uint8_t sc)
{
    if (sc >= sizeof(scancode_set1)) {
        return 0;
    }
    char c = shift_down ? scancode_set1_shift[sc] : scancode_set1[sc];
    if (!c && shift_down) {
        c = scancode_set1[sc];
    }
    if (ctrl_down) {
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 1);
        } else if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 1);
        }
    }
    return c;
}

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
        uint8_t released = sc & 0x7F;
        if (released == 0x2A || released == 0x36) {
            shift_down = 0;
        } else if (released == 0x1D) {
            ctrl_down = 0;
        }
        return;
    }
    if (sc == 0x2A || sc == 0x36) {
        shift_down = 1;
        return;
    }
    if (sc == 0x1D) {
        ctrl_down = 1;
        return;
    }
    char c = translate_scancode(sc);
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
