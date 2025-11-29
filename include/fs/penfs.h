#ifndef FS_PENFS_H
#define FS_PENFS_H

#include <stdint.h>

#define PENFS_MAX_FILENAME 32

int penfs_format(uint32_t sectors);
int penfs_list_directory(const char* path);
int penfs_create_file(const char* name);
int penfs_mkdir(const char* name);
void* penfs_read_file(const char* name, uint32_t* size);
int penfs_write_file(const char* name, const char* content, uint32_t len);
int penfs_delete_file(const char* name);

#endif
