#include "fat.h"
#include "../cpu/types.h"
#include "../drivers/ata.h"

#include "../drivers/screen.h"
#include "../kernel/string.h"

static u32 fat_start, root_start, root_sectors, data_start;
static u8 sec_per_clus;

void fs_init() {
  u16 buff[256];
  ata_read(0, 1, buff);
  bpb_t *bpb = (bpb_t *)buff;

  fat_start = bpb->reserved_sectors;
  root_start = fat_start + (bpb->num_fats * bpb->sectors_per_fat);
  root_sectors = (bpb->root_entries * 32) / bpb->bytes_per_sector;
  data_start = root_start + root_sectors;
  sec_per_clus = bpb->sectors_per_cluster;
}

void fs_info() {
  char b[16];
  print("fat_start=");
  int_to_ascii(fat_start, b);
  println(b);
  print("root_start=");
  int_to_ascii(root_start, b);
  println(b);
  print("data_start=");
  int_to_ascii(data_start, b);
  println(b);
  print("sec/clus=");
  int_to_ascii(sec_per_clus, b);
  println(b);
}
