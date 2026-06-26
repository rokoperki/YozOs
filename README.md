# YozOs

A hobby x86 operating system written from scratch — no GRUB, no Multiboot, no
borrowed kernel. The boot sector is hand-written assembly that brings the CPU up
from 16-bit real mode all the way into a 32-bit C kernel.

Built on macOS (Apple Silicon) with a cross-compiler, following the
_"Writing a Simple Operating System from Scratch"_ approach.

## Status

The boot pipeline is complete end to end, the kernel drives the screen through a
proper **C driver** instead of poking video memory by hand, and it now installs
an **Interrupt Descriptor Table** and handles CPU exceptions:

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
6. The kernel entry stub calls `main()`, which uses the **screen driver** to
   clear the screen and print via `print()` / `print_at()`.
7. `main()` then calls `isr_install()` to build and `lidt` the **IDT**, wiring
   the 32 CPU-exception vectors to assembly stubs that funnel into a single C
   handler. A couple of software interrupts (`int $2`, `int $3`) fire as a smoke
   test, printing the interrupt number and name.

The current milestone is interrupts: a 256-entry IDT
(`cpu/idt.c`), per-vector assembly stubs (`cpu/interupt.asm`) that push a uniform
stack frame, and a C dispatcher (`cpu/isr.c`) that names the exception. Only the
32 CPU exceptions are handled so far — hardware IRQs (the PIC remap) come next.

