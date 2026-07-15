#include "shell.h"

#include "../cpu/types.h"
#include "../cpu/user_mode.h"
#include "../drivers/ata.h"
#include "../drivers/screen.h"
#include "../elf/elf.h"
#include "../fs/fat.h"
#include "../fs/vfs.h"
#include "../kernel/function.h"
#include "../kernel/string.h"
#include "../memory/frame_alloc.h"
#include "../memory/memory_map.h"
#include "../task/task.h"

typedef void (*cmd_fn)(char *args);

typedef struct {
  const char *name;
  cmd_fn handler;
  const char *help;
} command_t;

static char shell_cwd[VFS_MAX_PATH] = "/";

static void cmd_help(char *a);
static void cmd_end(char *a);
static void cmd_mmap(char *a);
static void cmd_falloc(char *a);
static void cmd_ptest(char *a);
static void cmd_tasktest(char *a);
static void cmd_ident(char *a);
static void cmd_rsect(char *a);
static void cmd_wsect(char *a);
static void cmd_fsinfo(char *a);
static void cmd_ls(char *a);
static void cmd_cat(char *a);
static void cmd_create(char *a);
static void cmd_write(char *a);
static void cmd_mkdir(char *a);
static void cmd_writepat(char *a);
static void cmd_delete(char *a);
static void cmd_append(char *a);
static void cmd_rename(char *a);
static void cmd_cd(char *a);
static void cmd_pwd(char *a);
static void cmd_start_user_test(char *a);
static void cmd_start_fault_user_test(char *a);
static void cmd_tasks(char *a);
static void cmd_reap(char *a);
static void cmd_procs(char *a);
static void cmd_run(char *a);
static void cmd_kill(char *a);
static void cmd_wait(char *a);
static void cmd_vfstest(char *a);
static void cmd_rmdir(char *a);
static void cmd_elfinfo(char *a);
static void cmd_runelf(char *a);

static const command_t commands[] = {
    {"HELP", cmd_help, "list commands"},
    {"END", cmd_end, "halt the CPU"},
    {"MMAP", cmd_mmap, "print E820 memory map"},
    {"FALLOC", cmd_falloc, "frame allocator test"},
    {"PTEST", cmd_ptest, "trigger a page fault"},
    {"TASKTEST", cmd_tasktest, "multitasking test"},
    {"IDENT", cmd_ident, "ATA IDENTIFY"},
    {"RSECT", cmd_rsect, "read+dump sector 0"},
    {"WSECT", cmd_wsect, "write test sector 1"},
    {"FSINFO", cmd_fsinfo, "FAT16 BPB info"},
    {"LS", cmd_ls, "[dir] list directory"},
    {"CAT", cmd_cat, "<file>  print a file"},
    {"CREATE", cmd_create, "<file>  make empty file"},
    {"MKDIR", cmd_mkdir, "<dir> create directory"},
    {"RMDIR", cmd_rmdir, "<dir> remove empty directory"},
    {"WRITE", cmd_write, "<file> <text>  write a file"},
    {"WRITEPAT", cmd_writepat, "<file> <size>  write test pattern"},
    {"DELETE", cmd_delete, "<file>  remove a file"},
    {"APPEND", cmd_append, "<file> <text> append new text on existing in file"},
    {"RENAME", cmd_rename, "<file> <name> rename file"},
    {"PWD", cmd_pwd, "print current directory"},
    {"CD", cmd_cd, "<dir> change directory"},
    {"USERTEST", cmd_start_user_test, "test user mode"},
    {"USERFAULT", cmd_start_fault_user_test, "test fault user mode"},
    {"TASKS", cmd_tasks, "list tasks"},
    {"REAP", cmd_reap, "[pid] remove exited process/tasks"},
    {"PROCS", cmd_procs, "list user processes"},
    {"RUN", cmd_run, "<file> run YOZ1 or ELF binary"},
    {"KILL", cmd_kill, "<pid> kill process"},
    {"WAIT", cmd_wait, "<pid> show process exit status"},
    {"VFSTEST", cmd_vfstest, "test vfs"},
    {"ELFINFO", cmd_elfinfo, "<file> validate ELF header"},
    {"RUNELF", cmd_runelf, "<file> run ELF binary"},
    {0, 0, 0},
};

static void shell_copy_path(char *dst, const char *src) {
  int i = 0;
  while (src[i] && i < VFS_MAX_PATH - 1) {
    dst[i] = src[i];
    i++;
  }
  dst[i] = '\0';
}

