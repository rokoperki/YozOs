# YozOS

A hobby operating system built entirely from scratch — no GRUB, no Multiboot, no
borrowed kernel code.

## Status

Early development — the boot sector boots in 16-bit real mode, loads the kernel
off disk via BIOS, then switches the CPU into 32-bit protected mode and jumps to
C. In detail: it sets up a stack, prints a real-mode message via BIOS teletype,
reads the kernel from disk into memory at `0x1000`, loads a Global Descriptor
Table (flat code/data segments), sets the protected-mode bit in `cr0`, far-jumps
to flush the prefetch pipeline, reloads the segment registers, and calls into the
C kernel. The current `kernel.c` writes directly to VGA memory (`0xb8000`).

## Source

### Boot sector (`boot/`)

| File                  | Role                                                                                |
| --------------------- | ----------------------------------------------------------------------------------- |
| `boot_sect.asm`       | Boot sector entry — stack setup, real-mode message, `load_kernel`, PM switch, jump to C |
| `disk_load.asm`       | `disk_load` — reads `DH` sectors into `ES:BX` from drive `DL` (`int 0x13`)            |
| `gdt.asm`             | Global Descriptor Table (null/code/data) and `gdt_descriptor`                       |
| `pm_switch.asm`       | `switch_to_pm` — clear interrupts, `lgdt`, set `cr0`, far-jump, reload segments      |
| `print_string_pm.asm` | `print_string_pm` — writes a string to VGA memory (`0xb8000`) in 32-bit mode         |
| `print_string.asm`    | `print_string` — real-mode string printing via BIOS teletype (`int 0x10`)            |
| `print_hex.asm`       | `print_hex` — prints a 16-bit value as hex via `int 0x10`                            |

`print_hex` is not wired into the current boot path — it remains from the earlier
real-mode work as a debugging aid.

### Kernel & demos

| File       | Role                                                                            |
| ---------- | ------------------------------------------------------------------------------- |
| `kernel.c` | The 32-bit C kernel — entry `main`, linked at `0x1000`, writes to VGA memory    |
| `basic.c`  | Standalone C scratch from the book, for studying compiler output / disassembly  |

## Toolchain (macOS)

```bash
brew install i686-elf-gcc i686-elf-binutils nasm qemu
```

- `i686-elf-gcc` / `i686-elf-binutils` — cross toolchain (bare-metal 32-bit ELF,
  not the host Clang)
- `nasm` — assembler
- `qemu` — emulator (`qemu-system-i386`)

## Build & Run

Everything goes through the `Makefile`.

```bash
make            # build the full bootable disk image (os-image.bin)
make run        # build the image and boot it in QEMU
make run-boot   # boot only the boot sector (no kernel)
make clean      # remove all build artifacts
```

Targeted builds:

```bash
make boot       # assemble the boot sector  -> boot/boot_sect.bin
make kernel     # compile + link the kernel -> kernel.bin
make basic      # build the basic.c demo    -> basic.bin + basic.dis
```

### Disk layout

`os-image.bin` is a 16-sector (8 KiB) image: sector 0 is the boot sector, sector
1+ holds the kernel. The boot sector reads 15 sectors after itself, so the image
is padded to 16 — a too-small image makes the BIOS disk read fail (`disk_error`).

## Disassembly

```bash
make disasm-boot   # disassemble the flat boot sector (16-bit real mode)
make disasm-basic  # disassemble basic.elf code + dump .rodata strings
make basic.dis     # write code-only disassembly to basic.dis
```

## Debugging

To debug silent triple-faults during the protected-mode switch, log interrupts
and CPU state:

```bash
qemu-system-i386 -drive format=raw,file=os-image.bin -d int,cpu
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
