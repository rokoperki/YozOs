#include "fat.h"
#include "../cpu/types.h"
#include "../drivers/ata.h"

#include "../drivers/screen.h"
#include "../kernel/string.h"

static u32 fat_start, root_start, root_sectors, data_start;
static u8 sec_per_clus;

static char to_upper(char c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

void name_to_83(const char *in, char out[11]) {
  for (int i = 0; i < 11; i++)
    out[i] = ' ';

  int o = 0;
  for (int i = 0; in[i] && in[i] != '.' && o < 8; i++, o++)
    out[o] = to_upper(in[i]);

  const char *ext = 0;
  for (int i = 0; in[i]; i++)
    if (in[i] == '.') {
      ext = &in[i + 1];
      break;
    }

  if (ext)
    for (int i = 0; ext[i] && i < 3; i++)
      out[8 + i] = to_upper(ext[i]);
}

void name_from_83(const char in[11], char *out) {
  int o = 0;
  for (int i = 0; i < 8 && in[i] != ' '; i++)
    out[o++] = in[i];

  if (in[8] != ' ') {
    out[o++] = '.';
    for (int i = 8; i < 11 && in[i] != ' '; i++)
      out[o++] = in[i];
  }
  out[o] = '\0';
}

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

void fs_ls() {
  u16 buff[256];
  for (u32 s = 0; s < root_sectors; s++) {
    ata_read(root_start + s, 1, buff);
    dir_entry_t *e = (dir_entry_t *)buff;

    for (int i = 0; i < 16; i++) {
      if (e[i].name[0] == 0x00)
        return; // end of dir
      if (e[i].name[0] == 0xE5)
        continue; // deleted
      if (e[i].attr == 0x0F)
        continue; // lonf-filename fragment
      if (e[i].attr & 0x08)
        continue; // volume label
      if (e[i].attr & 0x02)
        continue; // macOS ._ sidecars .fseventsd

      char nm[13];
      name_from_83((char *)e[i].name, nm);
      print(nm);
      print("  ");
      int_to_ascii(e[i].size, nm);
      println(nm);
    }
  }
}

u16 fat_entry(u32 cluster) {
  u32 off = cluster * 2;
  u16 buff[256];
  ata_read((fat_start + off / 512), 1, buff);
  return buff[(off % 512) / 2];
}

void fs_cat(char *name) {
  char target[11];
  name_to_83(name, target);

  u16 first_cluster = 0;
  u32 size = 0;
  int found = 0;

  for (u32 s = 0; s < root_sectors && !found; s++) {
    u16 buff[256];
    ata_read(root_start + s, 1, buff);
    dir_entry_t *e = (dir_entry_t *)buff;
    for (int i = 0; i < 16; i++) {
      if (e[i].name[0] == 0x00) { // end of directory
        s = root_sectors;         // stop the outer loop too
        break;
      }
      int match = 1;
      for (int k = 0; k < 11; k++)
        if (e[i].name[k] != target[k]) {
          match = 0;
          break;
        }
      if (match) {
        first_cluster = e[i].first_cluster;
        size = e[i].size;
        found = 1;
        break;
      }
    }
  }

  if (!found) {
    println("file not found");
    return;
  }

  u16 cluster = first_cluster;
  u32 remaining = size;
  u16 buff[256]; // one sector

  while (cluster < 0xFFF8 && remaining > 0) {
    u32 lba = data_start + (cluster - 2) * sec_per_clus;
    for (u8 sc = 0; sc < sec_per_clus && remaining > 0; sc++) {
      ata_read(lba + sc, 1, buff);
      char *bytes = (char *)buff;
      u32 n = remaining < 512 ? remaining : 512;
      for (u32 j = 0; j < n; j++)
        print_char(bytes[j], -1, -1, 0);
      remaining -= n;
    }
    cluster = fat_entry(cluster);
  }
  println("");
}
