# YozOs build — "Writing a Simple OS from Scratch" style toolchain (macOS / i686-elf-*)

.DEFAULT_GOAL := all

# --- Toolchain --------------------------------------------------------------
CC      := i686-elf-gcc
LD      := i686-elf-ld
OBJDUMP := i686-elf-objdump
OBJCOPY := i686-elf-objcopy
ASM     := nasm
QEMU    := qemu-system-i386
PYTHON  := python3

# Freestanding kernel code (no host libc / startup files).
CFLAGS  := -ffreestanding -fno-pie -Wall -Wextra -MMD -MP
LDFLAGS :=

# --- Boot sector ------------------------------------------------------------
# boot_sect.asm %include's the other .asm files, so it depends on all of them.
BOOT_SRC  := boot/boot_sect.asm
BOOT_DEPS := $(wildcard boot/*.asm)
BOOT_BIN  := boot/boot_sect.bin
KERNEL_SECTORS_INC := boot/kernel_sectors.inc

$(BOOT_BIN): $(BOOT_DEPS) $(KERNEL_SECTORS_INC)
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
# Linked to run at 0x10000, the address the boot sector loads it to and jumps.
# basic.c is standalone scratch and is deliberately NOT in this list.
C_SOURCES := kernel.c $(wildcard drivers/*.c) $(wildcard cpu/*.c) $(wildcard kernel/*.c) $(wildcard memory/*.c) $(wildcard task/*.c) $(wildcard fs/*.c) $(wildcard shell/*.c) $(wildcard elf/*.c)
OBJ       := $(C_SOURCES:.c=.o)
DEPS      := $(OBJ:.o=.d)

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

# kernel_entry.o MUST come first so the byte at 0x10000 is the stub (calls main).
kernel.bin: kernel_entry.o $(OBJ) $(ASM_OBJ)
	$(LD) $(LDFLAGS) -Ttext 0x10000 --oformat binary -o $@ $^
	@printf ">>> %s built: %s bytes (%s sectors)\n" "$@" \
	  "$$(stat -f%z $@)" "$$(( ( $$(stat -f%z $@) + 511 ) / 512 ))"

$(KERNEL_SECTORS_INC): kernel.bin
	@sectors=$$(( ( $$(stat -f%z $<) + 511 ) / 512 )); \
	  test $$sectors -le 255 || \
	    { echo "ERROR: kernel.bin too large for boot loader read count (max 255 sectors)"; exit 1; }; \
	  printf "KERNEL_SECTORS equ %s\n" "$$sectors" > $@; \
	  printf ">>> %s generated: %s sectors\n" "$@" "$$sectors"

disasm-kernel: kernel.bin
	$(OBJDUMP) -D -b binary -m i386 -Mintel kernel.bin

# --- OS image (bootable disk) -----------------------------------------------
# Disk layout: sector 0 = boot sector, sector 1+ = kernel. Pad to 16 sectors
# (8 KiB); a too-small image makes the BIOS disk read fail (disk_error).
OS_IMAGE := os-image.bin

$(OS_IMAGE): $(BOOT_BIN) kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(BOOT_BIN) of=$@ conv=notrunc 2>/dev/null
	dd if=kernel.bin  of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null

# --- Data disk (ATA PIO target) ---------------------------------------------
DISK_IMG := disk.img

$(DISK_IMG):
	$(PYTHON) tools/fat16_mkfs.py $@ 2048

reset-user-disk:
	$(PYTHON) tools/fat16_mkfs.py $(DISK_IMG) 2048

# --- User programs ----------------------------------------------------------
USER_DISK_DIR := TEST
USER_HELLO_BIN := user/HELLO.BIN
USER_ECHO_BIN  := user/ECHO.BIN
USER_FAULT_BIN := user/FAULT.BIN
USER_PID_BIN := user/PID.BIN
USER_WAITSELF_BIN := user/WAITSELF.BIN
USER_PPID_BIN := user/PPID.BIN
USER_KILLSELF_BIN := user/KILLSELF.BIN
USER_READFILE_BIN := user/READFILE.BIN
USER_ECHOFD_BIN := user/ECHOFD.BIN
USER_STAT_BIN := user/STAT.BIN
USER_WRITEFILE_BIN := user/WRITEF.BIN
USER_APPEND_BIN := user/APPEND.BIN
USER_SEEK_BIN := user/SEEK.BIN
USER_OVERWRITE_BIN := user/OVERWR.BIN
USER_PATH_BIN := user/PATH.BIN
USER_DIRSTAT_BIN := user/DIRSTAT.BIN
USER_DIRTEST_BIN := user/DIRTEST.BIN
USER_NESTDIR_BIN := user/NESTDIR.BIN
USER_ELFHELLO_OBJ := user/elfhello.o
USER_ELFHELLO_ELF := user/HELLO.ELF
USER_BINS      := $(USER_HELLO_BIN) $(USER_ECHO_BIN) $(USER_FAULT_BIN) $(USER_PID_BIN) $(USER_WAITSELF_BIN) $(USER_PPID_BIN) $(USER_KILLSELF_BIN) $(USER_READFILE_BIN) $(USER_ECHOFD_BIN) $(USER_STAT_BIN) $(USER_WRITEFILE_BIN) $(USER_APPEND_BIN) $(USER_SEEK_BIN) $(USER_OVERWRITE_BIN) $(USER_PATH_BIN) $(USER_DIRSTAT_BIN) $(USER_DIRTEST_BIN) $(USER_NESTDIR_BIN)
USER_ELFS      := $(USER_ELFHELLO_ELF)

$(USER_HELLO_BIN): user/hello.asm
	$(ASM) $< -f bin -o $@

$(USER_ECHO_BIN): user/echo.asm
	$(ASM) $< -f bin -o $@

$(USER_FAULT_BIN): user/fault.asm
	$(ASM) $< -f bin -o $@

$(USER_PID_BIN): user/pid.asm
		$(ASM) $< -f bin -o $@

$(USER_WAITSELF_BIN): user/waitself.asm
		$(ASM) $< -f bin -o $@

$(USER_PPID_BIN): user/ppid.asm
		$(ASM) $< -f bin -o $@

$(USER_KILLSELF_BIN): user/killself.asm
		$(ASM) $< -f bin -o $@

$(USER_READFILE_BIN): user/readfile.asm
	$(ASM) $< -f bin -o $@

$(USER_ECHOFD_BIN): user/echofd.asm
	$(ASM) $< -f bin -o $@

$(USER_STAT_BIN): user/stat.asm
	$(ASM) $< -f bin -o $@

$(USER_WRITEFILE_BIN): user/writefile.asm
	$(ASM) $< -f bin -o $@

$(USER_APPEND_BIN): user/append.asm
	$(ASM) $< -f bin -o $@

$(USER_SEEK_BIN): user/seek.asm
	$(ASM) $< -f bin -o $@

$(USER_OVERWRITE_BIN): user/overwrite.asm
	$(ASM) $< -f bin -o $@

$(USER_PATH_BIN): user/path.asm
	$(ASM) $< -f bin -o $@

$(USER_DIRSTAT_BIN): user/dirstat.asm
	$(ASM) $< -f bin -o $@

$(USER_DIRTEST_BIN): user/dirtest.asm
	$(ASM) $< -f bin -o $@

$(USER_NESTDIR_BIN): user/nestdir.asm
	$(ASM) $< -f bin -o $@

$(USER_ELFHELLO_OBJ): user/elfhello.asm
	$(ASM) $< -f elf -o $@

$(USER_ELFHELLO_ELF): $(USER_ELFHELLO_OBJ)
	$(LD) -Ttext 0x8048000 -o $@ $<

user-programs: $(USER_BINS) $(USER_ELFS)

install-hello: $(DISK_IMG) $(USER_HELLO_BIN)
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_HELLO_BIN) $(USER_DISK_DIR)/HELLO.BIN

install-user-programs: $(DISK_IMG) $(USER_BINS) $(USER_ELFS)
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_HELLO_BIN) $(USER_DISK_DIR)/HELLO.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_ECHO_BIN) $(USER_DISK_DIR)/ECHO.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_FAULT_BIN) $(USER_DISK_DIR)/FAULT.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_PID_BIN) $(USER_DISK_DIR)/PID.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_WAITSELF_BIN) $(USER_DISK_DIR)/WAITSELF.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_PPID_BIN) $(USER_DISK_DIR)/PPID.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_KILLSELF_BIN) $(USER_DISK_DIR)/KILLSELF.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_READFILE_BIN) $(USER_DISK_DIR)/READFILE.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_ECHOFD_BIN) $(USER_DISK_DIR)/ECHOFD.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_STAT_BIN) $(USER_DISK_DIR)/STAT.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_WRITEFILE_BIN) $(USER_DISK_DIR)/WRITEF.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_APPEND_BIN) $(USER_DISK_DIR)/APPEND.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_SEEK_BIN) $(USER_DISK_DIR)/SEEK.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_OVERWRITE_BIN) $(USER_DISK_DIR)/OVERWR.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_PATH_BIN) $(USER_DISK_DIR)/PATH.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_DIRSTAT_BIN) $(USER_DISK_DIR)/DIRSTAT.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_DIRTEST_BIN) $(USER_DISK_DIR)/DIRTEST.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_NESTDIR_BIN) $(USER_DISK_DIR)/NESTDIR.BIN
	$(PYTHON) tools/fat16_put.py $(DISK_IMG) $(USER_ELFHELLO_ELF) $(USER_DISK_DIR)/HELLO.ELF

prepare-user-disk: $(OS_IMAGE) reset-user-disk
	$(MAKE) install-user-programs

# --- Run --------------------------------------------------------------------
run: $(OS_IMAGE)
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),index=0,if=floppy

# `-boot order=a` = boot the floppy first. Without it, SeaBIOS tries the IDE
# disk first; a FAT-formatted disk.img has a boot sector that just prints
# "This is not a bootable disk" and never falls through to the floppy.
run-disk: $(OS_IMAGE) $(DISK_IMG)
	$(QEMU) -display cocoa,zoom-to-fit=on,full-screen=off \
	        -boot order=a \
	        -drive format=raw,file=$(OS_IMAGE),index=0,if=floppy \
	        -drive format=raw,file=$(DISK_IMG),if=ide,index=0

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
-include $(DEPS)

.PHONY: all boot basic kernel image run run-disk run-boot usb user-programs install-hello install-user-programs prepare-user-disk reset-user-disk disasm-boot disasm-basic disasm-kernel clean
all: $(OS_IMAGE)
	@printf ">>> %s built: %s bytes (%s KiB)\n" "$(OS_IMAGE)" \
	  "$$(stat -f%z $(OS_IMAGE))" "$$(( $$(stat -f%z $(OS_IMAGE)) / 1024 ))"
boot: $(BOOT_BIN)
basic: basic.bin basic.dis
kernel: kernel.bin
image: $(OS_IMAGE)

clean:
	rm -f boot/boot_sect.bin basic.o basic.elf basic.bin basic.dis \
	      $(KERNEL_SECTORS_INC) kernel_entry.o kernel.bin $(OBJ) $(DEPS) $(ASM_OBJ) \
	      $(OS_IMAGE) $(USER_BINS) $(USER_ELFHELLO_OBJ) $(USER_ELFS)
