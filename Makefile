# YozOs build — "Writing a Simple OS from Scratch" style toolchain (macOS / i686-elf-*)

# --- Toolchain --------------------------------------------------------------
CC      := i686-elf-gcc
LD      := i686-elf-ld
OBJDUMP := i686-elf-objdump
OBJCOPY := i686-elf-objcopy
ASM     := nasm
QEMU    := qemu-system-i386

# Freestanding kernel code (no host libc / startup files). The i686-elf
# toolchain is natively 32-bit, so -m32 / -m elf_i386 are not needed.
CFLAGS  := -ffreestanding -fno-pie -Wall -Wextra
LDFLAGS :=

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

# Linked ELF: addresses resolved, sections/symbols intact. Best for reading.
basic.elf: basic.o
	$(LD) $(LDFLAGS) -Ttext 0x0 -o $@ $<

# Flat binary, carved out of the ELF.
basic.bin: basic.elf
	$(OBJCOPY) -O binary $< $@

# --- Disassembly helpers ----------------------------------------------------
# Default view: linked code, real addresses, CODE ONLY (data left alone).
basic.dis: basic.elf
	$(OBJDUMP) -d -Mintel $< > $@

# Full view: code AND data, with strings shown as a hex+ASCII dump (no garbage).
disasm-basic: basic.elf
	$(OBJDUMP) -d -Mintel $<
	@echo '--- .rodata (string data) ---'
	$(OBJDUMP) -s -j .rodata $<

# Raw flat boot sector (no ELF header — tell objdump the arch explicitly).
disasm-boot: $(BOOT_BIN)
	$(OBJDUMP) -D -b binary -mi386 -Mintel,addr16,data16 $(BOOT_BIN)

# --- Kernel -----------------------------------------------------------------
# Linked to run at 0x1000 — the address the boot sector loads it to and jumps.
#
# Kernel C sources: top-level kernel + every driver (port I/O lives in
# drivers/low_level.c). basic.c is standalone scratch and is deliberately NOT in
# this list, so it never gets linked into the kernel.
C_SOURCES := kernel.c $(wildcard drivers/*.c)
OBJ       := $(C_SOURCES:.c=.o)

# Generic rule for any kernel C source. -I. lets drivers/*.c include top-level
# headers such as low_level.h. The i686-elf cross-compiler is required: the
# "a"/"d" register constraints in low_level.c are x86-only and fail under host
# clang (arm64).
%.o: %.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

# Entry stub — assembled as an ELF object so the linker can resolve `extern main`.
kernel_entry.o: boot/kernel_entry.asm
	$(ASM) $< -f elf -o $@

# kernel_entry.o MUST come first so the byte at 0x1000 is the stub (which calls
# main), not whatever function gcc happened to place first in kernel.o.
kernel.bin: kernel_entry.o $(OBJ)
	$(LD) $(LDFLAGS) -Ttext 0x1000 --oformat binary -o $@ $^

# Raw flat kernel (no ELF header — tell objdump the arch explicitly). 32-bit,
# so no addr16/data16 (unlike the 16-bit boot sector).
disasm-kernel: kernel.bin
	$(OBJDUMP) -D -b binary -m i386 -Mintel kernel.bin

# --- OS image (bootable disk) -----------------------------------------------
# Disk layout: sector 0 = boot sector, sector 1+ = kernel. The boot sector
# reads 15 sectors after itself, so pad the image to 16 sectors (8 KiB) — a
# too-small image makes the BIOS disk read fail (disk_error).
OS_IMAGE := os-image.bin

$(OS_IMAGE): $(BOOT_BIN) kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=16 2>/dev/null
	dd if=$(BOOT_BIN) of=$@ conv=notrunc 2>/dev/null
	dd if=kernel.bin  of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null

# --- Run --------------------------------------------------------------------
# Boot the full OS image (boot sector + kernel) in QEMU.
run: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),index=0,if=floppy

# Boot only the boot sector (no kernel).
run-boot: $(BOOT_BIN)
	$(QEMU) -drive format=raw,file=$(BOOT_BIN),index=0,if=floppy

# --- Phony ------------------------------------------------------------------
.PHONY: all boot basic kernel image run run-boot disasm-boot disasm-basic disasm-kernel clean
all: $(OS_IMAGE)
boot: $(BOOT_BIN)
basic: basic.bin basic.dis
kernel: kernel.bin
image: $(OS_IMAGE)

clean:
	rm -f boot/boot_sect.bin basic.o basic.elf basic.bin basic.dis \
	      kernel_entry.o kernel.bin $(OBJ) $(OS_IMAGE)