static int shell_resolve_path(char *input, char *out) {
  int o = 0;
  int i = 0;

  if (!input || !input[0])
    return 0;

  if (input[0] == '.' && !input[1])
    return 0;

  if (input[0] == '.' && input[1] == '.')
    return 0;

  if (input[0] == '/') {
    while (input[i]) {
      if (o >= VFS_MAX_PATH - 1)
        return 0;
      out[o++] = input[i++];
    }
    out[o] = '\0';
    return 1;
  }

  while (shell_cwd[i]) {
    if (o >= VFS_MAX_PATH - 1)
      return 0;
    out[o++] = shell_cwd[i++];
  }

  if (o > 1) {
    if (o >= VFS_MAX_PATH - 1)
      return 0;
    out[o++] = '/';
  }

  i = 0;
  while (input[i]) {
    if (o >= VFS_MAX_PATH - 1)
      return 0;
    out[o++] = input[i++];
  }

  out[o] = '\0';
  return 1;
}

static void print_u32_dec(u32 value) {
  char buf[16];
  int_to_ascii(value, buf);
  print(buf);
}

void shell_print_prompt(void) {
  print("yozOS ");
  print(shell_cwd);
  print(" > ");
}

static void cmd_help(char *a) {
  UNUSED(a);
  for (int i = 0; commands[i].name; i++) {
    print("  ");
    print(commands[i].name);
    print("  ");
    println(commands[i].help);
  }
}

static void cmd_end(char *a) {
  UNUSED(a);
  print("Stopping the CPU. Bye!\n");
  asm volatile("hlt");
}

static void cmd_mmap(char *a) {
  UNUSED(a);
  memory_map_print();
}

static void cmd_falloc(char *a) {
  UNUSED(a);
  char buf[50];
  for (int i = 0; i < 4; i++) {
    u32 addr = alloc_frame();
    print_u64(addr, buf);
    if (i == 2)
      free_frame(addr);
    println("");
  }
}

static void cmd_ptest(char *a) {
  UNUSED(a);
  u32 *bad = (u32 *)0x800000; // 8 MiB — PDE[1], not present
  u32 x = *bad;               // ← page fault fires here
  UNUSED(x);
}

static void cmd_ident(char *a) {
  UNUSED(a);
  ata_identify();
}

static void cmd_rsect(char *a) {
  UNUSED(a);
  u16 sector[256];
  ata_read(0, 1, sector);
  println((char *)sector);
  for (int i = 0; i < 256; i++) {
    char buff[10];
    hex16_to_ascii(sector[i], buff);
    print(buff);
    print(" ");
  }
}

static void cmd_wsect(char *a) {
  UNUSED(a);
  u16 sector[256];
  for (int i = 0; i < 256; i++)
    sector[i] = 0;
  char *txt = (char *)sector;
  char *msg = "WROTE-FROM-YOZOS";
  for (int i = 0; msg[i]; i++)
    txt[i] = msg[i];
  ata_write(1, 1, sector);
  println((char *)sector);
  for (int i = 0; i < 256; i++) {
    char buff[10];
    hex16_to_ascii(sector[i], buff);
    print(buff);
    print(" ");
  }
}

static void cmd_fsinfo(char *a) {
  UNUSED(a);
  fs_info();
}

static void cmd_ls(char *a) {
  char path[VFS_MAX_PATH];

  if (!a || !a[0]) {
    shell_copy_path(path, shell_cwd);
  } else if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  if (path[0] == '/' && path[1] == '\0')
    fs_ls();
  else
    fs_ls_path(path);
}

static void cmd_cat(char *a) {
  char path[VFS_MAX_PATH];

  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  fs_cat(path);
}

static void cmd_create(char *a) {
  if (strcmp(a, " ") == 0 || strlen(a) == 0) {
    println("usage CREATE <file>");
    return;
  }

  char path[VFS_MAX_PATH];
  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  fs_create(path);
}

static void cmd_write(char *a) {
  // split args into "<file>" and "<text>" at the first space
  int i = 0;
  while (a[i] && a[i] != ' ')
    i++;
  if (a[i] != ' ') {
    println("usage: WRITE <file> <text>");
    return;
  }
  a[i] = '\0';

  char path[VFS_MAX_PATH];
  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  fs_write(path, a + i + 1);
}

static void cmd_append(char *a) {
  // split args into "<file>" and "<text>" at the first space
  int i = 0;
  while (a[i] && a[i] != ' ')
    i++;
  if (a[i] != ' ') {
    println("usage: APPEND <file> <text>");
    return;
  }
  a[i] = '\0';

  char path[VFS_MAX_PATH];
  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  fs_append(path, a + i + 1);
}

