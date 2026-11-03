# OpenKernel architecture and status

OpenKernel is an experimental freestanding 32-bit x86 kernel. The bootable
kernel is entered through a Multiboot-compatible GRUB image, initializes a
small set of interrupt-driven devices, reads a USTAR initramfs, and executes
programs from that archive. Kernel and application C code is built for i386;
small assembly files provide the entry point, interrupt stubs, and process
context save/restore.

## Major subsystems

- **Boot and startup:** Multiboot metadata, boot options, device initialization,
  archive validation, and configured init execution ([Boot and startup](boot.md)).
- **Initramfs:** USTAR archive indexing, init scripts, and binary lookup
  ([Initramfs and init scripts](initramfs.md)).
- **Filesystems:** an in-memory VFS tree with devtmpfs, procfs, and sysfs, plus
  a separate ATA-backed flat-file store ([VFS and storage](storage.md)).
- **System calls:** a 32-bit `int $0x80` interface for console I/O, processes,
  devices, directory listing, and framebuffer drawing ([System calls](syscalls.md)).
- **Programs and process execution:** flat/ELF loaders, cooperative scheduling,
  and stack-cloning `fork()` ([Programs and processes](processes.md)).
- **Display and input:** Multiboot linear framebuffer, PS/2 keyboard and mouse,
  framebuffer console, and dormant desktop drawing functions
  ([Display and input](display-input.md)).
- **Sound, time, exceptions, and memory checks:** PC-speaker tones, a busy-loop
  delay, CPU exception reporting, and a simple address ceiling.

## Important current limitations

OpenKernel is not currently a protected microkernel. Loaded programs execute
at kernel privilege in shared memory. Paging currently uses a supervisor
identity map; cooperative scheduling switches stacks at syscall boundaries,
and fork clones only the active stack. There are no per-process page tables,
timer preemption, or user-memory validation. An arbitrary program can corrupt
the kernel.

Several interfaces are intentionally partial or dormant: desktop routines are
not invoked by normal boot, mouse input has no userspace syscall, module
dependencies are not resolved, and the persistent OKFS store is not mounted
into the VFS namespace. See the linked subsystem pages for specific behavior
and caveats.
