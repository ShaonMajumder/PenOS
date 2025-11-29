#ifndef DRIVERS_VIRTIO_H
#define DRIVERS_VIRTIO_H

#include <stdint.h>
#include <drivers/pci.h>

// VirtIO Device IDs
#define VIRTIO_DEV_NETWORK   0x1000
#define VIRTIO_DEV_BLOCK     0x1001
#define VIRTIO_DEV_9P        0x1009  // 9P filesystem

// VirtIO Status Register Bits
#define VIRTIO_STATUS_ACKNOWLEDGE  1
#define VIRTIO_STATUS_DRIVER       2
#define VIRTIO_STATUS_DRIVER_OK    4
#define VIRTIO_STATUS_FEATURES_OK  8
#define VIRTIO_STATUS_FAILED       128

// VirtQueue Descriptor Flags
#define VRING_DESC_F_NEXT      1
#define VRING_DESC_F_WRITE     2
#define VRING_DESC_F_INDIRECT  4

#define VIRTQUEUE_SIZE 128

// VirtQueue Descriptor
typedef struct {
    uint64_t addr;    // Physical address
    uint32_t len;     // Length
    uint16_t flags;   // Flags
    uint16_t next;    // Next descriptor index
} __attribute__((packed)) vring_desc_t;

// VirtQueue Available Ring
typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTQUEUE_SIZE];
} __attribute__((packed)) vring_avail_t;

// VirtQueue Used Element
typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) vring_used_elem_t;

// VirtQueue Used Ring
typedef struct {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem_t ring[VIRTQUEUE_SIZE];
} __attribute__((packed)) vring_used_t;

// VirtQueue
typedef struct {
    vring_desc_t *desc;
    vring_avail_t *avail;
    vring_used_t *used;
    uint16_t num;
    uint16_t last_used_idx;
    uint16_t free_head;
    uint16_t num_free;
} virtqueue_t;

// VirtIO Device
typedef struct {
    pci_device_t *pci_dev;
    uint32_t iobase;
    virtqueue_t vq;
    uint8_t status;
} virtio_device_t;

// Buffer descriptor for chaining
typedef struct {
    void *buf;
    uint32_t len;
    int write; // 1 for device-writable (RX), 0 for device-readable (TX)
} virtio_buf_desc_t;

// Functions
int virtio_init(pci_device_t *pci_dev, virtio_device_t *dev);
void virtqueue_init(virtio_device_t *dev);
int virtqueue_add_buf(virtqueue_t *vq, void *buf, uint32_t len, int write);
int virtqueue_add_chain(virtqueue_t *vq, virtio_buf_desc_t *bufs, int count);
void virtqueue_kick(virtio_device_t *dev);
int virtqueue_get_buf(virtqueue_t *vq, uint32_t *len);


#endif
