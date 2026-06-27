# YozOs

A hobby x86 (32-bit i686) operating system written from scratch: no GRUB, no
Multiboot. Hand-written assembly brings the CPU from 16-bit real mode into a
32-bit C kernel. Built on macOS (Apple Silicon) with an `i686-elf`
cross-compiler; runs in QEMU and on real Legacy-BIOS hardware.

## Status

Boot pipeline is complete: boot sector → GDT → protected mode → C kernel. The
kernel drives the screen through a C driver and installs an **IDT** handling the
32 CPU exceptions. Currently wiring up **hardware interrupts** (PIC remap, IRQs)
and a first keyboard handler. Next stop is a keyboard + shell.

## Boot flow

```
BIOS ─► boot_sect.asm @ 0x7c00   [16-bit real mode]
        set up stack, print banner, load kernel @ 0x1000 (int 0x13)
        lgdt, set cr0.PE, far-jump ─► protected mode
        reload segments, call kernel @ 0x1000
   ─► main() in kernel.c          [32-bit protected mode]
        clear_screen / print via screen driver
        isr_install() ─► build + lidt the IDT
```

## Layout

| Path         | Role                                                              |
| ------------ | ----------------------------------------------------------------- |
| `boot/`      | 512-byte boot sector; `boot_sect.asm` `%include`s the other `.asm` |
| `kernel.c`   | 32-bit C kernel `main()`, linked at `0x1000`                       |
| `cpu/`       | IDT, ISR stubs (`interupt.asm`), C interrupt dispatcher (`isr.c`)  |
| `drivers/`   | VGA text-mode screen driver + port I/O primitives                 |
| `kernel/`    | Freestanding helpers (`memory_copy`, `int_to_ascii`, …)           |
| `basic.c`    | Standalone C scratch for studying disassembly (not booted)        |

## Toolchain

```bash
brew install i686-elf-gcc i686-elf-binutils nasm qemu
```

The `i686-elf` cross toolchain is required: `low_level.c` uses x86-only
register constraints that fail under host Clang.

## Build & run

```bash
make            # build bootable image (os-image.bin)
make run        # build + boot in QEMU
make usb        # flash image to a USB stick (see below)
make clean      # remove build artifacts
```

`os-image.bin` is a 16-sector raw image: sector 0 is the boot sector, sectors
1+ are the kernel (zero-padded; a too-small image trips `disk_error`).

## Booting on real hardware

```bash
make usb                   # auto-detects the plugged-in external disk
make usb DISK=/dev/disk4    # or name it explicitly
```

⚠️ This **erases** the target disk (a `y/N` prompt guards it). Use the whole
disk (`/dev/diskN`), never a partition. On the target machine: enable Legacy
Boot / CSM, disable Secure Boot, boot from the USB.

## Debugging

Triple-faults during the PM switch are silent. Log interrupts and CPU state:

```bash
qemu-system-i386 -drive format=raw,file=os-image.bin -d int,cpu
```

## Target stack

x86 32-bit (i686) · Legacy BIOS · NASM · `i686-elf-gcc` · QEMU + real hardware ·
built on macOS (Apple Silicon)
