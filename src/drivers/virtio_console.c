#include <drivers/virtio.h>
#include <drivers/virtio_console.h>
#include <drivers/pci.h>
#include <ui/console.h>
#include <string.h>
#include <mem/heap.h>

static virtio_device_t console_dev;
static int console_initialized = 0;

// Simple TX buffer
static char tx_buffer[256];

/**
 * Initialize VirtIO-Console device
 */
int virtio_console_init(void) {
    console_write("VirtIO-Console: Initializing...\n");
    
    // Find VirtIO console device
    pci_device_t *pci_dev = pci_find_virtio_device(VIRTIO_DEV_CONSOLE);
    if (!pci_dev) {
        console_write("VirtIO-Console: No device found\n");
        return -1;
    }
    
    // Initialize VirtIO device
    if (virtio_init(pci_dev, &console_dev) != 0) {
        console_write("VirtIO-Console: Init failed\n");
        return -1;
    }
    
    // Initialize TX virtqueue
    virtqueue_init(&console_dev);
    
    console_write("VirtIO-Console: Initialized\n");
    console_initialized = 1;
    return 0;
}

/**
 * Write a single character to console
 */
void virtio_console_putc(char c) {
    if (!console_initialized) return;
    
    // Simple implementation: buffer and flush on newline
    static int buf_pos = 0;
    
    tx_buffer[buf_pos++] = c;
    
    if (c == '\n' || buf_pos >= (int)(sizeof(tx_buffer) - 1)) {
        tx_buffer[buf_pos] = '\0';
        virtio_console_write(tx_buffer);
        buf_pos = 0;
    }
}

/**
 * Write a string to console
 */
void virtio_console_write(const char *s) {
    if (!console_initialized || !s) return;
    
    size_t len = strlen(s);
    if (len == 0) return;
    
    // Copy to TX buffer (static to ensure it stays valid)
    static char write_buf[256];
    if (len >= sizeof(write_buf)) {
        len = sizeof(write_buf) - 1;
    }
    memcpy(write_buf, s, len);
    write_buf[len] = '\0';
    
    // Add buffer to TX virtqueue
    virtqueue_add_buf(&console_dev.vq, (uint8_t*)write_buf, len, 0); // Device readable
    
    // Notify device
    virtqueue_kick(&console_dev);
    
    // Wait for completion (simple polling)
    uint32_t used_len;
    int idx;
    int timeout = 1000;
    while (timeout-- > 0) {
        idx = virtqueue_get_buf(&console_dev.vq, &used_len);
        if (idx >= 0) break;
    }
}

/**
 * Read from console (not implemented)
 */
int virtio_console_read(char *buf, size_t len) {
    (void)buf;
    (void)len;
    return -1;  // Not implemented yet
}
