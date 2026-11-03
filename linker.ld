ENTRY(_start)

SECTIONS
{
    . = 0x100000;
    __kernel_start = .;

    .multiboot : {
        *(.multiboot)
    }

    .text BLOCK(4K) : ALIGN(4K) {
        *(.text)
        *(.init.text)
        *(.exit.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata*)
    }

    .data BLOCK(4K) : ALIGN(4K) {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    }

    __kernel_modules_start = .;
    .kernel_modules : ALIGN(4) {
        KEEP(*(.kernel_modules))
    }
    __kernel_modules_end = .;
    __kernel_end = ALIGN(4K);
}