# Paging, memory allocation, and checks

The x86 boot path now enables 4 MiB identity-mapped paging and initializes a
physical page-frame bitmap from the Multiboot memory map. `memory_alloc_page()`
and `memory_alloc_pages()` allocate physical pages; matching free calls return
them to the bitmap. `/proc/meminfo` and `/sys/memory/` expose the usable and
free page counts.

The current map is a supervisor-only identity map of the 32-bit physical
address space. This is kernel paging, not process isolation: all programs
continue to execute at kernel privilege in a shared address space. The
allocator reserves the kernel image, Multiboot metadata, and initramfs module.

The older loader check `check_memory_quota(addr, size)` still rejects
wraparound and ranges ending above 16 MiB. It is not a general allocation
policy or per-process memory quota.

There are no per-process address spaces or memory protection boundaries.
Userspace executables run at kernel privilege in the shared address space, and
syscall pointers are generally trusted. A malformed or hostile program can
corrupt the kernel even if its load range passed the quota check.

The kernel publishes a page-aligned, versioned vDSO-like ABI page for a shared
syscall trampoline and a direct `getpid()` fast path. It is not independently
mapped or protected per process; it shares the current identity-mapped kernel
address space and must not be treated as a security boundary.

`oom_kill()` still logs the requested range and panics; it does not reclaim
memory or kill an isolated process. See [Programs and processes](processes.md)
for loader constraints and [Kernel panic](panic.md) for failure behavior.
