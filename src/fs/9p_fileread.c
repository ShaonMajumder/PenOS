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
