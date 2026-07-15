#include "elf.h"

int elf32_header_ok(u8 *image, u32 len) {
  if (!image)
    return 0;

  if (len < sizeof(elf32_ehdr_t))
    return 0;

  elf32_ehdr_t *h = (elf32_ehdr_t *)image;

  if (h->e_ident[0] != ELF_MAGIC0 || h->e_ident[1] != ELF_MAGIC1 ||
      h->e_ident[2] != ELF_MAGIC2 || h->e_ident[3] != ELF_MAGIC3)
    return 0;

  if (h->e_ident[4] != ELFCLASS32)
    return 0;

  if (h->e_ident[5] != ELFDATA2LSB)
    return 0;

  if (h->e_type != ET_EXEC)
    return 0;

  if (h->e_machine != EM_386)
    return 0;

  if (h->e_phoff == 0)
    return 0;

  if (h->e_phnum == 0)
    return 0;

  if (h->e_phentsize == 0)
    return 0;

  if (h->e_phoff + ((u32)h->e_phnum * h->e_phentsize) > len)
    return 0;

  return 1;
}

int elf32_program_headers_ok(u8 *image, u32 len) {
  if (!elf32_header_ok(image, len))
    return 0;

  elf32_ehdr_t *h = (elf32_ehdr_t *)image;

  for (u16 i = 0; i < h->e_phnum; i++) {
    u32 off = h->e_phoff + ((u32)i * h->e_phentsize);

    if (off + sizeof(elf32_phdr_t) > len)
      return 0;

    elf32_phdr_t *p = (elf32_phdr_t *)(image + off);

    if (p->p_type != PT_LOAD)
      continue;

    if (p->p_filesz > p->p_memsz)
      return 0;

    if (p->p_offset + p->p_filesz < p->p_offset)
      return 0;

    if (p->p_offset + p->p_filesz > len)
      return 0;

    if (p->p_memsz == 0)
      return 0;

    if (p->p_vaddr == 0)
      return 0;

    if (p->p_vaddr + p->p_memsz < p->p_vaddr)
      return 0;

    if ((p->p_flags & (PF_R | PF_W | PF_X)) == 0)
      return 0;
  }

  return 1;
}

elf32_ehdr_t *elf32_header(u8 *image) { return (elf32_ehdr_t *)image; }

elf32_phdr_t *elf32_program_header(u8 *image, u16 index) {
  elf32_ehdr_t *h = elf32_header(image);
  return (elf32_phdr_t *)(image + h->e_phoff + ((u32)index * h->e_phentsize));
}