static void cmd_mkdir(char *a) {
  char path[VFS_MAX_PATH];

  if (!a || !a[0]) {
    println("usage: MKDIR <dir>");
    return;
  }

  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  fs_mkdir(path);
}

static void cmd_rmdir(char *a) {
  char path[VFS_MAX_PATH];

  if (!a || !a[0]) {
    println("usage: RMDIR <dir>");
    return;
  }

  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  fs_rmdir(path);
}

static void cmd_rename(char *a) {
  int i = 0;
  while (a[i] && a[i] != ' ')
    i++;
  if (a[i] != ' ') {
    println("usage: RENAME <old> <new>");
    return;
  }
  a[i] = '\0';

  char old_path[VFS_MAX_PATH];
  char new_path[VFS_MAX_PATH];

  if (!shell_resolve_path(a, old_path) ||
      !shell_resolve_path(a + i + 1, new_path)) {
    println("invalid path");
    return;
  }

  fs_rename(old_path, new_path);
}

static void cmd_pwd(char *a) {
  UNUSED(a);
  println(shell_cwd);
}

static void cmd_cd(char *a) {
  char path[VFS_MAX_PATH];
  vfs_stat_t st;

  if (!a || !a[0]) {
    println("usage: CD <dir>");
    return;
  }

  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  if (vfs_stat(path, &st) < 0) {
    println("no such directory");
    return;
  }

  if (st.type != VFS_STAT_TYPE_DIR) {
    println("not a directory");
    return;
  }

  shell_copy_path(shell_cwd, path);
}

static int parse_u32(char *s, u32 *out) {
  if (!s[0])
    return 0;

  u32 n = 0;
  for (int i = 0; s[i]; i++) {
    if (s[i] < '0' || s[i] > '9')
      return 0;
    n = n * 10 + (s[i] - '0');
  }

  *out = n;
  return 1;
}

static void cmd_writepat(char *a) {
  int i = 0;
  while (a[i] && a[i] != ' ')
    i++;
  if (a[i] != ' ') {
    println("usage: WRITEPAT <file> <size>");
    return;
  }
  a[i] = '\0';

  u32 size;
  if (!parse_u32(a + i + 1, &size)) {
    println("usage: WRITEPAT <file> <size>");
    return;
  }
  if (size > 2400) {
    println("too big for v1");
    return;
  }

  static char pattern[2401];
  for (u32 j = 0; j < size; j++)
    pattern[j] = 'A' + ((j / 512) % 26);
  pattern[size] = '\0';

  char path[VFS_MAX_PATH];
  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  fs_write(path, pattern);
}

static void cmd_delete(char *a) {
  char path[VFS_MAX_PATH];

  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  fs_delete(path);
}

void user_input(char *input) {
  char *args = input;
  while (*args && *args != ' ')
    args++;
  if (*args == ' ') {
    *args = '\0';
    args++;
  }

  for (int i = 0; commands[i].name; i++) {
    if (strcmp(input, commands[i].name) == 0) {
      commands[i].handler(args);
      return;
    }
  }

  if (input[0])
    print("unknown command (try HELP)\n");
}

static task_t *user_test_task;
static int user_test_started;
static task_t *user_fault_task;
static int user_fault_started;

static void refresh_reaped_builtin_tasks(void) {
  if (user_test_started && user_test_task &&
      task_get_state(user_test_task) == TASK_UNUSED) {
    user_test_started = 0;
    user_test_task = 0;
  }
  if (user_fault_started && user_fault_task &&
      task_get_state(user_fault_task) == TASK_UNUSED) {
    user_fault_started = 0;
    user_fault_task = 0;
  }
}

static void reap_tasks(void) {
  user_process_reap();
  task_reap_exited();
  refresh_reaped_builtin_tasks();
}

static void cmd_start_user_test(char *a) {
  UNUSED(a);
  reap_tasks();

  if (user_test_started)
    return;

  task_t *task = start_user_test_task();
  if (!task) {
    return;
  }

  user_test_started = 1;
  user_test_task = task;
}

void cmd_start_fault_user_test(char *a) {
  UNUSED(a);
  reap_tasks();

  if (user_fault_started)
    return;

  task_t *task = start_user_fault_task();
  if (!task) {
    return;
  }

  user_fault_started = 1;
  user_fault_task = task;
}

