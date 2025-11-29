#ifndef FS_FS_H
#define FS_FS_H

#include <stdint.h>
#include <stddef.h>

// Simple in-memory filesystem API
// This provides a basic interface for the shell

const char* fs_find(const char* filename);
size_t fs_file_count(void);
const char* fs_file_name(size_t index);
size_t fs_file_size(size_t index);

// Directory operations
const char* fs_getcwd(void);
int fs_chdir(const char* path);

void fs_init(void);

#endif
