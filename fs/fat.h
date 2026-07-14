#ifndef FS_H
#define FS_H

#include "../cpu/types.h"

#define FAT_OK 0
#define FAT_ERR_INVALID -1
#define FAT_ERR_EXISTS -2
#define FAT_ERR_NOT_FOUND -3
#define FAT_ERR_NO_SPACE -4
#define FAT_ERR_TOO_BIG -5

typedef struct {
  u8 jump[3];             // 0x00
  u8 oem[8];              // 0x03
  u16 bytes_per_sector;   // 0x0B
  u8 sectors_per_cluster; // 0x0D
  u16 reserved_sectors;   // 0x0E
  u8 num_fats;            // 0x10
  u16 root_entries;       // 0x11
  u16 total_sectors;      // 0x13
  u8 media_descriptor;    // 0x15
  u16 sectors_per_fat;    // 0x16
} __attribute__((packed)) bpb_t;

typedef struct {
  u8 name[11];                         // 0x00 - 8name 3ext
  u8 attr;                             // 0x0B
  u8 reserved[14];                     // 0x0C
  u16 first_cluster;                   // 0x1A
  u32 size;                            // 0x1C
} __attribute__((packed)) dir_entry_t; // 32B

typedef struct {
  u32 size;
  u16 first_cluster;
} fat_file_info_t;

void fs_init(void);
void fs_info(void);
void fs_ls(void);
int fat_read_file(char *name, u8 *dst, u32 max_len, u32 *out_len);
void fs_cat(char *name);
void fs_create(char *name);
void fs_write(char *name, char *text);
void fs_delete(char *name);
void fs_append(char *name, char *text);
void fs_rename(char *name, char *new_name);

int fat_stat_file(char *name, fat_file_info_t *out);
int fat_read_file_at(char *name, u32 offset, u8 *dst, u32 len, u32 *out_len);
int fat_create_file(char *name);
int fat_write_file(char *name, u8 *src, u32 len);
int fat_write_file_at(char *name, u32 offset, u8 *src, u32 len);
int fat_append_file(char *name, u8 *src, u32 len);

#endif
