# Userspace device nodes

Userspace can create VFS device nodes with `ok_mkdev(path, type)` from
`<user/syscall.h>`. Valid type constants are `OK_DEVICE_CONSOLE`,
`OK_DEVICE_NULL`, `OK_DEVICE_ZERO`, `OK_DEVICE_RANDOM`, and
`OK_DEVICE_KEYBOARD` in `<kernel/syscall.h>`.

- Console devices send writes to the active kernel console.
- Null devices discard writes and return end-of-file on reads.
- Zero devices fill read buffers with zero bytes.
- Random devices return a simple pseudo-random byte stream; it is not suitable
  for cryptographic use.
- Keyboard devices return characters already available from the keyboard
  queue.

The kernel mounts devtmpfs at `/dev` and also exposes the same namespace at
`/device` for compatibility. It creates conventional paths during startup:
`console`, `fb0`, `null`, `zero`, `random`, `keyboard`, and `tty0` through
`tty9`. Init does not create device nodes.

This API creates namespace entries only. It does not provide general POSIX
file descriptors: the syscall ABI currently has no userspace `open()`,
`read()`, or `close()` operation for these nodes. Framebuffer access is through
the dedicated `ok_fb_*()` syscalls, not `/device/fb0`; see
[Framebuffer API](../kernel/framebuffer.md).
