# OpenKernel

OpenKernel is an experimental freestanding 32-bit x86 kernel written primarily
in C, with assembly for the boot entry, interrupt stubs, and context handling.
It boots under GRUB/Multiboot, indexes a USTAR initramfs, and provides a small
set of kernel and userspace interfaces. It is not currently a protected
microkernel: loaded programs execute at kernel privilege in shared memory.

## Documentation

The [feature documentation index](documenetation/index.md) covers the
architecture, build process, boot options, initramfs, VFS and persistent
storage, syscall ABI, executable formats, shell, framebuffer, keyboard/mouse,
sound/timing, interrupts, memory checks, and module status. It also documents
known limitations and inactive scaffolding.

## Build and run

See [Build and run](documenetation/setup/building.md) for prerequisites and
workflow. In brief, build the kernel with `make`; to build the ISO and start
QEMU use `make run-linux`. The normal build targets 32-bit x86 and requires
GCC-compatible `-m32` support and NASM. ISO creation and execution also require
GRUB tools, ISO utilities, and QEMU.

The `build/openkernel` Linux executable is only a QEMU launcher; it is not a
hosted kernel or Linux implementation of the OpenKernel shell.

## User program packaging

`okcc.sh input.c output.okx` creates an OKX1 flat executable with `_start` as
its entry point. `okcc.sh input.c output.oso` creates an OSO1 fixed-address
object using `_oso_entry`. OSO objects are loaded with `oso_load()` from
`<user/process.h>`. See [Programs and process execution](documenetation/kernel/processes.md)
for format details and restrictions.

The kernel now enables x86 paging, tracks physical frames, mounts devtmpfs,
procfs, and sysfs, supports cooperative scheduling at `yield()`, and can
insert supported ELF32 kernel modules from the initramfs. `fork()` clones the
active stack and resumes the child at the next scheduling point, but processes
still share kernel memory and run at kernel privilege; this is not POSIX
isolation. Do not run untrusted programs or modules. Consult the docs for
implementation limits.
