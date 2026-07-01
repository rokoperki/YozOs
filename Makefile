# YozOs build — "Writing a Simple OS from Scratch" style toolchain (macOS / i686-elf-*)

.DEFAULT_GOAL := all

# --- Toolchain --------------------------------------------------------------
CC      := i686-elf-gcc
LD      := i686-elf-ld
OBJDUMP := i686-elf-objdump
OBJCOPY := i686-elf-objcopy
ASM     := nasm
QEMU    := qemu-system-i386

# Freestanding kernel code (no host libc / startup files).
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

basic.elf: basic.o
	$(LD) $(LDFLAGS) -Ttext 0x0 -o $@ $<

basic.bin: basic.elf
	$(OBJCOPY) -O binary $< $@

# --- Disassembly helpers ----------------------------------------------------
basic.dis: basic.elf
	$(OBJDUMP) -d -Mintel $< > $@

disasm-basic: basic.elf
	$(OBJDUMP) -d -Mintel $<
	@echo '--- .rodata (string data) ---'
	$(OBJDUMP) -s -j .rodata $<

disasm-boot: $(BOOT_BIN)
	$(OBJDUMP) -D -b binary -mi386 -Mintel,addr16,data16 $(BOOT_BIN)

# --- Kernel -----------------------------------------------------------------
# Linked to run at 0x1000, the address the boot sector loads it to and jumps.
# basic.c is standalone scratch and is deliberately NOT in this list.
C_SOURCES := kernel.c $(wildcard drivers/*.c) $(wildcard cpu/*.c) $(wildcard kernel/*.c) $(wildcard memory/*.c) $(wildcard task/*.c)
OBJ       := $(C_SOURCES:.c=.o)

# Standalone assembly linked into the kernel (NOT %include'd into the boot
# sector). cpu/interupt.asm references `extern isr_handler`, so it must be ELF.
ASM_SOURCES := $(wildcard cpu/*.asm) $(wildcard memory/*.asm) $(wildcard task/*.asm)
ASM_OBJ     := $(ASM_SOURCES:.asm=.o)

# -I. lets drivers/*.c include top-level headers such as low_level.h.
%.o: %.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

kernel_entry.o: boot/kernel_entry.asm
	$(ASM) $< -f elf -o $@

%.o: %.asm
	$(ASM) $< -f elf -o $@

# kernel_entry.o MUST come first so the byte at 0x1000 is the stub (calls main).
kernel.bin: kernel_entry.o $(OBJ) $(ASM_OBJ)
	$(LD) $(LDFLAGS) -Ttext 0x1000 --oformat binary -o $@ $^

disasm-kernel: kernel.bin
	$(OBJDUMP) -D -b binary -m i386 -Mintel kernel.bin

# --- OS image (bootable disk) -----------------------------------------------
# Disk layout: sector 0 = boot sector, sector 1+ = kernel. Pad to 16 sectors
# (8 KiB); a too-small image makes the BIOS disk read fail (disk_error).
OS_IMAGE := os-image.bin

$(OS_IMAGE): $(BOOT_BIN) kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=16 2>/dev/null
	dd if=$(BOOT_BIN) of=$@ conv=notrunc 2>/dev/null
	dd if=kernel.bin  of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null

# --- Run --------------------------------------------------------------------
run: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),index=0,if=floppy

run-boot: $(BOOT_BIN)
	$(QEMU) -drive format=raw,file=$(BOOT_BIN),index=0,if=floppy

# --- Flash to USB -----------------------------------------------------------
# Write the bootable image to a physical USB stick (Legacy/CSM, Secure Boot off).
# DISK auto-detects the first external physical disk; override on the command
# line if you have more than one. Must be the WHOLE disk (/dev/diskN), never a
# partition. This ERASES the target disk (a y/N prompt guards it first).
DISK    ?= $(shell diskutil list external physical 2>/dev/null | awk '/^\/dev\/disk/{print $$1; exit}')
USB_RAW  = $(patsubst /dev/disk%,/dev/rdisk%,$(DISK))

usb: $(OS_IMAGE)
	@test -n "$(DISK)" || { echo "No external USB found. Plug one in, or pass DISK=/dev/diskN."; exit 1; }
	@printf ">>> ERASE %s and write $(OS_IMAGE)? [y/N] " "$(DISK)"; \
	  read ans; [ "$$ans" = y ] || [ "$$ans" = Y ] || { echo "aborted."; exit 1; }
	diskutil unmountDisk $(DISK)
	sudo dd if=$(OS_IMAGE) of=$(USB_RAW) bs=512 conv=sync
	diskutil eject $(DISK)

# --- Phony ------------------------------------------------------------------
.PHONY: all boot basic kernel image run run-boot usb disasm-boot disasm-basic disasm-kernel clean
all: $(OS_IMAGE)
	@printf ">>> %s built: %s bytes (%s KiB)\n" "$(OS_IMAGE)" \
	  "$$(stat -f%z $(OS_IMAGE))" "$$(( $$(stat -f%z $(OS_IMAGE)) / 1024 ))"
boot: $(BOOT_BIN)
basic: basic.bin basic.dis
kernel: kernel.bin
image: $(OS_IMAGE)

clean:
	rm -f boot/boot_sect.bin basic.o basic.elf basic.bin basic.dis \
	      kernel_entry.o kernel.bin $(OBJ) $(ASM_OBJ) $(OS_IMAGE)
