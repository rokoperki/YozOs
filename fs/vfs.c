#include "vfs.h"
#include "../cpu/user_abi.h"
#include "../kernel/mem.h"
#include "../kernel/string.h"
#include "fat.h"

static vfs_file_t open_files[VFS_MAX_OPEN_FILES];

static int flags_readable(u32 flags) {
  return flags == USER_O_RDONLY || (flags & USER_O_RDWR);
}

static int flags_writable(u32 flags) {
  return (flags & USER_O_WRONLY) || (flags & USER_O_RDWR);
}

static int vfs_flags_valid(u32 flags) {
  u32 mode = flags & USER_O_ACCMODE;
  u32 known = USER_O_ACCMODE | USER_O_CREATE | USER_O_TRUNC | USER_O_APPEND;

  if (flags & ~known)
    return 0;

  if (mode != USER_O_RDONLY && mode != USER_O_WRONLY && mode != USER_O_RDWR)
    return 0;

  if ((flags & USER_O_TRUNC) && (flags & USER_O_APPEND))
    return 0;

  if ((flags & USER_O_TRUNC) && mode == USER_O_RDONLY)
    return 0;

  if ((flags & USER_O_APPEND) && mode == USER_O_RDONLY)
    return 0;

  return 1;
}

void vfs_init(void) { memory_set((u8 *)open_files, 0, sizeof(open_files)); }

static int resolve_root_path(const char *cwd, const char *path, char *out) {
  int i = 0;
  int o = 0;

  if (!cwd || cwd[0] != '/' || cwd[1] != '\0')
    return 0;

  if (!path || !path[0])
    return 0;

  if (path[0] == '/') {
    i = 1;
  } else {
    while (path[i] == '.' && path[i + 1] == '/')
      i += 2;
  }

  if (!path[i])
    return 0;

  if (path[i] == '.' && !path[i + 1])
    return 0;

  if (path[i] == '.' && path[i + 1] == '.')
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

static int is_root_path(const char *cwd, const char *path) {
  if (!cwd || cwd[0] != '/' || cwd[1] != '\0')
    return 0;

  if (!path || !path[0])
    return 0;

  return ((path[0] == '/' && path[1] == '\0') ||
          (path[0] == '.' && path[1] == '\0'));
}

static int find_free_handle(void) {
  for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
    if (!open_files[i].used)
      return i;
  }

  return -1;
}

int vfs_open_at(const char *cwd, const char *path, u32 flags) {
  char name[VFS_MAX_NAME];
  fat_file_info_t info;

  if (!vfs_flags_valid(flags))
    return VFS_ERR_INVALID;

  if (!resolve_root_path(cwd, path, name))
    return VFS_ERR_INVALID;

  int h = find_free_handle();
  if (h < 0)
    return VFS_ERR_NO_SPACE;

  int exists = fat_stat_file(name, &info);

  if (!exists) {
    if (!(flags & USER_O_CREATE))
      return VFS_ERR_NOT_FOUND;

    int ret = fat_create_file(name);
    if (ret < 0 && ret != FAT_ERR_EXISTS)
      return VFS_ERR_INVALID;

    exists = fat_stat_file(name, &info);
    if (!exists)
      return VFS_ERR_NOT_FOUND;
  }

  if (info.attr & 0x10)
    return VFS_ERR_UNSUPPORTED;

  if (flags & USER_O_TRUNC) {
    int ret = fat_write_file(name, 0, 0);
    if (ret < 0)
      return VFS_ERR_IO;
    fat_stat_file(name, &info);
  }

  open_files[h].used = 1;
  if (flags & USER_O_APPEND)
    open_files[h].offset = info.size;
  else
    open_files[h].offset = 0;
  open_files[h].size = info.size;
  open_files[h].first_cluster = info.first_cluster;
  open_files[h].flags = flags;
  open_files[h].readable = flags_readable(flags);
  open_files[h].writable = flags_writable(flags);

  for (int i = 0; i < VFS_MAX_NAME; i++) {
    open_files[h].name[i] = name[i];
  }

  return h;
}

int vfs_open(const char *path, u32 flags) {
  return vfs_open_at("/", path, flags);
}

int vfs_read(int handle, u8 *dst, u32 len) {
  if (handle < 0 || handle >= VFS_MAX_OPEN_FILES)
    return VFS_ERR_INVALID;

  if (!dst && len > 0)
    return VFS_ERR_INVALID;

  vfs_file_t *f = &open_files[handle];

  if (!f->used)
    return VFS_ERR_INVALID;

  if (!f->readable)
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

int vfs_stat_at(const char *cwd, const char *path, vfs_stat_t *out) {
  char name[VFS_MAX_NAME];
  fat_file_info_t info;

  if (!out)
    return VFS_ERR_INVALID;

  out->size = 0;
  out->type = 0;

  if (is_root_path(cwd, path)) {
    out->size = 0;
    out->type = VFS_STAT_TYPE_DIR;
    return 0;
  }

  if (!resolve_root_path(cwd, path, name))
    return VFS_ERR_INVALID;

  if (!fat_stat_file(name, &info))
    return VFS_ERR_NOT_FOUND;

  out->size = info.size;

  if (info.attr & 0x10)
    out->type = VFS_STAT_TYPE_DIR;
  else
    out->type = VFS_STAT_TYPE_FILE;

  return 0;
}

int vfs_stat(const char *path, vfs_stat_t *out) {
  return vfs_stat_at("/", path, out);
}

int vfs_write(int handle, u8 *src, u32 len) {
  if (handle < 0 || handle >= VFS_MAX_OPEN_FILES)
    return VFS_ERR_INVALID;

  if (!src && len > 0)
    return VFS_ERR_INVALID;

  vfs_file_t *f = &open_files[handle];

  if (!f->used)
    return VFS_ERR_INVALID;

  if (!f->writable)
    return VFS_ERR_INVALID;

  int ret;

  if (f->flags & USER_O_APPEND) {
    ret = fat_append_file(f->name, src, len);
  } else {
    ret = fat_write_file_at(f->name, f->offset, src, len);
  }

  if (ret != FAT_OK)
    return VFS_ERR_IO;

  fat_file_info_t info;

  if (fat_stat_file(f->name, &info)) {
    f->size = info.size;
    f->first_cluster = info.first_cluster;
  }

  if (f->flags & USER_O_APPEND)
    f->offset = f->size;
  else
    f->offset += len;
  return len;
}

int vfs_lseek(int handle, u32 offset, u32 whence) {
  if (handle < 0 || handle >= VFS_MAX_OPEN_FILES)
    return VFS_ERR_INVALID;

  vfs_file_t *f = &open_files[handle];

  if (!f->used)
    return VFS_ERR_INVALID;

  u32 base;

  if (whence == USER_SEEK_SET) {
    base = 0;
  } else if (whence == USER_SEEK_CUR) {
    base = f->offset;
  } else if (whence == USER_SEEK_END) {
    base = f->size;
  } else {
    return VFS_ERR_INVALID;
  }

  u32 new_offset = base + offset;
  if (new_offset < base)
    return VFS_ERR_INVALID;

  f->offset = new_offset;
  return f->offset;
}
