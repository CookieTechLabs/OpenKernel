# Kernel module status

`<kernel/module.h>` defines the `kernel_module_t` descriptor, `KERNEL_MODULE()`
macro, and `__init` / `__exit` section attributes. The kernel can insert and
remove modules and load i386 ELF32 `ET_REL` `.ko` images from the initramfs.
At boot it scans `/lib/modules/` and attempts to load each `.ko`; a failed
module is reported and does not stop boot.

Module objects are not linked into `kernel.bin`. `make` builds only the
kernel; `make modules` places relocatable `.ko` files under
`build/modules/<version>/`, and `make os.iso` packages them under
`/lib/modules/<version>/` in the generated initramfs.

The loader supports allocated PROGBITS/NOBITS sections, `R_386_32` and
`R_386_PC32` relocations, a 4 MiB module-size limit, and the exported symbols
listed in `kernel/module.c`. Modules may also be inserted or removed with
`ok_module_load(path)` and `ok_module_unload(name)`. Removal calls the exit
hook and frees the module allocation.

This is not a security boundary: loaded code runs at kernel privilege, module
pointers are trusted, dependencies are not resolved, and only the built-in
kernel symbol export list can satisfy imports. Do not load untrusted modules.
