#include "fat.h"
#include "../cpu/types.h"
#include "../drivers/ata.h"
#include "../kernel/mem.h"

#include "../drivers/screen.h"
#include "../kernel/string.h"

static u32 fat_start, root_start, root_sectors, data_start, sectors_per_fat;
static u8 sec_per_clus, num_fats;

#define MAX_FILE_BYTES 8192
#define MAX_CHAIN_CLUSTERS (MAX_FILE_BYTES / 512)
#define FAT_NAME_BUF 13

typedef struct {
  int is_root;
  u16 cluster;
} fat_dir_ref_t;

static char to_upper(char c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

static int is_end_entry(const dir_entry_t *e) { return e->name[0] == 0x00; }

static int is_deleted_entry(const dir_entry_t *e) { return e->name[0] == 0xE5; }

static int is_long_name_entry(const dir_entry_t *e) { return e->attr == 0x0F; }

static int is_volume_label_entry(const dir_entry_t *e) {
  return (e->attr & 0x08) != 0;
}

static int is_hidden_sidecar_entry(const dir_entry_t *e) {
  return (e->attr & 0x02) != 0;
}

static int is_skippable_entry(const dir_entry_t *e) {
  return is_deleted_entry(e) || is_long_name_entry(e) ||
         is_volume_label_entry(e) || is_hidden_sidecar_entry(e);
}

static int is_valid_name_char(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static int is_dot_entry(const dir_entry_t *e) {
  return e->name[0] == '.' && e->name[1] == ' ' && e->name[2] == ' ' &&
         e->name[3] == ' ' && e->name[4] == ' ' && e->name[5] == ' ' &&
         e->name[6] == ' ' && e->name[7] == ' ' && e->name[8] == ' ' &&
         e->name[9] == ' ' && e->name[10] == ' ';
}

static int is_dotdot_entry(const dir_entry_t *e) {
  return e->name[0] == '.' && e->name[1] == '.' && e->name[2] == ' ' &&
         e->name[3] == ' ' && e->name[4] == ' ' && e->name[5] == ' ' &&
         e->name[6] == ' ' && e->name[7] == ' ' && e->name[8] == ' ' &&
         e->name[9] == ' ' && e->name[10] == ' ';
}

static int name_is_valid_83(const char *name) {
  int base_len = 0;
  int ext_len = 0;
  int seen_dot = 0;

  if (!name[0] || name[0] == '.')
    return 0;

  for (int i = 0; name[i]; i++) {
    char c = name[i];

    if (c == '.') {
      if (seen_dot)
        return 0;
      seen_dot = 1;
      continue;
    }

    if (!is_valid_name_char(c))
      return 0;

    if (seen_dot) {
      ext_len++;
      if (ext_len > 3)
        return 0;
    } else {
      base_len++;
      if (base_len > 8)
        return 0;
    }
  }

  return base_len > 0;
}

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

static int dir_find_in_region(u32 start_lba, u32 sector_count, char name83[11],
                              u32 *out_lba, int *out_idx) {
  u16 buff[256];

  for (u32 s = 0; s < sector_count; s++) {
    u32 lba = start_lba + s;
    ata_read(lba, 1, buff);
    dir_entry_t *e = (dir_entry_t *)buff;

    for (int i = 0; i < 16; i++) {
      if (is_end_entry(&e[i]))
        return 0;
      if (is_skippable_entry(&e[i]))
        continue;

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

int dir_find(char name83[11], u32 *out_lba, int *out_idx) {
  return dir_find_in_region(root_start, root_sectors, name83, out_lba, out_idx);
}

static int dir_find_in_cluster(u16 cluster, char name83[11], u32 *out_lba,
                               int *out_idx) {
  u32 lba = data_start + (cluster - 2) * sec_per_clus;
  return dir_find_in_region(lba, sec_per_clus, name83, out_lba, out_idx);
}

static int dir_find_in_ref(fat_dir_ref_t dir, char name83[11], u32 *out_lba,
                           int *out_idx) {
  if (dir.is_root)
    return dir_find(name83, out_lba, out_idx);

  return dir_find_in_cluster(dir.cluster, name83, out_lba, out_idx);
}

static int same_dir_ref(fat_dir_ref_t a, fat_dir_ref_t b) {
  return a.is_root == b.is_root && a.cluster == b.cluster;
}

void fs_init() {
  u16 buff[256];
  ata_read(0, 1, buff);
  bpb_t *bpb = (bpb_t *)buff;
  fat_start = bpb->reserved_sectors;
  num_fats = bpb->num_fats;
  sectors_per_fat = bpb->sectors_per_fat;
  root_start = fat_start + (bpb->num_fats * bpb->sectors_per_fat);
  root_sectors = ((u32)bpb->root_entries * 32 + bpb->bytes_per_sector - 1) /
                 bpb->bytes_per_sector;
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

static void fs_ls_region(u32 start_lba, u32 sector_count) {
  u16 buff[256];
  for (u32 s = 0; s < sector_count; s++) {
    ata_read(start_lba + s, 1, buff);
    dir_entry_t *e = (dir_entry_t *)buff;

    for (int i = 0; i < 16; i++) {
      if (is_end_entry(&e[i]))
        return; // end of dir
      if (is_skippable_entry(&e[i]))
        continue;

      char nm[13];
      name_from_83((char *)e[i].name, nm);
      print(nm);

      if (e[i].attr & 0x10)
        print("/");

      print("  ");
      int_to_ascii(e[i].size, nm);
      println(nm);
    }
  }
}

void fs_ls(void) { fs_ls_region(root_start, root_sectors); }

void fs_ls_path(char *path) {
  if (!path || !path[0] || (path[0] == '/' && path[1] == '\0')) {
    fs_ls();
    return;
  }

  fat_file_info_t info;

  if (!fat_stat_file(path, &info)) {
    println("no such directory");
    return;
  }

  if (!(info.attr & 0x10)) {
    println("not a directory");
    return;
  }

  if (info.first_cluster < 2) {
    println("bad directory");
    return;
  }

  u32 lba = data_start + (info.first_cluster - 2) * sec_per_clus;
  fs_ls_region(lba, sec_per_clus);
}

u16 fat_entry(u32 cluster) {
  u32 off = cluster * 2;
  u16 buff[256];
  ata_read((fat_start + off / 512), 1, buff);
  return buff[(off % 512) / 2];
}

static int path_next_component(char *path, int *pos, char *out) {
  int o = 0;

  while (path[*pos] == '/')
    (*pos)++;

  if (!path[*pos])
    return 0;

  while (path[*pos] && path[*pos] != '/') {
    if (o >= FAT_NAME_BUF - 1)
      return -1;
    out[o++] = path[*pos];
    (*pos)++;
  }

  out[o] = '\0';
  return 1;
}

static int dir_read_entry_in_ref(fat_dir_ref_t dir, char name83[11],
                                 dir_entry_t *out, u32 *out_lba, int *out_idx) {
  u32 lba;
  int idx;

  if (!out)
    return 0;

  if (!dir_find_in_ref(dir, name83, &lba, &idx))
    return 0;

  u16 buff[256];
  ata_read(lba, 1, buff);
  dir_entry_t *e = (dir_entry_t *)buff;

  *out = e[idx];

  if (out_lba)
    *out_lba = lba;

  if (out_idx)
    *out_idx = idx;

  return 1;
}

static int resolve_parent_dir(char *path, fat_dir_ref_t *parent,
                              char *leaf_name) {
  fat_dir_ref_t dir;
  char current[FAT_NAME_BUF];
  char next[FAT_NAME_BUF];
  int pos = 0;

  if (!path || !path[0] || !parent || !leaf_name)
    return FAT_ERR_INVALID;

  dir.is_root = 1;
  dir.cluster = 0;

  int got = path_next_component(path, &pos, current);
  if (got <= 0)
    return FAT_ERR_INVALID;

  if (!name_is_valid_83(current))
    return FAT_ERR_INVALID;

  while (1) {
    got = path_next_component(path, &pos, next);

    if (got < 0)
      return FAT_ERR_INVALID;

    if (got == 0) {
      *parent = dir;
      int i = 0;
      for (; current[i]; i++)
        leaf_name[i] = current[i];
      leaf_name[i] = '\0';
      return FAT_OK;
    }

    if (!name_is_valid_83(next))
      return FAT_ERR_INVALID;

    char current83[11];
    dir_entry_t entry;
    name_to_83(current, current83);

    if (!dir_read_entry_in_ref(dir, current83, &entry, 0, 0))
      return FAT_ERR_NOT_FOUND;

    if (!(entry.attr & 0x10))
      return FAT_ERR_INVALID;

    if (entry.first_cluster < 2)
      return FAT_ERR_INVALID;

    dir.is_root = 0;
    dir.cluster = entry.first_cluster;

    int i = 0;
    for (; next[i]; i++)
      current[i] = next[i];
    current[i] = '\0';
  }
}

int fat_read_file_at(char *name, u32 offset, u8 *dst, u32 len, u32 *out_len) {
  fat_file_info_t info;

  if (out_len)
    *out_len = 0;

  if (!dst && len > 0)
    return 0;

  if (!fat_stat_file(name, &info))
    return 0;

  if (info.attr & 0x10)
    return 0;

  if (offset >= info.size)
    return 1;

  u32 available = info.size - offset;
  u32 wanted = len < available ? len : available;

  u16 cluster = info.first_cluster;
  u32 file_pos = 0;
  u32 copied = 0;
  u16 buff[256];

  while (cluster < 0xFFF8 && copied < wanted) {
    u32 lba = data_start + (cluster - 2) * sec_per_clus;

    for (u8 sc = 0; sc < sec_per_clus && copied < wanted; sc++) {
      ata_read(lba + sc, 1, buff);

      char *bytes = (char *)buff;

      for (u32 i = 0; i < 512 && copied < wanted; i++) {
        if (file_pos >= offset) {
          dst[copied] = bytes[i];
          copied++;
        }

        file_pos++;

        if (file_pos >= info.size)
          break;
      }
    }
    cluster = fat_entry(cluster);
  }

  if (out_len)
    *out_len = copied;

  return copied == wanted;
}

int fat_read_file(char *name, u8 *dst, u32 max_len, u32 *out_len) {
  fat_file_info_t info;

  if (out_len)
    *out_len = 0;

  if (!fat_stat_file(name, &info)) {
    println("file not found");
    return 0;
  }

  if (info.size > max_len) {
    println("file too big");
    return 0;
  }

  return fat_read_file_at(name, 0, dst, info.size, out_len);
}

int fat_stat_file(char *name, fat_file_info_t *out) {
  fat_dir_ref_t parent;
  char leaf[13];

  if (!out)
    return 0;

  out->size = 0;
  out->first_cluster = 0;
  out->attr = 0;

  if (resolve_parent_dir(name, &parent, leaf) != FAT_OK)
    return 0;

  char target[11];
  name_to_83(leaf, target);

  u32 dir_lba;
  int idx;

  if (!dir_find_in_ref(parent, target, &dir_lba, &idx))
    return 0;

  u16 dir_buff[256];
  ata_read(dir_lba, 1, dir_buff);
  dir_entry_t *e = (dir_entry_t *)dir_buff;

  out->size = e[idx].size;
  out->first_cluster = e[idx].first_cluster;
  out->attr = e[idx].attr;
  return 1;
}

void fs_cat(char *name) {
  static u8 cat_buf[MAX_FILE_BYTES];
  u32 len;

  if (!fat_read_file(name, cat_buf, MAX_FILE_BYTES, &len))
    return;

  for (u32 i = 0; i < len; i++)
    print_char(cat_buf[i], -1, -1, 0);

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

static u32 count_free_clusters() {
  u32 free_count = 0;
  u32 total_slots = sectors_per_fat * 512 / 2;

  for (u32 c = 2; c < total_slots; c++) {
    if (fat_entry(c) == 0)
      free_count++;
  }

  return free_count;
}

static int find_free_slot_in_region(u32 start_lba, u32 sector_count,
                                    u32 *out_lba, int *out_index) {
  u16 buff[256];

  for (u32 s = 0; s < sector_count; s++) {
    u32 lba = start_lba + s;
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

int find_dir_free_slot(u32 *out_lba, int *out_index) {
  return find_free_slot_in_region(root_start, root_sectors, out_lba, out_index);
}

static int find_free_slot_in_cluster(u16 cluster, u32 *out_lba,
                                     int *out_index) {
  u32 lba = data_start + (cluster - 2) * sec_per_clus;
  return find_free_slot_in_region(lba, sec_per_clus, out_lba, out_index);
}

static int find_free_slot_in_ref(fat_dir_ref_t dir, u32 *out_lba,
                                 int *out_index) {
  if (dir.is_root)
    return find_dir_free_slot(out_lba, out_index);

  return find_free_slot_in_cluster(dir.cluster, out_lba, out_index);
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

static void free_chain(u16 first_cluster) {
  while (first_cluster < 0xFFF8 && first_cluster >= 2) {
    u16 next = fat_entry(first_cluster);
    fat_set_entry(first_cluster, 0x0000);
    first_cluster = next;
  }
}

int fat_create_file(char *name) {
  fat_dir_ref_t parent;
  char leaf[13];
  char n83[11];

  int ret = resolve_parent_dir(name, &parent, leaf);
  if (ret != FAT_OK)
    return ret;

  name_to_83(leaf, n83);

  u32 lba;
  int idx;

  if (dir_find_in_ref(parent, n83, &lba, &idx))
    return FAT_ERR_EXISTS;

  if (!find_free_slot_in_ref(parent, &lba, &idx))
    return FAT_ERR_NO_SPACE;

  dir_write_entry(lba, idx, n83, 0, 0, 0x20);
  return FAT_OK;
}

void fs_create(char *name) {
  int ret = fat_create_file(name);

  if (ret == FAT_OK)
    println("created");
  else if (ret == FAT_ERR_INVALID)
    println("invalid 8.3 name");
  else if (ret == FAT_ERR_EXISTS)
    println("file already exist.");
  else if (ret == FAT_ERR_NO_SPACE)
    println("dir full");
  else
    println("create failed");
}

void fs_mkdir(char *name) {
  int ret = fat_mkdir(name);

  if (ret == FAT_OK)
    println("directory created");
  else if (ret == FAT_ERR_INVALID)
    println("invalid 8.3 name");
  else if (ret == FAT_ERR_EXISTS)
    println("already exists");
  else if (ret == FAT_ERR_NO_SPACE)
    println("not enough space");
  else
    println("mkdir failed");
}

int fat_mkdir(char *name) {
  fat_dir_ref_t parent;
  char leaf[13];
  char n83[11];

  int ret = resolve_parent_dir(name, &parent, leaf);
  if (ret != FAT_OK)
    return ret;

  name_to_83(leaf, n83);

  u32 lba;
  int idx;

  if (dir_find_in_ref(parent, n83, &lba, &idx))
    return FAT_ERR_EXISTS;

  if (!find_free_slot_in_ref(parent, &lba, &idx))
    return FAT_ERR_NO_SPACE;

  u16 clus = fat_find_free();
  if (clus == 0)
    return FAT_ERR_NO_SPACE;

  fat_set_entry(clus, 0xFFFF);

  u16 empty[256];
  memory_set((u8 *)empty, 0, 512);

  u32 data_lba = data_start + (clus - 2) * sec_per_clus;
  for (u16 sc = 0; sc < sec_per_clus; sc++) {
    ata_write(data_lba + sc, 1, empty);
  }

  char dot[11] = {'.', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
  char dotdot[11] = {'.', '.', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
  u16 parent_cluster = parent.is_root ? 0 : parent.cluster;

  dir_write_entry(data_lba, 0, dot, clus, 0, 0x10);
  dir_write_entry(data_lba, 1, dotdot, parent_cluster, 0, 0x10);

  dir_write_entry(lba, idx, n83, clus, 0, 0x10);
  return FAT_OK;
}

int fat_write_file(char *name, u8 *src, u32 len) {
  fat_dir_ref_t parent;
  char leaf[13];
  char n83[11];

  if (len > 0 && !src)
    return FAT_ERR_INVALID;

  int ret = resolve_parent_dir(name, &parent, leaf);
  if (ret != FAT_OK)
    return ret;

  name_to_83(leaf, n83);

  u32 dir_lba;
  int idx;

  if (!dir_find_in_ref(parent, n83, &dir_lba, &idx))
    return FAT_ERR_NOT_FOUND;

  if (len > MAX_FILE_BYTES)
    return FAT_ERR_TOO_BIG;

  u16 buff[256];
  ata_read(dir_lba, 1, buff);
  dir_entry_t *e = (dir_entry_t *)buff;

  if (e[idx].attr & 0x10)
    return FAT_ERR_INVALID;

  u16 old_clus = e[idx].first_cluster;

  if (len == 0) {
    free_chain(old_clus);

    dir_write_entry(dir_lba, idx, n83, 0, 0, 0x20);
    return FAT_OK;
  }

  u32 cluster_bytes = sec_per_clus * 512;
  u32 needed = (len + cluster_bytes - 1) / cluster_bytes;
  if (needed > MAX_CHAIN_CLUSTERS)
    return FAT_ERR_TOO_BIG;

  u16 chain[MAX_CHAIN_CLUSTERS];

  for (u32 i = 0; i < needed; i++) {
    u16 clus = fat_find_free();
    if (clus == 0) {
      for (u32 j = 0; j < i; j++) {
        fat_set_entry(chain[j], 0x0000);
      }
      return FAT_ERR_NO_SPACE;
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
      memory_copy((char *)src + offset, (char *)buff, chunk);

      ata_write(data_lba + sc, 1, buff);

      offset += chunk;
    }
  }

  free_chain(old_clus);

  dir_write_entry(dir_lba, idx, n83, chain[0], len, 0x20);

  return FAT_OK;
}

int fat_write_file_at(char *name, u32 offset, u8 *src, u32 len) {
  if (len > 0 && !src)
    return FAT_ERR_INVALID;

  if (len == 0)
    return FAT_OK;

  fat_dir_ref_t parent;
  char leaf[13];
  char n83[11];

  int ret = resolve_parent_dir(name, &parent, leaf);
  if (ret != FAT_OK)
    return ret;

  name_to_83(leaf, n83);

  u32 dir_lba;
  int idx;

  if (!dir_find_in_ref(parent, n83, &dir_lba, &idx))
    return FAT_ERR_NOT_FOUND;

  u16 buff[256];
  ata_read(dir_lba, 1, buff);
  dir_entry_t *e = (dir_entry_t *)buff;

  if (e[idx].attr & 0x10)
    return FAT_ERR_INVALID;

  u16 first_cluster = e[idx].first_cluster;
  u32 old_size = e[idx].size;

  if (offset > old_size)
    return FAT_ERR_INVALID;

  u32 new_size = offset + len;

  if (new_size < offset)
    return FAT_ERR_TOO_BIG;

  if (new_size > MAX_FILE_BYTES)
    return FAT_ERR_TOO_BIG;

  if (first_cluster == 0) {
    if (offset != 0)
      return FAT_ERR_INVALID;

    return fat_write_file(name, src, len);
  }

  u32 cluster_bytes = sec_per_clus * 512;
  u32 old_cluster = (old_size + cluster_bytes - 1) / cluster_bytes;
  u32 new_cluster = (new_size + cluster_bytes - 1) / cluster_bytes;

  if (new_cluster > old_cluster) {
    u32 extra_clusters_needed = new_cluster - old_cluster;

    if (extra_clusters_needed > count_free_clusters())
      return FAT_ERR_NO_SPACE;

    u16 last_cluster = first_cluster;
    u16 next = fat_entry(last_cluster);

    while (next < 0xFFF8) {
      last_cluster = next;
      next = fat_entry(last_cluster);
    }

    for (u32 i = 0; i < extra_clusters_needed; i++) {
      u16 new_clus = fat_find_free();
      if (new_clus == 0)
        return FAT_ERR_NO_SPACE;

      fat_set_entry(last_cluster, new_clus);
      fat_set_entry(new_clus, 0xFFFF);
      last_cluster = new_clus;
    }
  }
  u32 cluster_index = offset / cluster_bytes;
  u32 offset_in_cluster = offset % cluster_bytes;
  u32 sector_in_cluster = offset_in_cluster / 512;
  u32 byte_in_sector = offset_in_cluster % 512;

  u16 current_cluster = first_cluster;
  for (u32 i = 0; i < cluster_index; i++) {
    current_cluster = fat_entry(current_cluster);
    if (current_cluster < 2 || current_cluster >= 0xFFF8)
      return FAT_ERR_INVALID;
  }

  u32 remaining = len;
  u32 src_offset = 0;

  while (remaining > 0) {
    u32 data_lba = data_start + (current_cluster - 2) * sec_per_clus;

    for (u16 sc = sector_in_cluster; sc < sec_per_clus && remaining > 0; sc++) {
      u16 sector_buff[256];

      ata_read(data_lba + sc, 1, sector_buff);

      u32 room = 512 - byte_in_sector;
      u32 chunk = remaining < room ? remaining : room;

      char *bytes = (char *)sector_buff;
      memory_copy((char *)src + src_offset, bytes + byte_in_sector, chunk);

      ata_write(data_lba + sc, 1, sector_buff);

      remaining -= chunk;
      src_offset += chunk;
      byte_in_sector = 0;
    }

    sector_in_cluster = 0;

    if (remaining > 0) {
      u16 next = fat_entry(current_cluster);

      if (next >= 0xFFF8)
        return FAT_ERR_INVALID;

      current_cluster = next;
    }
  }

  if (new_size > old_size) {
    e[idx].size = new_size;
    ata_write(dir_lba, 1, buff);
  }

  return FAT_OK;
}

void fs_write(char *name, char *text) {
  int ret = fat_write_file(name, (u8 *)text, strlen(text));

  if (ret == FAT_OK)
    println("written");
  else if (ret == FAT_ERR_INVALID)
    println("invalid 8.3 name");
  else if (ret == FAT_ERR_NOT_FOUND)
    println("no such file");
  else if (ret == FAT_ERR_TOO_BIG)
    println("too big for v1");
  else if (ret == FAT_ERR_NO_SPACE)
    println("not enough space");
  else
    println("write failed");
}

int fat_delete_file(char *path) {
  fat_dir_ref_t parent;
  char leaf[13];
  char n83[11];

  int ret = resolve_parent_dir(path, &parent, leaf);
  if (ret != FAT_OK)
    return ret;

  name_to_83(leaf, n83);

  u32 dir_lba;
  int idx;

  if (!dir_find_in_ref(parent, n83, &dir_lba, &idx))
    return FAT_ERR_NOT_FOUND;

  u16 buff[256];
  ata_read(dir_lba, 1, buff);
  dir_entry_t *e = (dir_entry_t *)buff;
  u16 clus = e[idx].first_cluster;

  if (e[idx].attr & 0x10)
    return FAT_ERR_INVALID;

  free_chain(clus);

  e[idx].name[0] = 0xE5;
  ata_write(dir_lba, 1, buff);

  return FAT_OK;
}

void fs_delete(char *name) {
  int ret = fat_delete_file(name);

  if (ret == FAT_OK)
    println("deleted");
  else if (ret == FAT_ERR_INVALID)
    println("is a directory");
  else if (ret == FAT_ERR_NOT_FOUND)
    println("no such file");
  else
    println("delete failed");
}

int fat_append_file(char *name, u8 *src, u32 append_len) {
  fat_dir_ref_t parent;
  char leaf[13];
  char n83[11];

  if (append_len > 0 && !src)
    return FAT_ERR_INVALID;

  int ret = resolve_parent_dir(name, &parent, leaf);
  if (ret != FAT_OK)
    return ret;

  name_to_83(leaf, n83);

  u32 dir_lba;
  int idx;

  if (!dir_find_in_ref(parent, n83, &dir_lba, &idx))
    return FAT_ERR_NOT_FOUND;

  u16 buff[256];
  ata_read(dir_lba, 1, buff);
  dir_entry_t *e = (dir_entry_t *)buff;

  if (e[idx].attr & 0x10)
    return FAT_ERR_INVALID;

  u16 first_cluster = e[idx].first_cluster;
  u32 old_size = e[idx].size;
  u32 new_size = old_size + append_len;

  if (new_size < old_size)
    return FAT_ERR_TOO_BIG;

  if (append_len == 0)
    return FAT_OK;

  if (new_size > MAX_FILE_BYTES)
    return FAT_ERR_TOO_BIG;

  if (first_cluster == 0)
    return fat_write_file(name, src, append_len);

  u32 cluster_bytes = sec_per_clus * 512;
  u32 offset_in_cluster = old_size % cluster_bytes;
  u32 sector_in_cluster = offset_in_cluster / 512;
  u32 byte_in_sector = offset_in_cluster % 512;
  u32 free_in_tail = 0;
  u32 extra_clusters_needed = 0;

  if (offset_in_cluster != 0)
    free_in_tail = cluster_bytes - offset_in_cluster;

  if (append_len > free_in_tail) {
    u32 extra_bytes = append_len - free_in_tail;
    extra_clusters_needed = (extra_bytes + cluster_bytes - 1) / cluster_bytes;
  }

  if (extra_clusters_needed > count_free_clusters())
    return FAT_ERR_NO_SPACE;

  u16 last_cluster = first_cluster;
  u16 next = fat_entry(last_cluster);
  while (next < 0xFFF8) {
    last_cluster = next;
    next = fat_entry(last_cluster);
  }

  if (offset_in_cluster == 0) {
    u16 new_clus = fat_find_free();
    if (new_clus == 0)
      return FAT_ERR_NO_SPACE;

    fat_set_entry(last_cluster, new_clus);
    fat_set_entry(new_clus, 0xFFFF);

    last_cluster = new_clus;
    sector_in_cluster = 0;
    byte_in_sector = 0;
  }

  u32 remaining = append_len;
  u32 src_offset = 0;
  u16 current_cluster = last_cluster;

  u32 data_lba = data_start + (current_cluster - 2) * sec_per_clus;

  for (u32 sc = sector_in_cluster; sc < sec_per_clus && remaining > 0; sc++) {
    u16 sector_buff[256];
    ata_read(data_lba + sc, 1, sector_buff);

    u32 room = 512 - byte_in_sector;
    u32 can_write = remaining < room ? remaining : room;

    char *bytes = (char *)sector_buff;
    memory_copy((char *)src + src_offset, bytes + byte_in_sector, can_write);

    ata_write(data_lba + sc, 1, sector_buff);

    remaining -= can_write;
    src_offset += can_write;
    byte_in_sector = 0;
  }

  if (remaining == 0) {
    e[idx].size = new_size;
    ata_write(dir_lba, 1, buff);
    return FAT_OK;
  }

  while (remaining > 0) {
    u16 new_clus = fat_find_free();
    if (new_clus == 0)
      return FAT_ERR_NO_SPACE;

    fat_set_entry(current_cluster, new_clus);
    fat_set_entry(new_clus, 0xFFFF);
    current_cluster = new_clus;

    u32 data_lba = data_start + (current_cluster - 2) * sec_per_clus;
    for (u16 sc = 0; sc < sec_per_clus && remaining > 0; sc++) {
      u16 sector_buff[256];
      memory_set((u8 *)sector_buff, 0, 512);

      u32 chunk = remaining < 512 ? remaining : 512;
      memory_copy((char *)src + src_offset, (char *)sector_buff, chunk);

      ata_write(data_lba + sc, 1, sector_buff);

      remaining -= chunk;
      src_offset += chunk;
    }
  }

  e[idx].size = new_size;
  ata_write(dir_lba, 1, buff);
  return FAT_OK;
}

void fs_append(char *name, char *text) {
  int ret = fat_append_file(name, (u8 *)text, strlen(text));

  if (ret == FAT_OK)
    println("appended");
  else if (ret == FAT_ERR_INVALID)
    println("invalid 8.3 name");
  else if (ret == FAT_ERR_NOT_FOUND)
    println("no such file");
  else if (ret == FAT_ERR_TOO_BIG)
    println("too big for v1");
  else if (ret == FAT_ERR_NO_SPACE)
    println("not enough space");
  else
    println("append failed");
}

int fat_rename_file(char *old_path, char *new_path) {
  fat_dir_ref_t old_parent;
  fat_dir_ref_t new_parent;
  char old_leaf[13];
  char new_leaf[13];
  char old83[11];
  char new83[11];

  int ret = resolve_parent_dir(old_path, &old_parent, old_leaf);
  if (ret != FAT_OK)
    return ret;

  ret = resolve_parent_dir(new_path, &new_parent, new_leaf);
  if (ret != FAT_OK)
    return ret;

  if (!same_dir_ref(old_parent, new_parent))
    return FAT_ERR_INVALID;

  name_to_83(old_leaf, old83);
  name_to_83(new_leaf, new83);

  u32 dir_lba;
  int idx;

  if (!dir_find_in_ref(old_parent, old83, &dir_lba, &idx))
    return FAT_ERR_NOT_FOUND;

  u32 new_dir_lba;
  int new_idx;

  if (dir_find_in_ref(old_parent, new83, &new_dir_lba, &new_idx))
    return FAT_ERR_EXISTS;

  u16 buff[256];
  ata_read(dir_lba, 1, buff);
  dir_entry_t *e = (dir_entry_t *)buff;

  if (e[idx].attr & 0x10)
    return FAT_ERR_INVALID;

  memory_copy(new83, (char *)e[idx].name, 11);

  ata_write(dir_lba, 1, buff);
  return FAT_OK;
}

void fs_rename(char *name, char *new_name) {
  int ret = fat_rename_file(name, new_name);

  if (ret == FAT_OK)
    println("renamed");
  else if (ret == FAT_ERR_INVALID)
    println("invalid rename");
  else if (ret == FAT_ERR_NOT_FOUND)
    println("no such file");
  else if (ret == FAT_ERR_EXISTS)
    println("new name already exists");
  else
    println("rename failed");
}

static int dir_cluster_is_empty(u16 cluster) {
  if (cluster < 2)
    return 0;

  u16 buff[256];
  u32 lba = data_start + (cluster - 2) * sec_per_clus;

  for (u16 s = 0; s < sec_per_clus; s++) {
    ata_read(lba + s, 1, buff);
    dir_entry_t *e = (dir_entry_t *)buff;

    for (int i = 0; i < 16; i++) {
      if (is_end_entry(&e[i]))
        return 1;

      if (is_skippable_entry(&e[i]))
        continue;

      if (is_dot_entry(&e[i]) || is_dotdot_entry(&e[i]))
        continue;

      return 0;
    }
  }
  return 1;
}

int fat_rmdir(char *path) {
  fat_dir_ref_t parent;
  char leaf[13];
  char n83[11];

  int ret = resolve_parent_dir(path, &parent, leaf);
  if (ret != FAT_OK)
    return ret;

  if (leaf[0] == '.')
    return FAT_ERR_INVALID;

  name_to_83(leaf, n83);

  u32 dir_lba;
  int idx;

  if (!dir_find_in_ref(parent, n83, &dir_lba, &idx))
    return FAT_ERR_NOT_FOUND;

  u16 buff[256];
  ata_read(dir_lba, 1, buff);
  dir_entry_t *e = (dir_entry_t *)buff;

  if (!(e[idx].attr & 0x10))
    return FAT_ERR_INVALID;

  u16 clus = e[idx].first_cluster;

  if (!dir_cluster_is_empty(clus))
    return FAT_ERR_NO_SPACE;

  free_chain(clus);

  e[idx].name[0] = 0xE5;
  ata_write(dir_lba, 1, buff);

  return FAT_OK;
}

void fs_rmdir(char *path) {
  int ret = fat_rmdir(path);

  if (ret == FAT_OK)
    println("directory removed");
  else if (ret == FAT_ERR_INVALID)
    println("invalid directory");
  else if (ret == FAT_ERR_NOT_FOUND)
    println("no such directory");
  else if (ret == FAT_ERR_NOT_EMPTY)
    println("directory not empty");
  else
    println("rmdir failed");
}
