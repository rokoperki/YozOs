# YozOS

A hobby operating system built entirely from scratch — no GRUB, no Multiboot, no
borrowed kernel code.

## Status

Early development — the boot sector boots in 16-bit real mode, prints a message
via BIOS teletype, then switches the CPU into 32-bit protected mode. It loads a
Global Descriptor Table (flat code/data segments), sets the protected-mode bit in
`cr0`, far-jumps to flush the prefetch pipeline, reloads the segment registers,
and prints directly to VGA memory from 32-bit code.

## Source

| File                  | Role                                                                                |
| --------------------- | ----------------------------------------------------------------------------------- |
| `boot_sect.asm`       | Boot sector entry — stack setup, real-mode message, PM switch, 32-bit `BEGIN_PM`     |
| `gdt.asm`             | Global Descriptor Table (null/code/data) and `gdt_descriptor`                       |
| `pm_switch.asm`       | `switch_to_pm` — clear interrupts, `lgdt`, set `cr0`, far-jump, reload segments      |
| `print_string_pm.asm` | `print_string_pm` — writes a string to VGA memory (`0xb8000`) in 32-bit mode         |
| `print_string.asm`    | `print_string` — real-mode string printing via BIOS teletype (`int 0x10`)            |
| `print_hex.asm`       | `print_hex` — prints a 16-bit value as hex via `int 0x10`                            |
| `disk_load.asm`       | `disk_load` — reads `DH` sectors into `ES:BX` from drive `DL` (`int 0x13`)            |

`disk_load` and `print_hex` are not wired into the current boot path — they remain
from the earlier real-mode work and will be reused once a kernel is loaded off disk.

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

To debug silent triple-faults during the protected-mode switch, log interrupts
and CPU state:

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
