# YozOs

A hobby x86 (32-bit i686) operating system written from scratch: no GRUB, no
Multiboot. Hand-written assembly brings the CPU from 16-bit real mode into a
32-bit C kernel. Built on macOS (Apple Silicon) with an `i686-elf`
cross-compiler; runs in QEMU and on real Legacy-BIOS hardware.

## Status

Done so far:

- **Boot → C kernel** — boot sector loads the kernel at `0x1000`, switches to
  32-bit protected mode (GDT + `cr0.PE`), and calls `main()`.
- **Interrupts** — IDT with the 32 CPU exceptions; PIC remapped, IRQ stubs and
  dispatcher (EOIs + registered handlers).
- **Keyboard + shell** — IRQ1 driver decodes scancodes into a line buffer; a
  `yozOS >` prompt dispatches commands: `END`, `MMAP`, `FALLOC`, `PTEST`,
  `TASKTEST`, `IDENT`, `RSECT`, `WSECT`, `FSINFO`, `LS`, `CAT <file>`.
- **Timer** — PIT channel 0 (square-wave) counting ticks via IRQ0.
- **Paging** — E820 memory detection → bitmap frame allocator → identity-map of
  the first 4 MiB → `CR3` + `CR0.PG`. A page-fault handler (ISR 14) reports the
  `CR2` address; `PTEST` proves the MMU is live.
- **Multitasking** — `task_t` contexts with an asm `switch_context`; preemptive
  switching driven off the timer IRQ (`TASKTEST`).
- **Disk + filesystem** — ATA PIO (LBA28) driver: `IDENTIFY`, sector read/write
  (`IDENT`/`RSECT`/`WSECT`). On top of it, a read-only FAT16 driver parses the
  BPB (`FSINFO`), lists the root directory (`LS`) and reads files by name
  (`CAT <file>`) off a disk formatted and populated from the host.

Next: FAT16 **write** (create/delete/append), then userspace (Stage 6) —
ring 3 + system calls.

## Layout

| Path       | Role                                                                                                               |
| ---------- | ------------------------------------------------------------------------------------------------------------------ |
| `boot/`    | 512-byte boot sector + real-mode asm (GDT, E820 detection); `boot_sect.asm` `%include`s the rest                   |
| `kernel.c` | 32-bit C kernel `main()` (linked at `0x1000`) + shell dispatcher (`user_input`)                                    |
| `cpu/`     | IDT, ISR/IRQ stubs (`interupt.asm`), dispatcher (`isr.c`, ISR 14 page fault), `timer.c`                            |
| `drivers/` | VGA text screen, port I/O primitives, `keyboard.c` (IRQ1 handler), `ata.c` (ATA PIO disk driver)                    |
| `kernel/`  | Freestanding helpers: `string.c` (`strlen`/`strcmp`/`append`/`int_to_ascii`), `mem.c` (`memory_copy`/`memory_set`) |
| `memory/`  | E820 reader (`memory_map.c`), bitmap frame allocator (`frame_alloc.c`), paging (`paging.c` + `paging_asm.asm`)     |
| `task/`    | Preemptive multitasking: `task.c` (`task_t`, scheduler) + `switch_context.asm`                                     |
| `fs/`      | Read-only FAT16: `fat.c` (BPB parse, root-dir `ls`, FAT-chain `cat`)                                               |
| `basic.c`  | Standalone C scratch for studying disassembly (not booted)                                                         |

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
make run-disk   # boot in QEMU with a FAT16 data disk (disk.img) attached as IDE
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
