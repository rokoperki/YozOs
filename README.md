# YozOS

A hobby operating system built entirely from scratch — no GRUB, no Multiboot, no
borrowed kernel code.

## Status

Early development — 16-bit real-mode bootloader. It can now load extra sectors
off the boot drive via BIOS (`int 0x13`) and read the data back, on top of the
earlier stack setup and screen output (hex printing via `int 0x10`, string
printing via BIOS teletype).

## Source

| File               | Role                                                             |
| ------------------ | --------------------------------------------------------------- |
| `boot_sect.asm`    | Boot sector entry — stack setup, disk load, hex dump            |
| `disk_load.asm`    | `disk_load` — reads `DH` sectors into `ES:BX` from drive `DL`   |
| `print_string.asm` | `print_string` (BIOS teletype) and `print_hex` helpers          |

The bootloader is drive-agnostic: it saves the drive number BIOS passes in `DL`
and reads from that, so it works whether booted from a floppy (`0x00`) or a hard
disk / USB (`0x80`).

## Toolchain (macOS)

```bash
brew install i686-elf-gcc i686-elf-binutils nasm qemu
```

- `i686-elf-gcc` — cross-compiler (bare-metal ELF, not the host Clang)
- `nasm` — assembler
- `qemu` — emulator (`qemu-system-i386`)

## Build & Run

Assemble the boot sector:

```bash
nasm -f bin boot_sect.asm -o boot_sect.bin
```

Run it in QEMU:

```bash
qemu-system-i386 -drive format=raw,file=boot_sect.bin
```

To debug the silent triple-faults during early boot, log interrupts and CPU state:

```bash
qemu-system-i386 -drive format=raw,file=boot_sect.bin -d int,cpu
```

## Target Stack

| Piece        | Choice                |
| ------------ | --------------------- |
| Architecture | x86, 32-bit (i686)    |
| Boot         | Legacy BIOS           |
| Assembler    | NASM                  |
| Compiler     | `i686-elf-gcc`        |
| Emulator     | QEMU                  |
| Host         | macOS (Apple Silicon) |
