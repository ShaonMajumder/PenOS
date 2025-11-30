#include <fs/9p.h>
#include <drivers/virtio.h>
#include <drivers/pci.h>
#include <ui/console.h>
#include <mem/heap.h>
#include <string.h>

static virtio_device_t p9_dev;
static uint16_t p9_tag = 0;
static uint32_t next_fid = 1;

static char p9_cwd[256] = "/";  // Current working directory
static int p9_initialized = 0;

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
    // Create descriptor chain
    virtio_buf_desc_t bufs[2];
    
    // TX buffer (device-readable)
    bufs[0].buf = tx;
    bufs[0].len = tx_len;
    bufs[0].write = 0;
    
    // RX buffer (device-writable)
    bufs[1].buf = rx;
    bufs[1].len = P9_MAX_MSG_SIZE;
    bufs[1].write = 1;
    
    // Add chain to virtqueue
    int idx = virtqueue_add_chain(&p9_dev.vq, bufs, 2);
    if (idx < 0) {
        return -1;
    }
    
    // Kick device
    virtqueue_kick(&p9_dev);
    
    // Wait for response
    for (int i = 0; i < 10000000; i++) {
        uint32_t len;
        int ret_idx = virtqueue_get_buf(&p9_dev.vq, &len);
        if (ret_idx >= 0) {
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
    p9_initialized = 1;
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
    
    // n_uname (numeric ID) - required for 9P2000.L
    write_u32(p, 0);  // 0 for root
    p += 4;
    
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
    uint8_t *p = tx_buffer;
    p += 4;
    *p++ = P9_TWALK;
    write_u16(p, p9_tag++);
    p += 2;
    
    write_u32(p, fid);
    p += 4;
    write_u32(p, newfid);
    p += 4;
    
    // Split path by '/' and count components
    if (!path || *path == '\0') {
        // Clone fid
        write_u16(p, 0);
        p += 2;
    } else {
        // Count and parse path components
        char path_copy[256];
        strcpy(path_copy, path);
        
        // Count components
        uint16_t nwname = 0;
        char *components[32];
        char *token = path_copy;
        
        while (*token) {
            // Skip leading slashes
            while (*token == '/') token++;
            if (*token == '\0') break;
            
            components[nwname++] = token;
            
            // Find end of component
            while (*token && *token != '/') token++;
            if (*token == '/') {
                *token = '\0';
                token++;
            }
        }
        
        write_u16(p, nwname);
        p += 2;
        
        // Write each component as a string
        for (int i = 0; i < nwname; i++) {
            p += write_string(p, components[i]);
        }
    }
    
    uint32_t size = p - tx_buffer;
    write_u32(tx_buffer, size);
    
    uint32_t rx_len;
    if (p9_rpc(tx_buffer, size, rx_buffer, &rx_len) != 0) {
        return -1;
    }
    
    if (rx_buffer[4] != P9_RWALK) {
        return -1;
    }
    
    return 0;
}

// Open file (LOPEN for 9P2000.L)
int p9_open(uint32_t fid, uint32_t flags) {
    uint8_t *p = tx_buffer;
    p += 4;
    *p++ = P9_TLOPEN;
    write_u16(p, p9_tag++);
    p += 2;
    
    write_u32(p, fid);
    p += 4;
    write_u32(p, flags);
    p += 4;
    
    uint32_t size = p - tx_buffer;
    write_u32(tx_buffer, size);
    
    uint32_t rx_len;
    if (p9_rpc(tx_buffer, size, rx_buffer, &rx_len) != 0) {
        return -1;
    }
    
    if (rx_buffer[4] != P9_RLOPEN) {
        return -1;
    }
    
    return 0;
}

// Read from file (READ for 9P2000.L)
int p9_read(uint32_t fid, uint64_t offset, uint32_t count, void *data) {
    uint8_t *p = tx_buffer;
    p += 4;
    *p++ = P9_TREAD;
    write_u16(p, p9_tag++);
    p += 2;
    
    write_u32(p, fid);
    p += 4;
    
    // offset (64-bit)
    write_u32(p, (uint32_t)offset);
    p += 4;
    write_u32(p, (uint32_t)(offset >> 32));
    p += 4;
    
    write_u32(p, count);
    p += 4;
    
    uint32_t size = p - tx_buffer;
    write_u32(tx_buffer, size);
    
    uint32_t rx_len;
    if (p9_rpc(tx_buffer, size, rx_buffer, &rx_len) != 0) {
        return -1;
    }
    
    if (rx_buffer[4] != P9_RREAD) {
        return -1;
    }
    
    // Copy data
    uint32_t count_rx = read_u32(rx_buffer + 7);
    if (count_rx > count) count_rx = count;
    
    memcpy(data, rx_buffer + 11, count_rx);
    return count_rx;
}

// Read directory (READDIR for 9P2000.L)
int p9_readdir(uint32_t fid, uint64_t offset, uint32_t count, void *data) {
    uint8_t *p = tx_buffer;
    p += 4;
    *p++ = P9_TREADDIR;
    write_u16(p, p9_tag++);
    p += 2;
    
    write_u32(p, fid);
    p += 4;
    
    // offset (64-bit)
    write_u32(p, (uint32_t)offset);
    p += 4;
    write_u32(p, (uint32_t)(offset >> 32));
    p += 4;
    
    write_u32(p, count);
    p += 4;
    
    uint32_t size = p - tx_buffer;
    write_u32(tx_buffer, size);
    
    uint32_t rx_len;
    if (p9_rpc(tx_buffer, size, rx_buffer, &rx_len) != 0) {
        return -1;
    }
    
    if (rx_buffer[4] != P9_RREADDIR) {
        return -1;
    }
    
    // Copy data
    uint32_t count_rx = read_u32(rx_buffer + 7);
    if (count_rx > count) count_rx = count;
    
    memcpy(data, rx_buffer + 11, count_rx);
    return count_rx;
}

// Close file
void p9_clunk(uint32_t fid) {
    uint8_t *p = tx_buffer;
    p += 4;
    *p++ = P9_TCLUNK;
    write_u16(p, p9_tag++);
    p += 2;
    
    write_u32(p, fid);
    p += 4;
    
    uint32_t size = p - tx_buffer;
    write_u32(tx_buffer, size);
    
    p9_rpc(tx_buffer, size, rx_buffer, NULL);
}

// Get current working directory
const char* p9_getcwd(void) {
    return p9_cwd;
}

// Change directory
int p9_change_directory(const char *path) {
    if (!p9_initialized) {
        return -1;
    }
    
    if (strcmp(path, ".") == 0) {
        return 0;
    }
    
    char new_path[256];
    
    if (strcmp(path, "..") == 0) {
        if (strcmp(p9_cwd, "/") == 0) {
            return 0;
        }
        
        int len = strlen(p9_cwd);
        int i;
        for (i = len - 1; i >= 0; i--) {
            if (p9_cwd[i] == '/') {
                break;
            }
        }
        
        if (i == 0) {
            strcpy(new_path, "/");
        } else {
            memcpy(new_path, p9_cwd, i);
            new_path[i] = '\0';
        }
    } else if (path[0] == '/') {
        strcpy(new_path, path);
    } else {
        if (strcmp(p9_cwd, "/") == 0) {
            strcpy(new_path, "/");
            strcat(new_path, path);
        } else {
            strcpy(new_path, p9_cwd);
            strcat(new_path, "/");
            strcat(new_path, path);
        }
    }
    
    // Verify directory exists
    uint32_t test_fid = next_fid++;
    
    // Remove leading slash for walk
    const char *walk_path = new_path;
    if (walk_path[0] == '/') {
        walk_path++;
    }
    
    if (p9_walk(0, test_fid, walk_path) != 0) {
        return -1;
    }
    
    if (p9_open(test_fid, 0) != 0) {
        p9_clunk(test_fid);
        return -1;
    }
    
    p9_clunk(test_fid);
    strcpy(p9_cwd, new_path);
    
    return 0;
}

// List directory
int p9_list_directory(const char *path) {
    if (!p9_initialized) {
        return -1;
    }

    const char *target = path ? path : p9_cwd;
    uint32_t fid = next_fid++;
    
    // Remove leading slash for walk
    const char *walk_path = target;
    if (walk_path[0] == '/') {
        walk_path++;
    }
    
    if (p9_walk(0, fid, walk_path) != 0) {
        return -1;
    }
    
    if (p9_open(fid, 0) != 0) {
        p9_clunk(fid);
        return -1;
    }
    
    // Read directory
    uint8_t buf[4096];
    int count = p9_readdir(fid, 0, sizeof(buf), buf);
    
    if (count < 0) {
        p9_clunk(fid);
        return -1;
    }
    
    if (count > 0) {
        int offset = 0;
        while (offset < count) {
            if (offset + 24 > count) break;
            
            uint8_t type = buf[offset + 21];
            uint16_t name_len = buf[offset + 22] | (buf[offset + 23] << 8);
            
            if (offset + 24 + name_len > count) break;
            
            char name[256];
            if (name_len > 255) name_len = 255;
            memcpy(name, buf + offset + 24, name_len);
            name[name_len] = '\0';
            
            console_write(name);
            if (type == 4) {
                console_write("/");
            }
            console_write("\n");
            
            offset += 24 + name_len;
        }
    }
    
    p9_clunk(fid);
    return 0;
}

// Helper: write 64-bit value
static void write_u64(uint8_t *buf, uint64_t val) {
    write_u32(buf, (uint32_t)(val & 0xFFFFFFFF));
    write_u32(buf + 4, (uint32_t)(val >> 32));
}

// Helper: read 64-bit value
static uint64_t read_u64(const uint8_t *buf) {
    uint64_t low = read_u32(buf);
    uint64_t high = read_u32(buf + 4);
    return low | (high << 32);
}

// Get file size using getattr
int p9_get_file_size(uint32_t fid, uint64_t *size)
{
    // Build TGETATTR message
    uint32_t offset = 0;
    write_u32(tx_buffer + offset, 0); // size (fill later)
    offset += 4;
    tx_buffer[offset++] = P9_TGETATTR;
    write_u16(tx_buffer + offset, p9_tag++);
    offset += 2;
    write_u32(tx_buffer + offset, fid);
    offset += 4;
    write_u64(tx_buffer + offset, 0x00000FFFULL); // request_mask (all attrs)
    offset += 8;
    
    write_u32(tx_buffer, offset); // Update size
    
    uint32_t rx_len = P9_MAX_MSG_SIZE;
    if (p9_rpc(tx_buffer, offset, rx_buffer, &rx_len) != 0) {
        return -1;
    }
    
    // Parse RGETATTR response
    // Skip: size(4) + type(1) + tag(2) + qid(13) + valid(8) + mode(4) + uid(4) + gid(4) + nlink(8) + rdev(8) + size(8)
    uint32_t resp_offset = 4 + 1 + 2 + 13 + 8 + 4 + 4 + 4 + 8 + 8;
    *size = read_u64(rx_buffer + resp_offset);
    
    return 0;
}

// Read entire file into buffer
int p9_read_file(const char *path, void **buffer, uint32_t *size)
{
    if (!p9_initialized) {
        return -1;
    }
    
    // Walk to file
    uint32_t file_fid = next_fid++;
    if (p9_walk(1, file_fid, path) != 0) {
        console_write("[9P] Failed to walk to file: ");
        console_write(path);
        console_write("\n");
        return -1;
    }
    
    // Open file for reading
    if (p9_open(file_fid, 0) != 0) { // O_RDONLY = 0
        console_write("[9P] Failed to open file\n");
        p9_clunk(file_fid);
        return -1;
    }
    
    // Get file size
    uint64_t file_size;
    if (p9_get_file_size(file_fid, &file_size) != 0) {
        console_write("[9P] Failed to get file size\n");
        p9_clunk(file_fid);
        return -1;
    }
    
    if (file_size == 0 || file_size > 10 * 1024 * 1024) { // Max 10MB
        console_write("[9P] Invalid file size\n");
        p9_clunk(file_fid);
        return -1;
    }
    
    // Allocate buffer
    void *data = kmalloc((uint32_t)file_size);
    if (!data) {
        console_write("[9P] Failed to allocate buffer\n");
        p9_clunk(file_fid);
        return -1;
    }
    
    // Read file in chunks
    uint32_t bytes_read = 0;
    uint32_t chunk_size = 4096; // Read 4KB at a time
    
    while (bytes_read < file_size) {
        uint32_t to_read = (file_size - bytes_read > chunk_size) ? chunk_size : (file_size - bytes_read);
        
        int result = p9_read(file_fid, bytes_read, to_read, (uint8_t*)data + bytes_read);
        if (result <= 0) {
            break;
        }
        
        bytes_read += result;
    }
    
    p9_clunk(file_fid);
    
    if (bytes_read != file_size) {
        console_write("[9P] Failed to read entire file\n");
        kfree(data);
        return -1;
    }
    
    *buffer = data;
    *size = (uint32_t)file_size;
    
    return 0;
}