static void cmd_tasktest(char *a) {
  UNUSED(a);
  reap_tasks();
  test_task();
}

static void cmd_tasks(char *a) {
  UNUSED(a);
  task_dump();
}

static void cmd_reap(char *a) {
  if (strcmp(a, " ") == 0 || strlen(a) == 0) {
    reap_tasks();
    return;
  }
  u32 out;
  if (!parse_u32(a, &out)) {
    println("usage: REAP [pid]");
    return;
  }

  user_process_reap_pid(out);
  task_reap_exited();
  refresh_reaped_builtin_tasks();
}

static void cmd_procs(char *a) {
  UNUSED(a);
  user_process_dump();
}

static void cmd_kill(char *a) {
  if (strcmp(a, " ") == 0 || strlen(a) == 0) {
    println("usage: KILL <pid>");
    return;
  }

  u32 pid;
  if (!parse_u32(a, &pid)) {
    println("usage: KILL <pid>");
    return;
  }
  user_process_kill_pid(pid);
  task_reap_exited();
  refresh_reaped_builtin_tasks();
}

static void cmd_wait(char *a) {
  if (strcmp(a, " ") == 0 || strlen(a) == 0) {
    println("usage: WAIT <pid>");
    return;
  }

  u32 pid;
  if (!parse_u32(a, &pid)) {
    println("usage: WAIT <pid>");
    return;
  }

  user_process_wait_pid(pid);
}

static void cmd_run(char *a) {
  char path[VFS_MAX_PATH];
  u8 magic[4];
  u32 read_len;

  if (strcmp(a, " ") == 0 || strlen(a) == 0) {
    println("usage: RUN <file>");
    return;
  }

  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  if (!fat_read_file_at(path, 0, magic, sizeof(magic), &read_len) ||
      read_len != sizeof(magic)) {
    println("could not read executable");
    return;
  }

  reap_tasks();

  if (magic[0] == 'Y' && magic[1] == 'O' && magic[2] == 'Z' &&
      magic[3] == '1') {
    run_user_file(path);
    return;
  }

  if (magic[0] == ELF_MAGIC0 && magic[1] == ELF_MAGIC1 &&
      magic[2] == ELF_MAGIC2 && magic[3] == ELF_MAGIC3) {
    run_user_elf_file(path);
    return;
  }

  println("unknown executable format");
}

void cmd_vfstest(char *a) {
  UNUSED(a);
  int h = vfs_open("TEST.TXT", USER_O_RDONLY);

  if (h < 0) {
    println("vfs open failed");
    return;
  }

  char buf[8];

  while (1) {
    int n = vfs_read(h, (u8 *)buf, 7);

    if (n < 0) {
      println("vfs read failed");
      break;
    }

    if (n == 0)
      break;

    buf[n] = '\0';
    print(buf);
  }

  println("");
  vfs_close(h);
}

static void cmd_elfinfo(char *a) {
  static u8 elf_buf[8192];
  u32 len;

  if (!a || !a[0]) {
    println("usage: ELFINFO <file>");
    return;
  }

  if (!fat_read_file(a, elf_buf, sizeof(elf_buf), &len)) {
    println("could not read file");
    return;
  }

  if (!elf32_header_ok(elf_buf, len)) {
    println("invalid ELF header");
    return;
  }

  if (!elf32_program_headers_ok(elf_buf, len)) {
    println("invalid ELF program headers");
    return;
  }

  elf32_ehdr_t *h = elf32_header(elf_buf);

  println("valid ELF");

  print("entry=");
  print_u32_dec(h->e_entry);
  println("");

  print("phnum=");
  print_u32_dec(h->e_phnum);
  println("");

  for (u16 i = 0; i < h->e_phnum; i++) {
    elf32_phdr_t *p = elf32_program_header(elf_buf, i);

    if (p->p_type != PT_LOAD)
      continue;

    print("LOAD vaddr=");
    print_u32_dec(p->p_vaddr);

    print(" off=");
    print_u32_dec(p->p_offset);

    print(" filesz=");
    print_u32_dec(p->p_filesz);

    print(" memsz=");
    print_u32_dec(p->p_memsz);

    print(" flags=");
    print_u32_dec(p->p_flags);

    println("");
  }
}

static void cmd_runelf(char *a) {
  char path[VFS_MAX_PATH];

  if (!a || !a[0]) {
    println("usage: RUNELF <file>");
    return;
  }

  if (!shell_resolve_path(a, path)) {
    println("invalid path");
    return;
  }

  run_user_elf_file(path);
}
