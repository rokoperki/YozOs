# YozOS

A hobby operating system built entirely from scratch — no GRUB, no Multiboot, no
borrowed kernel code.

## Status

Early development - 16bit loop_bootloader

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
nasm -f bin boot.asm -o boot.bin
```

Run it in QEMU:

```bash
qemu-system-i386 -drive format=raw,file=boot.bin
```

To debug the silent triple-faults during early boot, log interrupts and CPU state:

```bash
qemu-system-i386 -drive format=raw,file=boot.bin -d int,cpu
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
