#include <drivers/virtio.h>
#include <drivers/pci.h>
#include <arch/x86/io.h>
#include <ui/console.h>
#include <mem/heap.h>
#include <mem/paging.h>
#include <string.h>

// VirtIO Legacy PCI I/O Port Offsets
#define VIRTIO_PCI_DEVICE_FEATURES  0
#define VIRTIO_PCI_GUEST_FEATURES   4
#define VIRTIO_PCI_QUEUE_ADDR       8
#define VIRTIO_PCI_QUEUE_SIZE       12
#define VIRTIO_PCI_QUEUE_SEL        14
#define VIRTIO_PCI_QUEUE_NOTIFY     16
#define VIRTIO_PCI_STATUS           18
#define VIRTIO_PCI_ISR              19

static uint8_t virtio_read8(virtio_device_t *dev, uint32_t offset) {
    return inb(dev->iobase + offset);
}

static uint16_t virtio_read16(virtio_device_t *dev, uint32_t offset) {
    return inw(dev->iobase + offset);
}

static uint32_t virtio_read32(virtio_device_t *dev, uint32_t offset) {
    return inl(dev->iobase + offset);
}

static void virtio_write8(virtio_device_t *dev, uint32_t offset, uint8_t val) {
    outb(dev->iobase + offset, val);
}

static void virtio_write16(virtio_device_t *dev, uint32_t offset, uint16_t val) {
    outw(dev->iobase + offset, val);
}

static void virtio_write32(virtio_device_t *dev, uint32_t offset, uint32_t val) {
    outl(dev->iobase + offset, val);
}

// Initialize VirtIO device
int virtio_init(pci_device_t *pci_dev, virtio_device_t *dev) {
    dev->pci_dev = pci_dev;
    
    // Get I/O base from BAR0 (legacy mode)
    dev->iobase = pci_dev->bar0 & 0xFFFFFFFC;  // Clear bottom 2 bits
    
    console_write("VirtIO: Initializing device at I/O base ");
    console_write_hex(dev->iobase);
    console_putc('\n');
    
    // Reset device
    virtio_write8(dev, VIRTIO_PCI_STATUS, 0);
    
    // Set ACKNOWLEDGE bit
    uint8_t status = VIRTIO_STATUS_ACKNOWLEDGE;
    virtio_write8(dev, VIRTIO_PCI_STATUS, status);
    
    // Set DRIVER bit
    status |= VIRTIO_STATUS_DRIVER;
    virtio_write8(dev, VIRTIO_PCI_STATUS, status);
    
    // Read device features
    uint32_t features = virtio_read32(dev, VIRTIO_PCI_DEVICE_FEATURES);
    console_write("VirtIO: Device features: ");
    console_write_hex(features);
    console_putc('\n');
    
    // Write guest features (accept all for now)
    virtio_write32(dev, VIRTIO_PCI_GUEST_FEATURES, features);
    
    // Set FEATURES_OK
    status |= VIRTIO_STATUS_FEATURES_OK;
    virtio_write8(dev, VIRTIO_PCI_STATUS, status);
    
    // Verify FEATURES_OK
    status = virtio_read8(dev, VIRTIO_PCI_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        console_write("VirtIO: Features not accepted\n");
        return -1;
    }
    
    return 0;
}

// Initialize virtqueue
void virtqueue_init(virtio_device_t *dev) {
    // Select queue 0
    virtio_write16(dev, VIRTIO_PCI_QUEUE_SEL, 0);
    
    // Get queue size
    uint16_t queue_size = virtio_read16(dev, VIRTIO_PCI_QUEUE_SIZE);
    if (queue_size == 0 || queue_size > VIRTQUEUE_SIZE) {
        queue_size = VIRTQUEUE_SIZE;
    }
    
    console_write("VirtIO: Queue size: ");
    console_write_dec(queue_size);
    console_putc('\n');
    
    // Allocate virtqueue structures
    // Need: descriptors, available ring, used ring
    size_t desc_size = sizeof(vring_desc_t) * queue_size;
    size_t avail_size = sizeof(vring_avail_t);
    size_t used_size = sizeof(vring_used_t);
    size_t total_size = desc_size + avail_size + used_size + 4096; // Add padding
    
    void *vq_mem = kmalloc(total_size);
    if (!vq_mem) {
        console_write("VirtIO: Failed to allocate virtqueue\n");
        return;
    }
    memset(vq_mem, 0, total_size);
    
    // Set up pointers
    dev->vq.desc = (vring_desc_t *)vq_mem;
    dev->vq.avail = (vring_avail_t *)((uint8_t *)vq_mem + desc_size);
    dev->vq.used = (vring_used_t *)((uint8_t *)dev->vq.avail + avail_size);
    dev->vq.num = queue_size;
    dev->vq.last_used_idx = 0;
    dev->vq.free_head = 0;
    dev->vq.num_free = queue_size;
    
    // Initialize descriptor free list
    for (uint16_t i = 0; i < queue_size - 1; i++) {
        dev->vq.desc[i].next = i + 1;
    }
    dev->vq.desc[queue_size - 1].next = 0;
    
    // Get physical address
    uint32_t phys_addr = paging_virt_to_phys((uint32_t)vq_mem);
    uint32_t pfn = phys_addr >> 12;  // Page frame number
    
    // Tell device about queue
    virtio_write32(dev, VIRTIO_PCI_QUEUE_ADDR, pfn);
    
    console_write("VirtIO: Virtqueue initialized at PFN ");
    console_write_hex(pfn);
    console_putc('\n');
    
    // Set DRIVER_OK
    uint8_t status = virtio_read8(dev, VIRTIO_PCI_STATUS);
    status |= VIRTIO_STATUS_DRIVER_OK;
    virtio_write8(dev, VIRTIO_PCI_STATUS, status);
}

// Add buffer to virtqueue
int virtqueue_add_buf(virtqueue_t *vq, void *buf, uint32_t len, int write) {
    if (vq->num_free == 0) {
        return -1;  // No free descriptors
    }
    
    uint16_t head = vq->free_head;
    uint16_t idx = head;
    
    // Set up descriptor
    vq->desc[idx].addr = (uint64_t)paging_virt_to_phys((uint32_t)buf);
    vq->desc[idx].len = len;
    vq->desc[idx].flags = write ? VRING_DESC_F_WRITE : 0;
    vq->desc[idx].next = 0;
    
    // Update free list
    vq->free_head = vq->desc[head].next;
    vq->num_free--;
    
    // Add to available ring
    uint16_t avail_idx = vq->avail->idx;
    vq->avail->ring[avail_idx % vq->num] = head;
    vq->avail->idx = avail_idx + 1;
    
    return head;
}

// Notify device
void virtqueue_kick(virtio_device_t *dev) {
    virtio_write16(dev, VIRTIO_PCI_QUEUE_NOTIFY, 0);
}

// Get completed buffer
int virtqueue_get_buf(virtqueue_t *vq, uint32_t *len) {
    if (vq->last_used_idx == vq->used->idx) {
        return -1;  // No new completions
    }
    
    uint16_t used_idx = vq->last_used_idx % vq->num;
    uint32_t desc_idx = vq->used->ring[used_idx].id;
    
    if (len) {
        *len = vq->used->ring[used_idx].len;
    }
    
    // Return descriptor to free list
    vq->desc[desc_idx].next = vq->free_head;
    vq->free_head = desc_idx;
    vq->num_free++;
    
    vq->last_used_idx++;
    
    return desc_idx;
}
