#ifndef DRIVERS_VIRTIO_CONSOLE_H
#define DRIVERS_VIRTIO_CONSOLE_H

#include <stddef.h>

/**
 * VirtIO-Console Driver
 * Provides serial console I/O for debugging and logging
 */

/**
 * Initialize the VirtIO-Console device
 * Returns: 0 on success, -1 on failure
 */
int virtio_console_init(void);

/**
 * Write a single character to the console
 */
void virtio_console_putc(char c);

/**
 * Write a string to the console
 */
void virtio_console_write(const char *s);

/**
 * Read from console (optional - not implemented yet)
 * Returns: Number of bytes read, or -1 on error
 */
int virtio_console_read(char *buf, size_t len);

#endif
