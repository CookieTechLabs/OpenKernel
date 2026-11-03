# VFS and storage

OpenKernel currently has two distinct storage mechanisms: an in-memory VFS
namespace and an ATA-backed OKFS flat-file store. They are not joined by a
mount that makes persistent files visible under VFS paths.

## VFS namespace

The VFS is a fixed-size in-memory tree. It supports absolute-path lookup,
directory creation, file/device nodes, directory listing, and mounts. Limits
in the current implementation are 256 allocated nodes, 16 mounts, and 64
bytes per node name. Startup mounts devtmpfs at `/dev` and as the compatibility
alias `/device`, and mounts generated procfs and sysfs trees at `/proc` and
`/sys`. TAR indexing adds archive paths as nodes, enabling `ls`/directory
lookup; indexed regular files do not currently get TAR-backed read callbacks,
so listing a file does not mean VFS can read its contents.

The `readdir` syscall returns newline-separated child names. Directories have
a trailing `/`. Listing output is limited to the syscall's 1024-byte buffer
maximum.

## Device nodes

Userspace can request device creation with `ok_mkdev(path, type)` from
`<user/syscall.h>`. Device types are defined in `<kernel/syscall.h>`:

- `OK_DEVICE_CONSOLE`: writes go to the active kernel console.
- `OK_DEVICE_NULL`: reads return end-of-file; writes are discarded.
- `OK_DEVICE_ZERO`: reads return zero-filled bytes; writes are discarded.
- `OK_DEVICE_RANDOM`: reads return bytes from a simple linear-congruential
  generator; this is not cryptographically secure.
- `OK_DEVICE_KEYBOARD`: reads consume available translated keyboard characters.

The kernel creates conventional device nodes during VFS initialization; init
does not create them. A created device node is not a general POSIX file
descriptor: the current syscall ABI has no `open`, `read`, or `close`
interface for arbitrary VFS files. `ok_device_read()` and
`ok_device_write()` access a path directly.

## OKFS persistent store

OKFS uses ATA PIO reads and writes at fixed LBAs and maintains a fixed-size
flat directory. It can store up to 64 entries, with names limited to 47 bytes
plus terminator and a maximum file size of 64 KiB. `okfs_read()` and
`okfs_write()` operate on absolute-looking flat names; directories and
hierarchical lookup are not implemented.

On mount, a missing or invalid superblock signature causes the region to be
formatted. Startup attempts to unpack regular files from the initramfs into
OKFS and writes a few system files. The unpacker skips boot paths and does not
create directories. There is currently no delete, rename, free-space
management, robust disk-capacity validation, or VFS mount integration. Do not
assume data can be listed from the shell merely because OKFS accepted a write.
