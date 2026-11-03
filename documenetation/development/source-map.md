# Source tree guide

The following paths identify the main implementation areas:

| Path | Purpose |
| --- | --- |
| `kernel/entry.asm`, `kernel/start.c` | Multiboot entry and kernel startup. |
| `kernel/interrupts.asm`, `kernel/interrupts.c` | x86 interrupt stubs, IDT/PIC setup, exception/syscall dispatch. |
| `kernel/proc/`, `kernel/exec.c` | Program loading and limited execution-context handoff. |
| `fs/` | TAR reader, VFS, OKFS, and executable-format loaders. |
| `kernel/video/`, `kernel/bmp.c`, `kernel/desktop.c` | Framebuffer, framebuffer console, logo rendering, desktop helpers. |
| `sdrivers/` | Keyboard, mouse, and PC-speaker drivers. |
| `memory/` | Loader range checks and current panic-based OOM handler. |
| `initramfs/` | Tracked minimal initramfs template used to build the boot image. |
| `include/` | Public kernel and userspace C headers. |
| `modules/` | Module examples and build inputs; runtime support is incomplete. |
| `build_fs.sh`, `Makefile`, `make_iso.sh`, `run.sh` | Archive, kernel, ISO, and QEMU workflow. |

For subsystem behavior and limitations, start at the
[documentation index](../index.md). The implementation is the source of truth
when an older comment, README statement, or generated binary differs from
these pages.
