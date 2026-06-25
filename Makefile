# YozOs build — "Writing a Simple OS from Scratch" style toolchain (macOS / x86_64-elf-*)

# --- Toolchain --------------------------------------------------------------
CC      := x86_64-elf-gcc
LD      := x86_64-elf-ld
OBJDUMP := x86_64-elf-objdump
ASM     := nasm
QEMU    := qemu-system-i386

# 32-bit, freestanding kernel code (no host libc / startup files)
CFLAGS  := -m32 -ffreestanding -fno-pie -Wall -Wextra
LDFLAGS := -m elf_i386

# --- Boot sector ------------------------------------------------------------
# boot_sect.asm %include's the other .asm files, so it depends on all of them.
BOOT_SRC  := boot/boot_sect.asm
BOOT_DEPS := $(wildcard boot/*.asm)
BOOT_BIN  := boot/boot_sect.bin

$(BOOT_BIN): $(BOOT_DEPS)
	$(ASM) $(BOOT_SRC) -f bin -I boot/ -o $@

# --- C scratch (basic.c demo from the book) ---------------------------------
basic.o: basic.c
	$(CC) $(CFLAGS) -c $< -o $@

basic.bin: basic.o
	$(LD) $(LDFLAGS) -o $@ -Ttext 0x0 --oformat binary $<

# --- Disassembly helpers ----------------------------------------------------
# ELF object (shows symbols, relocations)
%.dis: %.o
	$(OBJDUMP) -d -Mintel $< > $@

# Raw flat binary (no ELF header — tell objdump the arch explicitly)
disasm-boot: $(BOOT_BIN)
	$(OBJDUMP) -D -b binary -mi386 -Mintel,addr16,data16 $(BOOT_BIN)

disasm-basic: basic.bin
	$(OBJDUMP) -D -b binary -mi386 -Mintel $<

# --- Run --------------------------------------------------------------------
run: $(BOOT_BIN)
	$(QEMU) -drive format=raw,file=$(BOOT_BIN),index=0,if=floppy

# --- Phony ------------------------------------------------------------------
.PHONY: all boot basic run disasm-boot disasm-basic clean
all: $(BOOT_BIN)
boot: $(BOOT_BIN)
basic: basic.bin basic.dis

clean:
	rm -f boot/boot_sect.bin basic.o basic.bin basic.dis
