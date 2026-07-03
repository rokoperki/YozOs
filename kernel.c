#include "cpu/isr.h"
#include "cpu/timer.h"
#include "drivers/ata.h"
#include "drivers/keyboard.h"
#include "drivers/screen.h"
#include "fs/fat.h"
#include "kernel/function.h"
#include "kernel/string.h"
#include "memory/frame_alloc.h"
#include "memory/memory_map.h"
#include "memory/paging.h"
#include "task/task.h"

int main() {
  clear_screen();

  print("\n");
  print("  __    __    ______   ________   ______    ______   \n");
  print(" /  |  /  |  /      \\ /        | /      \\  /      \\  \n");
  print(" $$ |  $$ | /$$$$$$  |$$$$$$$$/ /$$$$$$  |/$$$$$$  | \n");
  print(" $$ |  $$ | $$ |  $$ |   /$$/  $$ |  $$ |$$ \\__$$/  \n");
  print(" $$  \\/$$/  $$ |  $$ |  /$$/   $$ |  $$ |$$      \\   \n");
  print("  $$  $$<   $$ |  $$ | /$$/    $$ |  $$ | $$$$$$  |  \n");
  print("   $$$$  \\  $$ \\__$$ |/$$/____ $$ \\__$$ |/  \\__$$ |  \n");
  print("    $$  |   $$    $$/ $$       |$$    $$/ $$    $$/   \n");
  print("    $$/      $$$$$$/  $$$$$$$$/  $$$$$$/   $$$$$$/    \n");
  print("\n");
  print("  ======================================================= \n");
  print("  ======================================================= \n");
  print("\n");
  print("   [ ok ] boot sector\n");
  print("   [ ok ] protected mode\n");
  print("   [ ok ] screen driver online\n");
  print("\n");

  isr_install();

  print("yozOS > ");
  init_keyboard();
  init_timer(50);

  init_frames();
  init_paging();

  fs_init();

  __asm__ __volatile__("sti");
  return 0;
}

void user_input(char *input) {
  if (strcmp(input, "END") == 0) {
    print("Stopping the CPU. Bye!\n");
    asm volatile("hlt");
  } else if (strcmp(input, "MMAP") == 0) {
    memory_map_print();
  } else if (strcmp(input, "FALLOC") == 0) {
    char buf[50];
    for (int i = 0; i < 4; i++) {
      u32 addr = alloc_frame();
      print_u64(addr, buf);
      if (i == 2) {
        free_frame(addr);
      }
      println("");
    }
  } else if (strcmp(input, "PTEST") == 0) {
    u32 *bad = (u32 *)0x800000; // 8 MiB — PDE[1], not present
    u32 x = *bad;               // ← page fault fires here
    UNUSED(x);
  } else if (strcmp(input, "TASKTEST") == 0) {
    test_task();
  } else if (strcmp(input, "IDENT") == 0) {
    ata_identify();

  } else if (strcmp(input, "RSECT") == 0) {
    u16 sector[256];
    ata_read(0, 1, sector);
    println((char *)sector);
    for (int i = 0; i < 256; i++) {
      char buff[10];
      hex16_to_ascii(sector[i], buff);
      print(buff);
      print(" ");
    }
  } else if (strcmp(input, "WSECT") == 0) {
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
  } else if (strcmp(input, "FSINFO") == 0) {
    fs_info();
  } else if (strcmp(input, "LS") == 0) {
    fs_ls();
  } else if (starts_with(input, "CAT ")) {
    fs_cat(input + 4);
  } else if (starts_with(input, "CREATE")) {
    fs_create(input + 7);
  } else if (starts_with(input, "WRITE ")) {
    char *rest = input + 6;

    int i = 0;
    while (rest[i] && rest[i] != ' ')
      i++;
    if (rest[i] == ' ') {
      rest[i] = '\0';
      fs_write(rest, rest + i + 1);
    }
  }
}
