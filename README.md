# ZX86v2

An experimental 32-bit x86 protected-mode operating system kernel, built from scratch as a long-running spare-time project (~10 years and counting). It boots via Multiboot, sets up its own GDT/IDT/paging, and provides a monolithic kernel with a built-in shell, filesystem stack, VESA graphics, networking, sound, and a simple asymmetric multiprocessing (AMP) system for offloading work to secondary CPUs.

This is a research/hobby kernel, not a production OS — expect rough edges, debug logging left in place, and features in various states of completeness.

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
  - [Boot Process](#boot-process)
  - [Memory Layout](#memory-layout)
  - [Asymmetric Multiprocessing (AMP)](#asymmetric-multiprocessing-amp)
  - [Process Model](#process-model)
  - [Filesystem Stack](#filesystem-stack)
  - [Graphics](#graphics)
  - [Networking](#networking)
  - [Sound](#sound)
- [Repository Layout](#repository-layout)
- [Building](#building)
- [Running](#running)
- [System Call API](#system-call-api)
- [Shell](#shell)
- [Bundled Languages & Tools](#bundled-languages--tools)
- [Known Limitations](#known-limitations)

## Overview

ZX86v2 is a single kernel binary (`kernel.bin`) that:

- Boots as a Multiboot-compliant kernel (GRUB/QEMU `-kernel`) at `0x100000`.
- Runs entirely in ring 0 for kernel code, and switches to ring 3 for the shell and user programs via `switch_to_user_mode()`.
- Implements its own GDT, IDT, ISR/IRQ dispatch, paging, kernel heap, and cooperative-ish task structures.
- Provides a BSD-inspired VFS with a FAT32 backend (via the [FatFs](http://elm-chan.org/fw/ff/00index_e.html) library) and a `/dev` device filesystem.
- Drives VESA/VGA graphics with a software framebuffer, font renderer, and terminal emulation layer.
- Includes a minimal TCP/IP stack (ARP/IP/ICMP/TCP) over an RTL8139 NIC driver.
- Supports Sound Blaster 16 audio.
- Can load and execute 32-bit ELF binaries, with isolated page directories per process.
- Ships a built-in shell with a FASM assembler, disassembler, `vi`-style editor, and Scheme/BASIC/toy-database interpreters — all runnable without leaving the kernel image.

## Architecture

### Boot Process

1. `src/kernel/boot.asm` — Multiboot entry point (`start`), sets up an initial stack, jumps into `kmain()`.
2. `kmain()` in [src/kernel/main.c](src/kernel/main.c) initializes, in order:
   - GDT/IDT ("descriptor tables"), syscalls, PIT timer (100Hz), paging, tasking, keyboard (IRQ1), ATA/IDE, PCI.
   - Network stack (RTL8139 + ARP/IP/ICMP/TCP) if a NIC is detected.
   - AMP subsystem (`amp_init`) — detects APIC, allocates the shared AMP workspace, registers the IPI handler.
   - Graphics (`graphics_init(800, 600)`), then AMP shared memory, then boots secondary CPUs (`amp_start_secondary_cpus`).
   - Sound Blaster 16 detection/init.
   - VFS init and disk mount (`os_mount`).
   - `switch_to_user_mode()` — drops CPU0 to ring 3 and enters the shell loop (`shell_main()`), which never returns under normal operation.

### Memory Layout

Key virtual address regions (see [link.ld](link.ld) and `include/amp.h`):

| Region | Address | Notes |
|---|---|---|
| Kernel image | `0x100000` | Multiboot header, `.text`/`.data`/`.bss`, page-aligned |
| Kernel stack (relocated) | `0xE0000000` | Set up in `move_stack()` during `initialise_tasking()` |
| AMP secondary CPU stacks | `0xE0000000 +` | 64KB per CPU, allocated just below the shared memory region |
| AMP shared memory | `0xF0000000` | 8MB region (`SHARED_MEMORY_SIZE`), mapped identically into every CPU's page directory for lock-free-ish cross-CPU communication |
| User stack (ELF processes) | near `0xBFFFFFFC` | Top-down user stack for switched processes |

The kernel heap is a custom `kmalloc`/`kfree` allocator (`src/kernel/kheap.c`) with an ordered-array free-list; a separate libc-style heap exists for C-runtime allocations (`malloc`/`free` in `src/libc/malloc.c`).

### Asymmetric Multiprocessing (AMP)

Unlike a typical SMP scheduler, ZX86v2 does not load-balance tasks automatically. Instead, **CPU0 is the master** — it owns the kernel, the shell, all syscalls, and graphics — while **CPU1 through CPU15** (`MAX_CPUS`, capped at `MAX_BOOT_CPUS = 8` by default) are booted via the APIC INIT/STARTUP IPI sequence and sit in a power-efficient idle loop (`sti; hlt`) waiting for work.

- Startup: `amp_start_secondary_cpus()` copies a small real-mode-to-protected-mode trampoline (`src/kernel/ap_startup.asm`) to `0x8000`, then sends INIT + STARTUP IPIs per AP via the local APIC (`apic_send_init_ipi`/`apic_send_startup_ipi`).
- Communication: a single `amp_workspace_t` structure (allocated once, mapped into every page directory at `SHARED_MEMORY_BASE`) holds a fixed-size queue of `cpu_function_call_t` slots (`MAX_CONCURRENT_FUNCTIONS = 16`), per-slot argument/result buffers (64KB each), and per-CPU status/heartbeat info.
- Dispatch: `amp_execute_function()` / `amp_execute_function_on_cpu()` copy arguments into a shared slot, mark it `FUNCTION_PENDING`, and send an `IPI_FUNCTION_READY` IPI to the target CPU. The target CPU's main loop (`secondary_cpu_main()` in `src/kernel/amp_cpu.c`) picks up the call, executes the function pointer directly (secondary CPUs run in the kernel's address space), writes the result back, and marks the slot `FUNCTION_COMPLETE`. The caller busy-polls (with `pause`) until completion or a 5-second timeout.
- Syscall forwarding: if code running on a secondary CPU needs a kernel service that must run on CPU0 (e.g. anything touching the framebuffer), it queues a request in `amp_syscall_state_t` and sends an IPI to CPU0, which services it from its own timer/IPI handler via `amp_handle_syscall_requests()`.
- Distributed graphics: bitmap operations can be dispatched to the top two boot CPUs (`BITMAP_CPU_COUNT = 2`, e.g. CPU6/7 on an 8-CPU boot) for parallel pixel processing.

**Critical constraint:** only CPU0 performs `kmalloc`/`kfree` and page-directory switches. Secondary CPUs operate against pre-mapped kernel memory and must avoid those operations to prevent races on the shared page tables.

### Process Model

- `os_create_elf_process()` parses and loads a 32-bit ELF image (`src/drivers/elf-load.c`), allocating an isolated `page_directory_t` and a `process_memory_layout_t` describing the code/data/stack regions for the new process.
- `os_exec_process(pid)` switches into the process's page directory and transfers control to its entry point in ring 3 via `switch_to_user_mode_with_target()` (an `iret`-based privilege transition).
- Process execution can be requested on a specific CPU through the AMP function-offload path (`amp_execute_function_on_cpu`), letting an ELF binary run its startup work on a secondary CPU while syscalls are transparently forwarded back to CPU0.
- A simple process table (`process_table[MAX_PROCESSES]`, 32 slots) tracks created processes outside of the cooperative task/ready-queue structures used for the kernel's own task list.

### Filesystem Stack

Layered, BSD-`vnode`-flavoured design:

```
Shell / syscalls
      │
      ▼
  VFS (src/fs/vfs.c)  ── mount table, path resolution, open file descriptors
      │
      ├── FAT32 adapter (src/fs/fat_vfs.c) ── wraps FatFs (src/drivers/ff.c)
      ├── DevFS (src/fs/devfs.c)           ── /dev entries for block/char devices
      │
      ▼
  Block device layer (src/drivers/blockdev.c, dfs.c)
      │
      ▼
  ATA/IDE + disk I/O (src/drivers/disc.c, diskio.c)
```

Typical bring-up sequence: `blockdev_register()` → `vfs_mount()` → shell/file access. A RAM-disk driver (`src/drivers/ramdisk.c` / `include/ramdisk.h`) and FAT32 formatter (`src/drivers/fat32_format.c`) are also included (the RAM-disk mount is currently disabled at boot pending a page-fault fix under AMP — see `kmain()`).

### Graphics

- `src/graphics/framebuffer.c` — VESA/linear-framebuffer management with double-buffering support.
- `src/graphics/font.c` — bitmap font rendering.
- `src/graphics/terminal.c` — text-console emulation layered on top of the framebuffer (scrollback, cursor, colour attributes), used by the shell's `printf`/`putchar` output.
- `src/graphics/bitmap.c` — bitmap blit/processing routines, some of which can be distributed to secondary CPUs via AMP.
- `src/drivers/qemu_vga.c` — VGA/QEMU-specific mode-setting glue.

All terminal/graphics operations are intentionally kept on CPU0 to avoid synchronizing scroll/cursor state across CPUs.

### Networking

A minimal hand-rolled TCP/IP stack:

- `src/drivers/rtl8139.c` — RTL8139 NIC driver (used for both TX/RX and interrupt handling).
- `src/network/ip.c` — IPv4 + ARP.
- `src/network/icmp.c` — ICMP (ping) support.
- `src/network/tcp.c` — basic TCP.

QEMU is configured (see `bootkernel.bat` / `boot_echo.bat`) with host port `2007` forwarded to guest port `7`, useful for exercising the echo-server test path.

### Sound

`src/drivers/sound.c` provides Sound Blaster 16-compatible playback, wired up to QEMU's SB16 audio backend.

## Repository Layout

```
src/
  kernel/     Core kernel: boot, GDT/IDT, paging, syscalls, tasking, AMP, kernel heap, timer
  drivers/    ATA/IDE, PCI, FAT32 (FatFs), block devices, keyboard, serial, RTC, RTL8139, sound, ELF loader
  fs/         VFS, FAT32-to-VFS adapter, DevFS
  graphics/   Framebuffer, fonts, terminal emulation, bitmap ops
  network/    IP/ARP, ICMP, TCP
  libc/       Freestanding C library subset (stdio, string, malloc, math, printf/scanf, sha-256)
  user/       Shell, vi-style editor, disassembler, FASM front-end, Scheme, BASIC, toy DB
include/      Public headers for all of the above, plus a small libc/libc++ shim
fasm/         Embedded FASM (flat assembler) source, used by the in-shell `fasm` command
imgtool/      Host-side tools for building/manipulating the FAT32 disk image
obj/          Build output (object files)
link.ld       Kernel linker script (Multiboot header @ 0x100000)
Makefile      Cross-compiler build rules
kerndev.bat / bootkernel.bat / boot_debug.bat / boot_echo.bat
              Windows dev-environment/toolchain and QEMU launch scripts
```

## Building

Requires an `i686-elf` GCC cross-compiler toolchain, NASM, and (for the in-kernel assembler) FASM sources are already vendored under `fasm/`.

```bash
make            # Build kernel.bin
make clean      # Remove build artifacts
```

The `Makefile` auto-detects Windows vs. other OSes for compiler paths; on Windows, run `kerndev.bat` first to put `i686-elf-gcc`, NASM, FASM, and QEMU on `PATH`.

## Running

```bash
./bootkernel        # Launch QEMU (SMP, RTL8139 NIC, SB16 audio) — see bootkernel.bat/.sh
```

Other launch variants:
- `boot_debug.bat` — serial output redirected to the console, for kernel-level debugging.
- `boot_echo.bat` — network echo-server testing (host `2007` → guest `7`).

## System Call API

User-mode code (the shell and anything it loads) talks to the kernel exclusively through `int 0x80`, dispatched in `src/kernel/syscall.c`. The syscall number goes in `eax`; arguments follow in `ebx`, `ecx`, `edx`, `esi`. Wrappers are declared in `include/syscall.h` (`syscall_<name>()`).

**Important:** the syscall gate is what allows privileged instructions (like `hlt`) to run safely on behalf of ring-3 callers — the handler itself always executes at CPL0 regardless of the caller's privilege level. Blocking-wait code that might be called from user/shell context should go through a syscall rather than executing `hlt`/`cli` directly.

| # | Syscall | Signature | Purpose |
|---|---|---|---|
| 0 | `os_puts` | `(const char*)` | Write a string to the console |
| 1 | `os_getc` | `(int fd)` | Blocking read of one keypress (halts CPU0 until a key IRQ arrives) |
| 2 | `os_putc` | `(int c)` | Write a single character |
| 3 | `os_asctime` | `()` | Get current time as a string |
| 4 | `os_ls` | `(int argc, char** argv)` | List directory contents |
| 5 | `os_rmdir` | `(int argc, char** argv)` | Remove a directory |
| 6 | `os_mkdir` | `(int argc, char** argv)` | Create a directory |
| 7 | `os_cd` | `(int argc, char** argv)` | Change directory |
| 8 | `os_getcwd` | `(char* buf, int size)` | Get current working directory |
| 9 | `os_chdrive` | `(int argc, char** argv)` | Change drive |
| 10 | `os_fasm` | `(int argc, char** argv)` | Invoke the embedded FASM assembler |
| 11 | `os_fork` | `()` | Fork a task |
| 12 | `os_create_elf_process` | `(void* elf_data, size_t size)` | Load an ELF image into a new isolated process |
| 13 | `os_exec_process` | `(int pid)` | Execute a previously created process |
| 14 | `os_exit_process` | `(int exit_code)` | Terminate the current process |
| 15 | `os_virtual_alloc` | `(uint32_t addr, size_t size)` | Map virtual memory pages |
| 16 | `os_virtual_free` | `(void* ptr)` | Unmap virtual memory |
| 17 | `os_amp_is_initialized` | `()` | Query whether AMP is up |
| 18 | `os_amp_get_cpu_count` | `()` | Number of online secondary CPUs |
| 19 | `os_amp_print_status` | `()` | Dump AMP status to the console |

Higher-level AMP function-offload API (kernel-side, used internally rather than via `int 0x80`):

- `bool amp_init(void)` / `bool amp_start_secondary_cpus(void)` / `void amp_shutdown(void)`
- `int amp_execute_function(void *func, void *args, size_t args_size, void *result, size_t result_size)` — synchronous, round-robin CPU selection.
- `int amp_execute_function_on_cpu(uint8_t cpu_id, ...)` — synchronous, targeted CPU (CPU0 runs the function locally rather than through the workspace).
- `int amp_execute_function_async(void *func, void *args, size_t args_size)` / `bool amp_is_task_complete(uint32_t task_id)` — fire-and-forget with polling.
- `bool amp_is_initialized(void)`, `uint8_t amp_get_cpu_count(void)`, `bool amp_is_cpu_online(uint8_t cpu_id)`, `void amp_print_status(void)`.

## Shell

The shell (`src/user/shell.c`) runs entirely in ring 3 and provides a conventional command-line experience. See [readme.md](readme.md) for the full command reference (`ls`, `cd`, `mkdir`, `rmdir`, `rm`, `mv`, `cat`, `bcat`, `peek`/`poke`/`dump`/`save`/`load` for raw memory access, `fasm`, `vi`, `disasm`, `elfsup`, plus `help`/`time`/`date`).

## Bundled Languages & Tools

All of the following run inside the kernel image, without needing a hosted OS:

- **FASM** — a nearly-complete port of the Flat Assembler (`fasm/`), invokable as the `fasm` shell command or via the `os_fasm` syscall, for assembling `.asm` files directly on-target.
- **Scheme** — an embedded Scheme interpreter (`src/user/scheme.c`, `scheme-api.c`), `scheme` shell command.
- **BASIC** — a small BASIC interpreter (`src/user/basic.c`), `basic` shell command.
- **Toy database** — simple record storage/query engine (`src/user/db.c`), `db` shell command.
- **`vi_simple`** — a minimal `vi`-style modal text editor.
- **`disasm`** — an x86 disassembler for inspecting machine code in memory.
- **ELF loader** (`elfsup` / `os_create_elf_process`) — load and run standalone 32-bit ELF executables.

## Known Limitations

- The scheduler is intentionally simple — AMP is *asymmetric* work offload, not a general SMP load balancer; secondary CPUs cannot allocate kernel memory or switch page directories themselves.
- The RAM-disk + FAT32 auto-mount at boot is disabled pending a page-fault issue interacting with the AMP shared-memory mappings (see the commented-out block in `kmain()`).
- Extensive `serial_puts()` debug logging is still present throughout the AMP/syscall-forwarding paths from active debugging sessions; treat serial output as a firehose during bring-up.
- Privileged instructions (`hlt`, `cli`, `sti`) are only safe when guaranteed to execute at CPL0 (kernel init, ISR/IRQ handlers, syscall handlers, or the secondary-CPU AMP main loop). Any new blocking/idle code reachable from ring-3 shell/user code must route through a syscall rather than executing them directly.
