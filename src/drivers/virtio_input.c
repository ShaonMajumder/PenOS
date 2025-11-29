#include <drivers/virtio.h>
#include <drivers/pci.h>
#include <drivers/keyboard.h>
#include <ui/console.h>
#include <mem/heap.h>
#include <string.h>

static virtio_device_t input_dev;
static int input_initialized = 0;

// Linux Input Event Codes (simplified)
#define EV_KEY 0x01
#define EV_REL 0x02

// Mouse event codes
#define REL_X 0x00
#define REL_Y 0x01
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112

// Keyboard event codes
#define KEY_ESC 1
#define KEY_1 2
#define KEY_2 3
#define KEY_3 4
#define KEY_4 5
#define KEY_5 6
#define KEY_6 7
#define KEY_7 8
#define KEY_8 9
#define KEY_9 10
#define KEY_0 11
#define KEY_MINUS 12
#define KEY_EQUAL 13
#define KEY_BACKSPACE 14
#define KEY_TAB 15
#define KEY_Q 16
#define KEY_W 17
#define KEY_E 18
#define KEY_R 19
#define KEY_T 20
#define KEY_Y 21
#define KEY_U 22
#define KEY_I 23
#define KEY_O 24
#define KEY_P 25
#define KEY_LEFTBRACE 26
#define KEY_RIGHTBRACE 27
#define KEY_ENTER 28
#define KEY_LEFTCTRL 29
#define KEY_A 30
#define KEY_S 31
#define KEY_D 32
#define KEY_F 33
#define KEY_G 34
#define KEY_H 35
#define KEY_J 36
#define KEY_K 37
#define KEY_L 38
#define KEY_SEMICOLON 39
#define KEY_APOSTROPHE 40
#define KEY_GRAVE 41
#define KEY_LEFTSHIFT 42
#define KEY_BACKSLASH 43
#define KEY_Z 44
#define KEY_X 45
#define KEY_C 46
#define KEY_V 47
#define KEY_B 48
#define KEY_N 49
#define KEY_M 50
#define KEY_COMMA 51
#define KEY_DOT 52
#define KEY_SLASH 53
#define KEY_RIGHTSHIFT 54
#define KEY_KPASTERISK 55
#define KEY_LEFTALT 56
#define KEY_SPACE 57

// Simple translation table (index = KEY_ code)
static const char key_map[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 8, 9,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

// Mouse state
typedef struct {
    int x;
    int y;
    int buttons;  // Bit 0=left, 1=right, 2=middle
} mouse_state_t;

static mouse_state_t mouse_state = {0, 0, 0};

static virtio_input_event_t events[32]; // Buffer for events

// Poll for input events
void virtio_input_poll(void) {
    if (!input_initialized) return;

    uint32_t len;
    int idx = virtqueue_get_buf(&input_dev.vq, &len);
    
    while (idx >= 0) {
        // Process event
        virtio_input_event_t *evt = &events[idx % 32]; // Simplified mapping
        
        if (evt->type == EV_KEY && evt->value == 1) { // Key press
            if (evt->code < sizeof(key_map)) {
                char c = key_map[evt->code];
                if (c) {
                    keyboard_push_char(c);
                }
            }
        } else if (evt->type == EV_REL) { // Mouse movement
            if (evt->code == REL_X) {
                mouse_state.x += (int32_t)evt->value;
            } else if (evt->code == REL_Y) {
                mouse_state.y += (int32_t)evt->value;
            }
        } else if (evt->type == EV_KEY) { // Mouse buttons
            if (evt->code == BTN_LEFT) {
                if (evt->value) mouse_state.buttons |= 1;
                else mouse_state.buttons &= ~1;
            } else if (evt->code == BTN_RIGHT) {
                if (evt->value) mouse_state.buttons |= 2;
                else mouse_state.buttons &= ~2;
            } else if (evt->code == BTN_MIDDLE) {
                if (evt->value) mouse_state.buttons |= 4;
                else mouse_state.buttons &= ~4;
            }
        }
        
        // Requeue buffer
        virtqueue_add_buf(&input_dev.vq, (uint8_t*)evt, sizeof(virtio_input_event_t), 1); // Device writable
        
        // Check next
        idx = virtqueue_get_buf(&input_dev.vq, &len);
    }
    
    // Kick if we processed anything (though usually input is interrupt driven, polling works for now)
    virtqueue_kick(&input_dev);
}

int virtio_input_init(void) {
    console_write("VirtIO-Input: Initializing...\n");
    
    pci_device_t *pci_dev = pci_find_virtio_device(VIRTIO_DEV_INPUT);
    if (!pci_dev) {
        console_write("VirtIO-Input: No device found\n");
        return -1;
    }
    
    if (virtio_init(pci_dev, &input_dev) != 0) {
        console_write("VirtIO-Input: Init failed\n");
        return -1;
    }
    
    virtqueue_init(&input_dev);
    
    // Populate receive queue
    for (int i = 0; i < 16; i++) {
        virtqueue_add_buf(&input_dev.vq, (uint8_t*)&events[i], sizeof(virtio_input_event_t), 1);
    }
    virtqueue_kick(&input_dev);
    
    console_write("VirtIO-Input: Initialized\n");
    input_initialized = 1;
    return 0;
}

void mouse_get_position(int *x, int *y) {
    if (x) *x = mouse_state.x;
    if (y) *y = mouse_state.y;
}

int mouse_get_buttons(void) {
    return mouse_state.buttons;
}
