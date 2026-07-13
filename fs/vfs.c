#include "vfs.h"
#include "../kernel/mem.h"
#include "../kernel/string.h"
#include "fat.h"

static vfs_file_t open_files[VFS_MAX_OPEN_FILES];

void vfs_init(void) { memory_set((u8 *)open_files, 0, sizeof(open_files)); }

static int normalize_path(const char *path, char *out) {
  int i = 0;
  int o = 0;

  if (!path || !path[0])
    return 0;

  if (path[0] == '/')
    i = 1;

  if (!path[i])
    return 0;

  for (; path[i]; i++) {
    if (path[i] == '/')
      return 0;

    if (o >= VFS_MAX_NAME - 1)
      return 0;

    out[o++] = path[i];
  }

  out[o] = '\0';
  return 1;
}

static int find_free_handle(void) {
  for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
    if (!open_files[i].used)
      return i;
  }

  return -1;
}

int vfs_open(const char *path) {
  char name[VFS_MAX_NAME];
  fat_file_info_t info;

  if (!normalize_path(path, name))
    return VFS_ERR_INVALID;

  int h = find_free_handle();
  if (h < 0)
    return VFS_ERR_NO_SPACE;

  if (!fat_stat_file(name, &info))
    return VFS_ERR_NOT_FOUND;

  open_files[h].used = 1;
  open_files[h].offset = 0;
  open_files[h].size = info.size;
  open_files[h].first_cluster = info.first_cluster;

  for (int i = 0; i < VFS_MAX_NAME; i++) {
    open_files[h].name[i] = name[i];
  }

  return h;
}

int vfs_read(int handle, u8 *dst, u32 len) {
  if (handle < 0 || handle >= VFS_MAX_OPEN_FILES)
    return VFS_ERR_INVALID;

  if (!dst && len > 0)
    return VFS_ERR_INVALID;

  vfs_file_t *f = &open_files[handle];

  if (!f->used)
    return VFS_ERR_INVALID;

  if (f->offset >= f->size)
    return 0;

  u32 read_len = 0;

  if (!fat_read_file_at(f->name, f->offset, dst, len, &read_len))
    return VFS_ERR_NOT_FOUND;

  f->offset += read_len;
  return read_len;
}

int vfs_close(int handle) {
  if (handle < 0 || handle >= VFS_MAX_OPEN_FILES)
    return VFS_ERR_INVALID;

  open_files[handle].used = 0;
  return 0;
}
