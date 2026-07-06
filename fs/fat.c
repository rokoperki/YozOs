#include "fat.h"
#include "../cpu/types.h"
#include "../drivers/ata.h"
#include "../kernel/mem.h"

#include "../drivers/screen.h"
#include "../kernel/string.h"

static u32 fat_start, root_start, root_sectors, data_start, sectors_per_fat;
static u8 sec_per_clus, num_fats;

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
  num_fats = bpb->num_fats;
  sectors_per_fat = bpb->sectors_per_fat;
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
  print("num fats=");
  int_to_ascii(num_fats, b);
  println(b);
  print("sectors per fat=");
  int_to_ascii(sectors_per_fat, b);
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

void fat_set_entry(u32 cluster, u16 value) {
  u32 off = cluster * 2;
  u16 buff[256];

  for (int k = 0; k < num_fats; k++) {
    u32 lba = fat_start + k * sectors_per_fat + off / 512;
    ata_read(lba, 1, buff);
    buff[(off % 512) / 2] = value;
    ata_write(lba, 1, buff);
  }
}

u32 fat_find_free() {
  u32 total_slots = sectors_per_fat * 512 / 2;

  for (u32 c = 2; c < total_slots; c++) {
    if (fat_entry(c) == 0)
      return c;
  }

  return 0;
}

int find_dir_free_slot(u32 *out_lba, int *out_index) {
  u16 buff[256];

  for (u32 s = 0; s < root_sectors; s++) {
    u32 lba = root_start + s;
    ata_read(lba, 1, buff);
    dir_entry_t *e = (dir_entry_t *)buff;

    for (int i = 0; i < 16; i++) {
      u8 first = e[i].name[0];

      if (first == 0x00 || first == 0xE5) {
        *out_lba = lba;
        *out_index = i;
        return 1;
      }
    }
  }

  return 0;
}

void dir_write_entry(u32 lba, int index, char name83[11], u16 first_cluster,
                     u32 size, u8 attr) {

  u16 buff[256];
  ata_read(lba, 1, buff);
  dir_entry_t *e = (dir_entry_t *)buff;

  dir_entry_t *slot = &e[index];

  memory_set((u8 *)slot, 0, sizeof(dir_entry_t));
  memory_copy(name83, (char *)slot->name, 11);
  slot->attr = attr;
  slot->first_cluster = first_cluster;
  slot->size = size;
  ata_write(lba, 1, buff);
}

void fs_create(char *name) {
  char n83[11];
  name_to_83(name, n83);

  u32 lba;
  int idx;

  if (!find_dir_free_slot(&lba, &idx)) {
    println("dir full");
    return;
  }

  dir_write_entry(lba, idx, n83, 0, 0, 0x20);
  println("created");
}

int dir_find(char name83[11], u32 *out_lba, int *out_idx) {
  u16 buff[256];
  for (u32 s = 0; s < root_sectors; s++) {
    u32 lba = root_start + s;
    ata_read(lba, 1, buff);
    dir_entry_t *e = (dir_entry_t *)buff;
    for (int i = 0; i < 16; i++) {
      if (e[i].name[0] == 0x00)
        return 0; // end of dir → not found
      if (e[i].name[0] == 0xE5)
        continue; // deleted → skip

      int match = 1;
      for (int k = 0; k < 11; k++)
        if (e[i].name[k] != name83[k]) {
          match = 0;
          break;
        }
      if (match) {
        *out_lba = lba;
        *out_idx = i;
        return 1;
      }
    }
  }
  return 0;
}

void fs_write(char *name, char *text) {
  char n83[11];
  name_to_83(name, n83);

  u32 dir_lba;
  int idx;

  if (!dir_find(n83, &dir_lba, &idx)) {
    println("no such file");
    return;
  }

  u32 len = strlen(text);
  if (len > 8192) { // v1: must fit in one cluster
    println("too big for v1");
    return;
  }

  u16 buff[256];
  ata_read(dir_lba, 1, buff);
  dir_entry_t *e = (dir_entry_t *)buff;

  u16 old_clus = e[idx].first_cluster;

  if (len == 0) {
    while (old_clus >= 2 && old_clus < 0xFFF8) {
      u16 next = fat_entry(old_clus);
      fat_set_entry(old_clus, 0x0000);
      old_clus = next;
    }

    dir_write_entry(dir_lba, idx, n83, 0, 0, 0x20);
    println("written");
    return;
  }

  u32 cluster_bytes = sec_per_clus * 512;
  u32 needed = (len + cluster_bytes - 1) / cluster_bytes;

  u16 chain[4];

  for (u32 i = 0; i < needed; i++) {
    u16 clus = fat_find_free();
    if (clus == 0) {
      for (u32 j = 0; j < i; j++) {
        fat_set_entry(chain[j], 0x0000);
      }
      println("not enough space");
      return;
    }

    chain[i] = clus;
    fat_set_entry(clus, 0xFFFF);
  }

  for (u32 i = 0; i < needed - 1; i++) {
    fat_set_entry(chain[i], chain[i + 1]);
  }
  fat_set_entry(chain[needed - 1], 0xFFFF);

  u32 offset = 0;
  for (u32 i = 0; i < needed; i++) {
    u32 data_lba = data_start + (chain[i] - 2) * sec_per_clus;

    for (u16 sc = 0; sc < sec_per_clus; sc++) {
      if (offset == len)
        break;

      u32 remaining = len - offset;
      u32 chunk = remaining < 512 ? remaining : 512;

      memory_set((u8 *)buff, 0, 512);
      memory_copy(text + offset, (char *)buff, chunk);

      ata_write(data_lba + sc, 1, buff);

      offset += chunk;
    }
  }

  while (old_clus >= 2 && old_clus < 0xFFF8) {
    u16 next = fat_entry(old_clus);
    fat_set_entry(old_clus, 0x0000);
    old_clus = next;
  }

  dir_write_entry(dir_lba, idx, n83, chain[0], len, 0x20);

  println("written");
}

void fs_delete(char *name) {
  char n83[11];
  name_to_83(name, n83);

  u32 dir_lba;
  int idx;

  if (!dir_find(n83, &dir_lba, &idx)) {
    println("no such file");
    return;
  }

  u16 buff[256];
  ata_read(dir_lba, 1, buff);
  dir_entry_t *e = (dir_entry_t *)buff;
  u16 clus = e[idx].first_cluster;

  while (clus < 0xFFF8 && clus >= 2) {
    u16 next = fat_entry(clus);
    fat_set_entry(clus, 0x0000);
    clus = next;
  }

  e[idx].name[0] = 0xE5;
  ata_write(dir_lba, 1, buff);
  println("deleted");
}
