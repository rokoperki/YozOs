# YozOs

A hobby x86 operating system written from scratch — no GRUB, no Multiboot, no
borrowed kernel. The boot sector is hand-written assembly that brings the CPU up
from 16-bit real mode all the way into a 32-bit C kernel.

Built on macOS (Apple Silicon) with a cross-compiler, following the
*"Writing a Simple Operating System from Scratch"* approach.

## Status

The boot pipeline is complete end to end:

1. BIOS loads the **boot sector** at `0x7c00` in 16-bit real mode.
2. The boot sector sets up a stack and prints a real-mode banner over BIOS
   teletype (`int 0x10`).
3. It reads the **kernel** off disk into memory at `0x1000` via BIOS disk
   services (`int 0x13`).
4. It loads a flat **GDT**, sets the protected-mode bit in `cr0`, and far-jumps
   to flush the prefetch queue.
5. In 32-bit protected mode it reloads the segment registers, resets the stack,
   prints a protected-mode banner straight to **VGA memory** (`0xb8000`), and
   `call`s into the kernel at `0x1000`.
6. The kernel entry stub calls `main()`, which writes a character cell directly
   to the VGA text buffer.

The kernel itself is still a stub — the milestone here is the boot path, not OS
features.

## Boot flow

```
BIOS ──► 0x7c00  boot_sect.asm   [16-bit real mode]
                 │ set up stack, print banner (int 0x10)
                 │ load_kernel ──► disk_load.asm  (int 0x13)  ─► kernel @ 0x1000
                 │ switch_to_pm ─► pm_switch.asm
                 │     cli ; lgdt [gdt_descriptor] ; cr0 |= 1
                 │     jmp CODE_SEG:init_pm   ◄── far jump flushes prefetch
                 ▼
        init_pm  [32-bit protected mode]
                 │ reload ds/ss/es/fs/gs = DATA_SEG ; reset stack @ 0x90000
                 │ print banner ─► print_string_pm.asm  (writes to 0xb8000)
                 │ call 0x1000 ──► kernel_entry.asm stub
                 ▼
             main()  in kernel.c  ─► writes to VGA buffer (0xb8000)
```

## Layout

### Boot sector — `boot/`

`boot_sect.asm` is the entry point; it `%include`s the other files, so they are
assembled into one flat 512-byte binary.

| File                  | Role                                                                                   |
| --------------------- | -------------------------------------------------------------------------------------- |
| `boot_sect.asm`       | Entry: stack setup, real-mode banner, `load_kernel`, `switch_to_pm`, `call` into kernel |
| `disk_load.asm`       | `disk_load` — read `DH` sectors into `ES:BX` from drive `DL` (`int 0x13`), error check  |
| `gdt.asm`             | Flat Global Descriptor Table (null / code / data) + `gdt_descriptor`, `CODE_SEG`/`DATA_SEG` |
| `pm_switch.asm`       | `switch_to_pm` — `cli`, `lgdt`, set `cr0.PE`, far-jump, reload segment registers        |
| `print_string.asm`    | `print_string` — real-mode string print via BIOS teletype (`int 0x10`)                  |
| `print_string_pm.asm` | `print_string_pm` — write a string to VGA memory (`0xb8000`) in 32-bit mode             |
| `print_hex.asm`       | `print_hex` — print a 16-bit value as hex (real mode); a debug aid, *not* in the boot path |
| `kernel_entry.asm`    | 32-bit stub linked first so `0x1000` is guaranteed to `call main`                       |

### Kernel & scratch

| File       | Role                                                                              |
| ---------- | --------------------------------------------------------------------------------- |
| `kernel.c` | The 32-bit C kernel — `main()`, linked at `0x1000`, writes to VGA memory          |
| `basic.c`  | Standalone C scratch, for studying compiler output / disassembly (not booted)     |

## Toolchain (macOS)

```bash
brew install i686-elf-gcc i686-elf-binutils nasm qemu
```

- `i686-elf-gcc` / `i686-elf-binutils` — bare-metal 32-bit ELF cross toolchain
  (natively 32-bit, so no `-m32` / `-m elf_i386` needed; *not* host Clang)
- `nasm` — assembler
- `qemu` — emulator (`qemu-system-i386`)

## Build & run

Everything is driven by the `Makefile`.

```bash
make            # build the bootable disk image (os-image.bin)
make run        # build the image and boot it in QEMU
make run-boot   # boot only the boot sector (no kernel)
make clean      # remove all build artifacts
```

Targeted builds:

```bash
make boot       # assemble the boot sector  -> boot/boot_sect.bin
make kernel     # compile + link the kernel -> kernel.bin
make basic      # build the basic.c scratch -> basic.bin + basic.dis
```

### Disk image layout

`os-image.bin` is a 16-sector (8 KiB) raw image:

- **sector 0** — the boot sector
- **sectors 1+** — the kernel

The boot sector reads the 15 sectors *after* itself, so the image is zero-padded
to 16 sectors. An image that is too small makes the BIOS disk read fail and trips
`disk_error`.

## Disassembly

```bash
make disasm-boot    # disassemble the flat boot sector (16-bit real mode)
make disasm-kernel  # disassemble the flat kernel binary (32-bit)
make disasm-basic   # disassemble basic.elf code + dump .rodata strings
make basic.dis      # write code-only disassembly to basic.dis
```

The boot sector and kernel are flat binaries with no ELF header, so `objdump` is
told the architecture explicitly (`addr16,data16` for the 16-bit boot sector;
plain `i386` for the 32-bit kernel).

## Debugging

Triple-faults during the protected-mode switch are silent. Log interrupts and CPU
state to see where it dies:

```bash
qemu-system-i386 -drive format=raw,file=os-image.bin -d int,cpu
```

## Target stack

| Piece        | Choice                |
| ------------ | --------------------- |
| Architecture | x86, 32-bit (i686)    |
| Boot         | Legacy BIOS           |
| Assembler    | NASM                  |
| Compiler     | `i686-elf-gcc`        |
| Emulator     | QEMU                  |
| Host         | macOS (Apple Silicon) |
