#ifndef ELF_H
#define ELF_H

#include "../cpu/types.h"

#define ELF_MAGIC0 0x7F
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'

#define ELFCLASS32 1
#define ELFDATA2LSB 1

#define ET_EXEC 2
#define EM_386 3

#define PT_LOAD 1

#define PF_X 1
#define PF_W 2
#define PF_R 4

typedef struct {
  u32 p_type;
  u32 p_offset;
  u32 p_vaddr;
  u32 p_paddr;
  u32 p_filesz;
  u32 p_memsz;
  u32 p_flags;
  u32 p_align;
} __attribute__((packed)) elf32_phdr_t;

typedef struct {
  u8 e_ident[16];
  u16 e_type;
  u16 e_machine;
  u32 e_version;
  u32 e_entry;
  u32 e_phoff;
  u32 e_shoff;
  u32 e_flags;
  u16 e_ehsize;
  u16 e_phentsize;
  u16 e_phnum;
  u16 e_shentsize;
  u16 e_shnum;
  u16 e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

int elf32_header_ok(u8 *image, u32 len);
int elf32_program_headers_ok(u8 *image, u32 len);

elf32_ehdr_t *elf32_header(u8 *image);
elf32_phdr_t *elf32_program_header(u8 *image, u16 index);

#endif
