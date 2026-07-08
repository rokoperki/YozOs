# YozOs

A hobby x86 (32-bit i686) operating system written from scratch: no GRUB, no
Multiboot. Hand-written assembly brings the CPU from 16-bit real mode into a
32-bit C kernel. Built on macOS (Apple Silicon) with an `i686-elf`
cross-compiler; runs in QEMU and on real Legacy-BIOS hardware.

## Status

Done so far:

- **Boot → C kernel** — boot sector loads the kernel at `0x10000`, switches to
  32-bit protected mode (GDT + `cr0.PE`), and calls `main()`.
- **Interrupts** — IDT with the 32 CPU exceptions; PIC remapped, IRQ stubs and
  dispatcher (EOIs + registered handlers).
- **Keyboard + shell** — IRQ1 driver decodes scancodes into a line buffer and
  publishes completed lines; the kernel main loop dispatches shell commands
  outside interrupt context. The table-driven `yozOS >` shell supports `HELP`,
  `END`, `MMAP`, `FALLOC`, `PTEST`, `TASKTEST`, `TASKS`, `REAP`, `PROCS`,
  `USERTEST`, `USERFAULT`, `RUN <file>`, `IDENT`, `RSECT`, `WSECT`, `FSINFO`, `LS`,
  `CAT <file>`, `CREATE <file>`, `WRITE <file> <text>`, `WRITEPAT <file>
  <size>`, `DELETE <file>`, `APPEND <file> <text>`, `RENAME <file> <text>`.
- **Timer** — PIT channel 0 (square-wave) counting ticks via IRQ0.
- **Paging** — E820 memory detection → bitmap frame allocator → identity-map of
  the first 4 MiB → `CR3` + `CR0.PG`. A page-fault handler (ISR 14) reports the
  `CR2` address; `PTEST` proves the MMU is live.
- **Multitasking** — `task_t` contexts with an asm `switch_context`; preemptive
  switching driven off the timer IRQ. The scheduler now has a boot-time
  main/idle task model, task states, a fixed task table, `spawn_task()`, exited
  task reaping, and debug shell commands (`TASKS`, `REAP`). `TASKTEST` is a
  finite rerunnable scheduler smoke test.
- **Disk + filesystem** — ATA PIO (LBA28) driver: `IDENTIFY`, sector read/write
  (`IDENT`/`RSECT`/`WSECT`). On top of it, a FAT16 driver parses the BPB
  (`FSINFO`), lists the root directory (`LS`), reads files by name
  (`CAT <file>`), and supports simple root-directory writes (`CREATE`, `WRITE`,
  `DELETE`, `APPEND`, `RENAME`) on a FAT-formatted data disk.
  FAT support is intentionally scoped to the root directory, 8.3 filenames, and
  files up to 8 KiB.
- **Userspace bring-up** — the kernel installs a C-managed final GDT with ring-3
  code/data descriptors, loads a TSS (`ltr`) for ring-3 to ring-0 stack
  switching, enters linked-in user programs with `iret`, and handles `int 0x80`
  syscalls: `SYS_WRITE_CHAR`, `SYS_STRING_WRITE`, `SYS_WRITE_BUFFER`,
  `SYS_READ_LINE`, `SYS_EXIT`, and `SYS_YIELD`. The syscall stub saves/restores
  user register and segment state, switches to kernel data segments for the C
  handler, and returns values through the interrupted user's `eax`. Kernel pages
  are supervisor-only by default; linked-in user code/data/stack regions are
  explicitly marked `PAGE_USER` and carry read/write/execute permissions.
  Syscall pointer arguments are validated against both page-table user bits and
  declared region permissions. Keyboard input has shell/user ownership so
  `SYS_READ_LINE` can read a line while the shell is not consuming it. `USERTEST`
  and `USERFAULT` now run as scheduler-owned kernel tasks that enter ring 3,
  report async exit/fault status, and are tracked in a small user-process table
  visible through `PROCS`. Built-in user tests are described through
  loader-shaped `user_program_t` descriptors. `RUN <file>` loads a headered
  `YOZ1` flat binary from the FAT16 data disk into a fixed user address
  (`0x70000`), validates its magic/load address/entry/size, builds a
  `user_program_t`, and enters it in ring 3. External test programs now cover
  output (`HELLO.BIN`), input (`ECHO.BIN`), and user fault handling (`FAULT.BIN`).

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
| `fs/`      | FAT16: `fat.c` (BPB parse, root-dir list/read/create/write/append/delete/rename; 8.3 names, 8 KiB files)           |
| `shell/`   | Table-driven shell dispatcher and command handlers                                                                 |
| `user/`    | External `YOZ1` flat user binaries: `HELLO.BIN`, `ECHO.BIN`, and `FAULT.BIN`                                      |
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
make user-programs  # build external flat user binaries
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

Then run this in the YozOs shell:

```text
RUN HELLO.BIN
RUN ECHO.BIN
RUN FAULT.BIN
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