YozOs also **boots on real x86 hardware** — write the image to a USB stick with
`make usb` and boot any PC that still supports Legacy BIOS / CSM. See
[Booting on real hardware](#booting-on-real-hardware).

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
             main()  in kernel.c
                 │ clear_screen() / print()
                 │ isr_install() ─► cpu/idt.c   set_idt_gate ×32 ; lidt
                 │     int $2 / int $3  ──► idt[n] ─► cpu/interupt.asm (isrN)
                 │                                      │ push int_no, save state
                 │                                      ▼
                 │                              cpu/isr.c  isr_handler(registers_t)
                 │                                      ─► print int_no + name
                 ▼
        drivers/screen.c  ─► chars to VGA buffer (0xb8000)
                          ─► cursor via CRTC ports (0x3d4/0x3d5)
                                 │
                                 ▼
        drivers/low_level.c  ─► port_byte_in / port_byte_out (in/out)
```

## Layout

### Boot sector — `boot/`

`boot_sect.asm` is the entry point; it `%include`s the other files, so they are
assembled into one flat 512-byte binary.

| File                  | Role                                                                                                       |
| --------------------- | ---------------------------------------------------------------------------------------------------------- |
| `boot_sect.asm`       | Entry: zero segment regs, stack setup, real-mode banner, `load_kernel`, `switch_to_pm`, `call` into kernel |
| `disk_load.asm`       | `disk_load` — read `DH` sectors into `ES:BX` from drive `DL` (`int 0x13`), error check                     |
| `gdt.asm`             | Flat Global Descriptor Table (null / code / data) + `gdt_descriptor`, `CODE_SEG`/`DATA_SEG`                |
| `pm_switch.asm`       | `switch_to_pm` — `cli`, `lgdt`, set `cr0.PE`, far-jump, reload segment registers                           |
| `print_string.asm`    | `print_string` — real-mode string print via BIOS teletype (`int 0x10`)                                     |
| `print_string_pm.asm` | `print_string_pm` — write a string to VGA memory (`0xb8000`) in 32-bit mode                                |
| `print_hex.asm`       | `print_hex` — print a 16-bit value as hex (real mode); a debug aid, _not_ in the boot path                 |
| `kernel_entry.asm`    | 32-bit stub linked first so `0x1000` is guaranteed to `call main`                                          |

### Kernel & scratch

| File       | Role                                                                                       |
| ---------- | ------------------------------------------------------------------------------------------ |
| `kernel.c` | The 32-bit C kernel — `main()`, linked at `0x1000`, drives the screen and installs the IDT |
| `basic.c`  | Standalone C scratch, for studying compiler output / disassembly (not booted)              |

### Drivers — `drivers/`

| File             | Role                                                                                       |
| ---------------- | ------------------------------------------------------------------------------------------ |
| `screen.c/.h`    | VGA text-mode screen driver — `print`, `print_at`, `clear_screen`, cursor, scrolling, `\n` |
| `low_level.c/.h` | Port I/O primitives — `port_byte_in` / `port_byte_out` (x86 `in`/`out` via inline asm)     |

The screen driver writes character cells straight to video memory at `0xb8000`
and steers the hardware cursor through the VGA CRTC index/data ports
(`0x3d4` / `0x3d5`). `low_level.c` uses x86-only register constraints, so it must
be built with the `i686-elf` cross-compiler, not host Clang.

### CPU / interrupts — `cpu/`

| File           | Role                                                                                                                 |
| -------------- | -------------------------------------------------------------------------------------------------------------------- |
| `idt.c/.h`     | The 256-entry IDT — `set_idt_gate()` fills a gate, `set_idt()` runs `lidt`                                           |
| `isr.c/.h`     | `isr_install()` wires vectors 0–31; `isr_handler()` is the C dispatcher; `registers_t` mirrors the saved stack frame |
| `interupt.asm` | 32 per-vector stubs (`isr0`–`isr31`) + `isr_common_stub` — push a uniform frame, `call isr_handler`, `iret`          |
| `types.h`      | Fixed-width typedefs (`u8`/`u16`/`u32`…) and the `low_16`/`high_16` address-split macros                             |

Each interrupt has its own stub because the CPU passes no vector number to the
handler — the stub hardcodes it (`push N`). Exceptions 8 and 10–14 push a real
error code; the rest push a dummy `0` so the stack frame (and thus `registers_t`)
is identical for all 32. `interupt.asm` is assembled as an **ELF object** (not
`%include`d like the boot sector) so the linker can resolve `call isr_handler`
and the `isr0`–`isr31` symbols.

### Kernel helpers — `kernel/`

| File        | Role                                                                 |
| ----------- | -------------------------------------------------------------------- |
| `util.c/.h` | Freestanding libc-lite — `memory_copy`, `memory_set`, `int_to_ascii` |

## Toolchain (macOS)

```bash
brew install i686-elf-gcc i686-elf-binutils nasm qemu
```

- `i686-elf-gcc` / `i686-elf-binutils` — bare-metal 32-bit ELF cross toolchain
  (natively 32-bit, so no `-m32` / `-m elf_i386` needed; _not_ host Clang)
- `nasm` — assembler
- `qemu` — emulator (`qemu-system-i386`)

## Build & run

Everything is driven by the `Makefile`.

```bash
make            # build the bootable disk image (os-image.bin)
make run        # build the image and boot it in QEMU
make run-boot   # boot only the boot sector (no kernel)
make usb        # flash the image to a USB stick (boot real hardware)
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

The boot sector reads the 15 sectors _after_ itself, so the image is zero-padded
to 16 sectors. An image that is too small makes the BIOS disk read fail and trips
`disk_error`.

## Booting on real hardware

`make usb` writes `os-image.bin` to a USB stick so you can boot a physical PC,
not just QEMU:

```bash
make usb                  # auto-detects the plugged-in external disk
make usb DISK=/dev/disk4   # or name the disk explicitly
```

It auto-detects the first external physical disk, shows a `y/N` prompt, then
unmounts → `dd`s the raw image → ejects (`sudo` prompts for your password).

> ⚠️ This **erases** the target disk. With more than one USB plugged in, pass
> `DISK=` explicitly. It must be the **whole disk** (`/dev/diskN`), never a
> partition (`/dev/diskNsM`) — otherwise sector 0 won't be the boot sector.

On the target machine's firmware:

- Enable **Legacy Boot / CSM** (the boot sector is legacy-BIOS MBR; UEFI-only
  machines with no CSM can't run it).
- Disable **Secure Boot**.
- Put the USB first in the boot order, or use the one-time boot menu.

Writing the raw image to the whole disk overwrites sector 0, so the stick boots
"superfloppy" style (no partition table) and the kernel lives in the first track.
The one prerequisite that makes this work on real hardware is that the boot
sector **zeroes `DS`/`ES`/`SS` at entry** — a real BIOS, unlike QEMU, doesn't
guarantee those are 0, and the disk read (`int 0x13`) loads into `ES:BX`. Without
it the read lands at a bogus address and trips `disk_error`.

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

| Piece        | Choice                                                   |
| ------------ | -------------------------------------------------------- |
| Architecture | x86, 32-bit (i686)                                       |
| Boot         | Legacy BIOS                                              |
| Assembler    | NASM                                                     |
| Compiler     | `i686-elf-gcc`                                           |
| Runs on      | QEMU + real x86 PCs (Legacy BIOS / CSM, booted from USB) |
| Host         | macOS (Apple Silicon)                                    |
