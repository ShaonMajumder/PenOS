#include <fs/penfs.h>
#include <ui/console.h>
#include <string.h>

int penfs_format(uint32_t sectors) {
    (void)sectors;
    console_write("PenFS: Format not implemented (dummy)\n");
    return 0;
}

int penfs_list_directory(const char* path) {
    (void)path;
    console_write("PenFS: List directory not implemented (dummy)\n");
    return 0;
}

int penfs_create_file(const char* name) {
    (void)name;
    console_write("PenFS: Create file not implemented (dummy)\n");
    return 0;
}

int penfs_mkdir(const char* name) {
    (void)name;
    console_write("PenFS: Mkdir not implemented (dummy)\n");
    return 0;
}

void* penfs_read_file(const char* name, uint32_t* size) {
    (void)name;
    if (size) *size = 0;
    console_write("PenFS: Read file not implemented (dummy)\n");
    return NULL;
}

int penfs_write_file(const char* name, const char* content, uint32_t len) {
    (void)name;
    (void)content;
    (void)len;
    console_write("PenFS: Write file not implemented (dummy)\n");
    return 0;
}

int penfs_delete_file(const char* name) {
    (void)name;
    console_write("PenFS: Delete file not implemented (dummy)\n");
    return 0;
}
