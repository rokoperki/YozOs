#ifndef FS_H
#define FS_H

#include "../cpu/types.h"

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

void fs_init(void);
void fs_info(void);
void fs_ls(void);
void fs_cat(char *name);
void fs_create(char *name);
void fs_write(char *name, char *text);

#endif
