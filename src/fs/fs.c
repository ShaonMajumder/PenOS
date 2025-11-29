#include <fs/fs.h>
#include <fs/9p.h>
#include <string.h>
#include <ui/console.h>

// Simple in-memory filesystem with a few demo files
// This is a placeholder until VirtIO-9p is fully implemented

typedef struct {
    const char* name;
    const char* content;
    size_t size;
} fs_file_t;

static fs_file_t files[] = {
    {"readme.txt", "Welcome to PenOS!\nThis is a minimal 32-bit OS.\n", 50},
    {"hello.txt", "Hello from the filesystem!\n", 28},
    {"test.txt", "Testing 123\n", 12},
};

static const size_t file_count = sizeof(files) / sizeof(files[0]);

// Current working directory (simple implementation)
static char current_dir[256] = "/";

void fs_init(void) {
    // Try to initialize 9p first
    if (p9_init() == 0) {
        console_write("FS: Using VirtIO-9p filesystem\n");
        // TODO: Replace file list with 9p backend
    } else {
        console_write("FS: Using in-memory filesystem with ");
        console_write_dec((uint32_t)file_count);
        console_write(" file(s)\n");
    }
}

const char* fs_find(const char* filename) {
    for (size_t i = 0; i < file_count; i++) {
        if (strcmp(files[i].name, filename) == 0) {
            return files[i].content;
        }
    }
    return NULL;
}

size_t fs_file_count(void) {
    return file_count;
}

const char* fs_file_name(size_t index) {
    if (index >= file_count) {
        return NULL;
    }
    return files[index].name;
}

size_t fs_file_size(size_t index) {
    if (index >= file_count) {
        return 0;
    }
    return files[index].size;
}

const char* fs_getcwd(void) {
    return current_dir;
}

int fs_chdir(const char* path) {
    // Simple implementation: only support root and "." for now
    if (strcmp(path, "/") == 0 || strcmp(path, ".") == 0) {
        current_dir[0] = '/';
        current_dir[1] = '\0';
        return 0;
    }
    
    // For now, reject any other path since we don't have real directories
    return -1;
}
