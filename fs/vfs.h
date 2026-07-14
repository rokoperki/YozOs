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
#define VFS_ERR_IO -4
#define VFS_ERR_UNSUPPORTED -5

#define VFS_STAT_TYPE_FILE 1
#define VFS_STAT_TYPE_DIR 2

typedef struct {
  int used;
  char name[VFS_MAX_PATH];
  u32 size;
  u32 offset;
  u16 first_cluster;
  u32 flags;
  int readable;
  int writable;
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
int vfs_write(int handle, u8 *src, u32 len);
int vfs_lseek(int handle, u32 offset, u32 whence);

int vfs_open_at(const char *cwd, const char *path, u32 flags);
int vfs_stat_at(const char *cwd, const char *path, vfs_stat_t *out);

#endif
