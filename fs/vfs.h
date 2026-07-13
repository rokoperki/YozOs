#ifndef VFS_H
#define VFS_H

#include "../cpu/types.h"

#define VFS_MAX_OPEN_FILES 16
#define VFS_MAX_PATH 64
#define VFS_MAX_NAME 13
#define VFS_MAX_FILE_BYTES 8192

#define VFS_ERR_INVALID -1
#define VFS_ERR_NOT_FOUND -2
#define VFS_ERR_NO_SPACE -3

#define VFS_STAT_TYPE_FILE 1

typedef struct {
  int used;
  char name[VFS_MAX_NAME];
  u32 size;
  u32 offset;
  u16 first_cluster;
} vfs_file_t;

typedef struct {
  u32 size;
  u32 type;
} vfs_stat_t;

void vfs_init(void);
int vfs_open(const char *path, u32 flags);
int vfs_read(int handle, u8 *dst, u32 len);
int vfs_close(int handle);
int vfs_stat(const char *path, vfs_stat_t *out);

#endif
