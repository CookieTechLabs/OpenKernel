# Programs and process execution

## Current process model

OpenKernel runs programs directly in the kernel's 32-bit address space.
Cooperative scheduling can switch saved interrupt frames when a process calls
`yield()`. `fork()` clones the active 16 KiB stack and returns the child PID in
the parent and zero in the child; the child becomes runnable at the next
yield. Exiting a forked child resumes another runnable process.

This is not process isolation: the child stack is private, but code, globals,
heap, devices, and kernel state remain shared, and every process runs at kernel
privilege. There is no timer preemption or `wait()` syscall. Programs must
cooperate at syscall/yield boundaries, and a bad pointer or untrusted program
can corrupt the kernel. Do not assume POSIX semantics beyond the documented
fork return behavior.

`execve(path, argv, envp)` searches the initramfs for `path` and loads the
image. `argv` and `envp` are currently ignored. Programs normally enter
through the shell or an init script's `exec` command. PID 1 is the init
program; attempting to exit PID 1 panics. The wrappers are declared in
`<user/process.h>`.

The older `kernel/user.c` interface only updates a single static
`user_process_t` record. It is not used by `execve()` and does not start an
isolated process; see [Metadata, assertions, and warnings](metadata-assertions.md)
for that legacy interface's status.

## Executable formats

The execution paths recognize multiple formats; they are not interchangeable
and some are legacy/basic loaders:

- **OKX1:** Flat executable produced by `okcc.sh input.c output.okx`. The
  current script links at `0x200000` and uses `_start`; the header supplies a
  load address, entry offset, and payload size.
- **OSO1:** Flat shared-object-style image produced by `okcc.sh input.c
  output.oso`. It is loaded at `0x300000`; the expected entry symbol is
  `_oso_entry`. Use `oso_load()` from `<user/process.h>` to get an
  `int32_t(void)` entry pointer. The format is not relocatable, has a fixed
  1 MiB region, does not support unloading, and cannot safely host multiple
  independent objects at that same address.
- **PMX1:** Legacy flat image with load-address and entry-offset fields. Its
  quota check panics on failure; entry-offset validation is limited.
- **ELF32 i386:** A basic segment loader copies loadable segments and clears
  BSS. It is not a dynamic linker and does not apply relocations.
- **Raw image:** Legacy fallback behavior differs between `execve()` and the
  kernel init-script runner; prefer the explicit packaged formats.

The loader implementations are in `fs/binfmt_okx.c`, `fs/binfmt_pmx.c`, and
`fs/elf_stub.c`; dispatch is in `kernel/proc/exec.c` and `kernel/exec.c`.

## Memory and security warnings

Images execute at kernel privilege. The current address-quota check only
rejects ranges that overflow or end above 16 MiB; it does not allocate memory,
track per-program ownership, or prevent programs from accessing other kernel
memory. Do not execute untrusted binaries. See [Memory checks](memory.md) and
[System-call ABI](syscalls.md).
