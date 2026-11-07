# Userspace system-call ABI

OpenKernel exposes a 32-bit software interrupt interface at vector `0x80`.
Inline wrappers are in `<user/syscall.h>`; syscall IDs and errno-like constants
are in `<kernel/syscall.h>`. The ABI uses EAX for the syscall number and result,
and EBX, ECX, and EDX for up to three arguments. Results below zero represent
errors. Programs are compiled for 32-bit x86.

## Implemented calls and wrappers

| Operation | User wrapper | Notes |
| --- | --- | --- |
| Write to stdout/stderr | `ok_write(fd, buf, size)` | Only descriptors 1 and 2 are accepted; maximum 4096 bytes per dispatch. |
| Write active console | `ok_console_write(buf, size)` | Maximum 4096 bytes per dispatch. |
| Get process ID | `ok_getpid()` | Returns the current cooperative process ID. |
| Yield | `ok_yield()` | Cooperatively schedules a runnable process; no timer preemption. |
| Reboot | `ok_reboot()` | Requests a system reboot using ACPI reset support or the keyboard-controller fallback; does not return. |
| Fork | `fork()` / `ok_fork()` | Clones the active stack; parent gets child PID, child gets zero. Address space remains shared. |
| Exit | `ok_exit(status)` | Returns from a loaded program when an execution context is active; PID 1 cannot exit. |
| Execute image | `execve(path, argv, envp)` | Looks up the image in initramfs; `argv` and `envp` are currently ignored. |
| Get keyboard character | `ok_getchar()` | Returns a translated character or waits by busy-retrying `-OK_EAGAIN`. |
| Clear text/console | `ok_clear()` | Clears the kernel console display. |
| Create device node | `ok_mkdev(path, type)` | Creates one of the supported device types. |
| List directory | `ok_listdir(path, buf, capacity)` | Newline-separated names; kernel maximum is 1024 bytes. |
| Load OSO | `ok_load_oso(path)` | Loads a fixed-address OSO image; see [Programs and processes](processes.md). |
| Insert/remove kernel module | `ok_module_load(path)`, `ok_module_unload(name)` | Loads an i386 ELF relocatable `.ko` from the initramfs or removes an inserted module. Modules execute at kernel privilege. |
| Shared syscall ABI | `ok_vdso_get()` | Returns the versioned shared ABI page. Its syscall trampoline uses `int 0x80`; `ok_getpid()` reads the current PID directly from the page. |
| Framebuffer | `ok_fb_*()` | Display info, raw-byte write, RGB pixel, and clear; see [Framebuffer API](framebuffer.md). |

Unknown syscall numbers return `-OK_ENOSYS`. See
[Programs and process execution](processes.md) for fork limitations.

## Error and safety model

The syscall layer uses negative values such as `-OK_EFAULT`, `-OK_EIO`, and
`-OK_ENOSYS`; some validation paths return `-22`. This is not a complete POSIX
ABI. In particular, most pointer arguments are dereferenced directly by the
kernel. Programs execute at kernel privilege in shared memory, so the syscall
interface does not provide a security boundary or protect the kernel from bad
pointers.

Framebuffer-specific behavior and limits are documented in
[Framebuffer API](framebuffer.md). The implementation and dispatch table are
in `kernel/syscall.c`.

The vDSO-like page is page-aligned and shared in the kernel's identity-mapped
address space. It is not mapped read-only or separately into process address
spaces; the current execution model has no per-process mappings or protection.
