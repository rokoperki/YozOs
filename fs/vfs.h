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

typedef struct {
  int used;
  char name[VFS_MAX_NAME];
  u32 size;
  u32 offset;
} vfs_file_t;

void vfs_init(void);
int vfs_open(const char *path);
int vfs_read(int handle, u8 *dst, u32 len);
int vfs_close(int handle);

#endif
