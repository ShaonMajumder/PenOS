#include <fs/9p.h>
#include <drivers/virtio.h>
#include <drivers/pci.h>
#include <ui/console.h>
#include <mem/heap.h>
#include <string.h>

static virtio_device_t p9_dev;
static uint16_t p9_tag = 0;
static uint32_t next_fid = 1;

#define P9_MAX_MSG_SIZE 8192

static uint8_t tx_buffer[P9_MAX_MSG_SIZE];
static uint8_t rx_buffer[P9_MAX_MSG_SIZE];

// Helper: write 16-bit value
static void write_u16(uint8_t *buf, uint16_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
}

// Helper: write 32-bit value
static void write_u32(uint8_t *buf, uint32_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

// Helper: read 32-bit value
static uint32_t read_u32(const uint8_t *buf) {
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

// Helper: write string
static uint32_t write_string(uint8_t *buf, const char *str) {
    uint16_t len = str ? strlen(str) : 0;
    write_u16(buf, len);
    if (len > 0) {
        memcpy(buf + 2, str, len);
    }
    return 2 + len;
}

// Send 9p message and receive response
static int p9_rpc(uint8_t *tx, uint32_t tx_len, uint8_t *rx, uint32_t *rx_len) {
    // Add TX buffer to virtqueue
    int tx_idx = virtqueue_add_buf(&p9_dev.vq, tx, tx_len, 0);
    if (tx_idx < 0) {
        return -1;
    }
    
    // Add RX buffer to virtqueue
    int rx_idx = virtqueue_add_buf(&p9_dev.vq, rx, P9_MAX_MSG_SIZE, 1);
    if (rx_idx < 0) {
        return -1;
    }
    
    // Kick device
    virtqueue_kick(&p9_dev);
    
    // Wait for response (simple polling)
    for (int i = 0; i < 1000000; i++) {
        uint32_t len;
        int idx = virtqueue_get_buf(&p9_dev.vq, &len);
        if (idx >= 0) {
            if (rx_len) *rx_len = len;
            return 0;
        }
    }
    
    return -1;  // Timeout
}

// Initialize 9p filesystem
int p9_init(void) {
    console_write("9P: Initializing...\n");
    
    // Find VirtIO 9P device
    pci_device_t *pci_dev = pci_find_virtio_device(VIRTIO_DEV_9P);
    if (!pci_dev) {
        console_write("9P: No VirtIO 9P device found\n");
        return -1;
    }
    
    console_write("9P: Found VirtIO 9P device\n");
    
    // Initialize VirtIO device
    if (virtio_init(pci_dev, &p9_dev) != 0) {
        console_write("9P: VirtIO init failed\n");
        return -1;
    }
   
    // Initialize virtqueue
    virtqueue_init(&p9_dev);
    
    // Perform version handshake
    if (p9_version() != 0) {
        console_write("9P: Version handshake failed\n");
        return -1;
    }
    
    // Attach to filesystem
    if (p9_attach("root", "") < 0) {
        console_write("9P: Attach failed\n");
        return -1;
    }
    
    console_write("9P: Initialized successfully\n");
    return 0;
}

// 9P version handshake
int p9_version(void) {
    uint8_t *p = tx_buffer;
    
    // Leave space for size
    p += 4;
    
    // Type
    *p++ = P9_TVERSION;
    
    // Tag
    write_u16(p, P9_NOTAG);
    p += 2;
    
    // msize
    write_u32(p, P9_MAX_MSG_SIZE);
    p += 4;
    
    // version string
    p += write_string(p, "9P2000.L");
    
    // Write size
    uint32_t size = p - tx_buffer;
    write_u32(tx_buffer, size);
    
    // Send and receive
    uint32_t rx_len;
    if (p9_rpc(tx_buffer, size, rx_buffer, &rx_len) != 0) {
        return -1;
    }
    
    // Check response type
    if (rx_buffer[4] != P9_RVERSION) {
        console_write("9P: Unexpected response to Tversion\n");
        return -1;
    }
    
    console_write("9P: Version negotiated\n");
    return 0;
}

// Attach to filesystem
int p9_attach(const char *uname, const char *aname) {
    uint8_t *p = tx_buffer;
    p += 4;  // size
    *p++ = P9_TATTACH;
    write_u16(p, p9_tag++);
    p += 2;
    
    // fid
    uint32_t root_fid = 0;
    write_u32(p, root_fid);
    p += 4;
    
    // afid (no auth)
    write_u32(p, P9_NOFID);
    p += 4;
    
    // uname
    p += write_string(p, uname);
    
    // aname
    p += write_string(p, aname);
    
    uint32_t size = p - tx_buffer;
    write_u32(tx_buffer, size);
    
    uint32_t rx_len;
    if (p9_rpc(tx_buffer, size, rx_buffer, &rx_len) != 0) {
        return -1;
    }
    
    if (rx_buffer[4] != P9_RATTACH) {
        console_write("9P: Attach failed\n");
        return -1;
    }
    
    console_write("9P: Attached to filesystem\n");
    return root_fid;
}

// Walk to a file
int p9_walk(uint32_t fid, uint32_t newfid, const char *path) {
    // Simplified: just return newfid
    // Full implementation would split path and send walk elements
    return newfid;
}

// Open file
int p9_open(uint32_t fid, uint8_t mode) {
    // Simplified stub
    return 0;
}

// Read file
int p9_read(uint32_t fid, uint64_t offset, uint32_t count, void *data) {
    // Simplified stub - would send Tread message
    return 0;
}

// Close file
void p9_clunk(uint32_t fid) {
    // Simplified stub - would send Tclunk message
}
