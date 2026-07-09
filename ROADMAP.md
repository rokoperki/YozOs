# YozOs Roadmap

Long-term goal: grow YozOs from a hobby 32-bit kernel into a usable operating
system that can run real user programs, then eventually support ELF binaries,
networking, a GUI, packages, and browser-class software. A modern browser is an
endgame target; it depends on a large stack of process, memory, filesystem,
network, graphics, threading, dynamic-linking, and POSIX-like compatibility work.

## Current Position

YozOs currently has a working early 32-bit OS foundation:

- [x] **Boot -> 32-bit C kernel** — custom BIOS boot sector, protected mode, GDT,
      kernel loaded at `0x10000`, `.bss` zeroing, dynamic kernel sector count.
- [x] **Interrupts and IRQs** — IDT, CPU exception stubs, PIC remap, IRQ
      dispatch, timer and keyboard IRQs.
- [x] **Keyboard shell** — table-driven `yozOS >` shell with memory, task,
      filesystem, disk, and userspace commands.
- [x] **Timer** — PIT channel 0 through IRQ0.
- [x] **Memory management v1** — E820 memory map, bitmap frame allocator,
      identity-mapped paging, page-fault reporting.
- [x] **Multitasking v1** — `task_t`, context switching, scheduler, task states,
      `spawn_task()`, `TASKS`, `REAP`.
- [x] **FAT16 v1** — ATA PIO disk access, root-directory FAT16 reads/writes,
      `FSINFO`, `LS`, `CAT`, `CREATE`, `WRITE`, `WRITEPAT`, `APPEND`, `DELETE`,
      `RENAME`.
- [x] **Ring-3 userspace v1** — user GDT/TSS setup, `iret` entry, syscall stub,
      `SYS_WRITE_CHAR`, `SYS_STRING_WRITE`, `SYS_WRITE_BUFFER`, `SYS_READ_LINE`,
      `SYS_EXIT`, `SYS_YIELD`, syscall pointer validation, user fault handling.
- [x] **Flat FAT-loaded userspace v1** — `RUN <file>` loads headered `YOZ1`
      flat binaries from `disk.img`, validates magic/load address/entry/size,
      enters ring 3, tracks processes with PIDs, links loaded processes to their
      owning task, and supports external test binaries:
      `HELLO.BIN`, `ECHO.BIN`, `FAULT.BIN`.
- [x] **External user build workflow** — `make prepare-user-disk` builds the OS
      image and installs all external user binaries into `disk.img`.

## Phase 1: Stabilize Current Flat Userspace

Goal: make the current FAT-loaded, fixed-address userspace path reliable before
opening larger architecture work.

- [x] Headered `YOZ1` binaries.
- [x] Async `RUN <file>` through scheduler tasks.
- [x] Stable process names in `PROCS`.
- [x] PIDs in `PROCS`.
- [x] Loaded `RUN` processes link to owning tasks.
- [x] External hello/input/fault test binaries.
- [x] Reproducible user-disk prep target.
- [x] Finish single-slot loader cleanup: keep the fixed `USER_LOAD_ADDR` model
      explicit, clear the load region before each load, and keep header checks
      strict.
- [x] Keep `RUN`, `USERTEST`, `USERFAULT`, `TASKTEST`, `PROCS`, `TASKS`, and
      `REAP` behavior consistent enough for repeated testing.

## Phase 2: Real Process Model

Goal: make processes a first-class kernel concept instead of test scaffolding.

- Process table cleanup and clearer ownership rules.
- PID lifecycle and process states.
- Link every user process to its owning task, including built-in tests.
- `wait`/reap by PID.
- `kill` by PID.
- Better process exit/fault reporting.
- Process-related syscalls (`getpid`, later `exit`, `wait`, `spawn`/`exec`).
- Decide whether tasks and processes stay separate concepts or merge more tightly.

## Phase 3: Address Spaces

Goal: isolate user programs so multiple processes can safely use the same virtual
layout.

- One page directory per process.
- Shared kernel mapping in every address space.
- User pages mapped per process.
- Per-process user stacks.
- Switch address spaces during task switches.
- Syscall pointer validation against the current process address space.
- Use the same virtual load address for different processes safely.

## Phase 4: VFS And File Descriptors

Goal: stop binding userland directly to root-only FAT helpers and move toward a
Unix-like file model.

