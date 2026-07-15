# YozOs

A hobby x86 (32-bit i686) operating system written from scratch: no GRUB, no
Multiboot. Hand-written assembly brings the CPU from 16-bit real mode into a
32-bit C kernel. Built on macOS (Apple Silicon) with an `i686-elf`
cross-compiler; runs in QEMU and on real Legacy-BIOS hardware.

## Status

YozOs currently boots a custom BIOS loader into a 32-bit C kernel with IDT/PIC
interrupts, PIT timer ticks, keyboard input, paging, preemptive tasks,
per-process address spaces, and ring-3 user programs.

The kernel can load headered `YOZ1` flat binaries and static ELF32 executables
from a FAT16 data disk, then run them as scheduler-owned user processes. `RUN`
auto-detects both formats by file magic. Userland has a small process/syscall
surface (`getpid`, `getppid`, `waitpid`, `kill`, `yield`, `exit`) plus
fd-oriented I/O: `open`, `read`, `write`, `close`, `stat`, and `lseek`. Syscall
failures use a stable errno-style negative ABI shared through `cpu/user_abi.h`.

Storage is intentionally simple: ATA PIO + FAT16 support, wrapped by a small VFS
layer. FAT is limited to 8.3 names, nested subdirectory paths, and files up to
8 KiB, but supports shell and userland read/write/append/seek tests.

Current external user tests cover basic output/input, process syscalls, user
faults, fd stdin/stdout, file read/write/append, stat, seek, overwrite, path
resolution, directory-aware stat types, subdirectory file I/O, and static ELF
execution:
`HELLO.BIN`, `ECHO.BIN`, `ECHOFD.BIN`, `PID.BIN`, `PPID.BIN`, `WAITSELF.BIN`,
`KILLSELF.BIN`, `FAULT.BIN`, `READFILE.BIN`, `STAT.BIN`, `WRITEF.BIN`,
`APPEND.BIN`, `SEEK.BIN`, `OVERWR.BIN`, `PATH.BIN`, `DIRSTAT.BIN`,
`DIRTEST.BIN`, `NESTDIR.BIN`, `HELLO.ELF`, and `BSS.ELF`.

Current limits:

- Static ELF support is early: program headers, loadable segments, BSS zeroing,
  entry validation, and basic permissions work, but there is no `argc`/`argv`
  stack ABI, C runtime, libc, dynamic linking, or `exec` yet.
- The filesystem has no long names, directory fd API, or large-file support.
- FAT writes are still capped at small fixed-size files.

## Layout

| Path       | Role                                                                                                               |
| ---------- | ------------------------------------------------------------------------------------------------------------------ |
| `boot/`    | 512-byte boot sector + real-mode asm (GDT, E820 detection); `boot_sect.asm` `%include`s the rest                   |
| `kernel.c` | 32-bit C kernel `main()` (linked at `0x10000`) and subsystem initialization                                        |
| `cpu/`     | GDT/TSS/userspace entry, IDT, ISR/IRQ stubs (`interupt.asm`), syscall stub/dispatcher, user syscall wrappers/test, `timer.c` |
| `drivers/` | VGA text screen, port I/O primitives, `keyboard.c` (IRQ1 handler), `ata.c` (ATA PIO disk driver)                   |
| `kernel/`  | Freestanding helpers: `string.c` (`strlen`/`strcmp`/`append`/`int_to_ascii`), `mem.c` (`memory_copy`/`memory_set`) |
| `memory/`  | E820 reader (`memory_map.c`), bitmap frame allocator (`frame_alloc.c`), paging (`paging.c` + `paging_asm.asm`)     |
| `task/`    | Preemptive multitasking: `task.c` (`task_t`, scheduler) + `switch_context.asm`                                     |
| `fs/`      | FAT16 + VFS: root-dir list/read/create/write/append/delete/rename, stat, seek, fd-backed reads/writes; 8.3 names, 8 KiB files |
| `elf/`     | ELF32 header/program-header validation helpers for static user executables                                         |
| `shell/`   | Table-driven shell dispatcher and command handlers                                                                 |
| `user/`    | External `YOZ1` and ELF user test sources for output, input, process, fault, fd/VFS syscall, and ELF loader tests |
| `tools/`   | Host-side helper scripts, including a small FAT16 image installer for test binaries                                |
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
make user-programs  # build external YOZ1 and ELF user binaries
make install-user-programs  # copy user binaries into disk.img
make prepare-user-disk  # build image + install all user binaries
make run        # build + boot in QEMU
make run-disk   # boot in QEMU with a FAT16 data disk (disk.img) attached as IDE
make usb        # flash image to a USB stick (see below)
make clean      # remove build artifacts
```

`os-image.bin` is a 1.44 MB raw floppy image: sector 0 is the boot sector,
sectors 1+ contain the kernel, and the rest is zero-padded. `make` computes the
linked kernel sector count, writes it to `boot/kernel_sectors.inc`, and fails if
the image would exceed the boot loader's one-byte sector count.

To test the external userspace path:

```bash
make prepare-user-disk
make run-disk
```

`make prepare-user-disk` recreates `disk.img` and installs the external test
binaries under the FAT `TEST/` directory. Then run this in the YozOs shell:

```text
RUN TEST/HELLO.BIN
RUN TEST/ECHO.BIN
RUN TEST/FAULT.BIN
RUN TEST/READFILE.BIN
RUN TEST/ECHOFD.BIN
RUN TEST/STAT.BIN
RUN TEST/WRITEF.BIN
RUN TEST/APPEND.BIN
RUN TEST/SEEK.BIN
RUN TEST/OVERWR.BIN
RUN TEST/PATH.BIN
RUN TEST/DIRSTAT.BIN
MKDIR TESTDIR
RUN TEST/DIRTEST.BIN
MKDIR A
MKDIR A/B
RUN TEST/NESTDIR.BIN
RUN TEST/HELLO.ELF
RUN TEST/BSS.ELF
```

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
