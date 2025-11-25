#include "drivers/mouse.h"
#include "ui/console.h"
#include "arch/x86/interrupts.h"
#include "arch/x86/io.h"
#include <string.h>

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define MOUSE_IRQ_VECTOR 44

static volatile mouse_state_t g_state;
static int packet_index = 0;
static uint8_t packet[3];

static void mouse_wait(uint8_t type)
{
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(PS2_STATUS) & 0x01) != 0) {
                return;
            }
        }
    } else {
        while (timeout--) {
            if ((inb(PS2_STATUS) & 0x02) == 0) {
                return;
            }
        }
    }
}

static void mouse_write(uint8_t val)
{
    mouse_wait(1);
    outb(PS2_CMD, 0xD4);
    mouse_wait(1);
    outb(PS2_DATA, val);
}

static uint8_t mouse_read(void)
{
    mouse_wait(0);
    return inb(PS2_DATA);
}

static void print_signed(int32_t v)
{
    if (v < 0) {
        console_putc('-');
        v = -v;
    }
    console_write_dec((uint32_t)v);
}

static void mouse_handle_packet(void)
{
    int dx = (int8_t)packet[1];
    int dy = (int8_t)packet[2];
    g_state.x += dx;
    g_state.y -= dy;
    g_state.buttons = packet[0] & 0x07;

    console_write("[mouse] x=");
    print_signed(g_state.x);
    console_write(" y=");
    print_signed(g_state.y);
    console_write(" btn=");
    console_write_dec(g_state.buttons);
    console_write("\n");
}

static void mouse_irq(interrupt_frame_t *frame)
{
    (void)frame;
    uint8_t data = inb(PS2_DATA);
    packet[packet_index++] = data;
    if (packet_index == 3) {
        mouse_handle_packet();
        packet_index = 0;
    }
}

void mouse_init(void)
{
    memset((void *)&g_state, 0, sizeof(g_state));
    packet_index = 0;

    mouse_wait(1);
    outb(PS2_CMD, 0xA8);

    mouse_wait(1);
    outb(PS2_CMD, 0x20);
    mouse_wait(0);
    uint8_t status = inb(PS2_DATA);
    status |= 0x02;
    status |= 0x01;
    mouse_wait(1);
    outb(PS2_CMD, 0x60);
    mouse_wait(1);
    outb(PS2_DATA, status);

    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();

    register_interrupt_handler(MOUSE_IRQ_VECTOR, mouse_irq);
    console_write("PS/2 mouse initialized\n");
}

void mouse_get_state(mouse_state_t *out)
{
    if (!out) {
        return;
    }
    *out = g_state;
}