- VFS layer over filesystem backends.
- Path parsing and current working directory.
- Directories.
- File handles/file descriptors.
- `open`, `read`, `write`, `close`, `stat` syscalls.
- Keep FAT as a simple backend for now.
- Expand FAT support after VFS exists: subdirectories, larger files, better FAT
  chain allocation, and fuller metadata handling.
- Add ext2 later as the first more serious filesystem.

## Phase 5: Static ELF Userland

Goal: run normal statically linked programs, not only custom flat binaries.

- ELF program-header loader.
- Loadable segments with permissions.
- Zeroed BSS.
- User stack ABI with `argc`, `argv`, `envp`.
- Minimal C runtime entry.
- Small libc or tiny libc port.
- Basic commands as user programs.

## Phase 6: POSIX-ish Syscall Surface

Goal: support small Unix-like CLI software.

- File descriptor syscalls.
- `mmap`/`munmap` or a simpler first mapping API.
- `brk`/heap growth.
- `spawn`/`exec` first; `fork` later if still desired.
- `waitpid`, `getpid`, time syscalls.
- Pipes and basic polling later.
- Error codes and ABI conventions close enough for libc.

## Phase 7: Networking

Goal: provide sockets and basic internet connectivity.

- Pick a QEMU-first NIC target, likely e1000 or virtio-net.
- Ethernet frame send/receive.
- ARP.
- IPv4.
- ICMP ping.
- UDP.
- TCP.
- DNS resolver.
- Socket syscalls.

## Phase 8: GUI

Goal: build a usable graphical environment.

- Framebuffer driver.
- Keyboard and mouse event path.
- Font rendering.
- 2D drawing primitives.
- Window/compositor model.
- GUI server/client protocol.
- Simple GUI apps before a full desktop.

## Phase 9: Dynamic Linking And Packages

Goal: support a larger userland ecosystem.

- Dynamic ELF loader.
- Shared libraries.
- Package archive format.
- Package metadata and dependency handling.
- Installer/updater command.
- Port small packages before large ones.

## Phase 10: Browser-Class Software

Goal: eventually run browser-like software. This depends on most earlier phases.

- Robust virtual memory and `mmap`.
- Threads and synchronization primitives.
- Dynamic linking and shared libraries.
- Networking with TCP/TLS-capable libraries.
- Fonts and graphics stack.
- Timers/events/polling.
- Much broader POSIX/Linux compatibility.
- Start with a tiny browser or simple networked GUI app before modern browsers.

## Immediate Next Order

1. Finish **Phase 1** loader/process cleanup.
2. Start **Phase 2** real process model.
3. Build **Phase 3** per-process address spaces.
4. Add **Phase 4** VFS/file descriptors.
5. Move from `YOZ1` flat binaries to **Phase 5** static ELF userland.

## Deliberately Deferred

- ELF until process and address-space foundations are ready.
- Dynamic linking until static ELF and a syscall ABI are stable.
- Browser-class software until networking, GUI, dynamic linking, threads, and a
  much larger compatibility layer exist.
- Subdirectories and long filenames until VFS/path work begins.
- Full package management until a real userland and filesystem model exist.

## Reading References

- **Interrupts** — OSDev "Interrupt Descriptor Table", "8259 PIC",
  "Interrupts"; cfenollosa.
- **Keyboard** — OSDev "PS/2 Keyboard"; cfenollosa keyboard lesson.
- **Timer** — OSDev "Programmable Interval Timer".
- **Memory** — Little OS Book paging chapter; OSDev "Setting Up Paging" and
  "Page Frame Allocation"; OSTEP address-translation/paging chapters.
- **Multitasking** — Little OS Book scheduling chapter; OSDev "Multitasking
  Systems"; Brendan's multitasking tutorial; OSTEP scheduling/concurrency.
- **Filesystem** — Little OS Book filesystem chapter; OSDev "File Systems",
  "FAT", "ATA PIO Mode"; ext2 references before VFS/ext2 work.
- **Userspace** — Little OS Book user mode and system-call chapters; OSDev
  "Getting to Ring 3", "System Calls".
- **ELF/POSIX** — ELF specification, System V i386 ABI, musl/libc porting notes.
- **Networking** — OSDev networking pages, e1000/virtio-net documentation,
  TCP/IP Illustrated or a small TCP/IP stack walkthrough.
- **GUI** — OSDev framebuffer and input references; simple compositor/windowing
  articles before toolkit work.
