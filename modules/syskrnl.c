#include <stdint.h>
#include <kernel/module.h>
#include <kernel.h>
#include <generated/buildenv.h>
#include <kernel/syskrnl.h>

static int __init syskrnl_init(void)
{
    k_print("[MODULE] OpenKernel Build Platform: ");
    k_print(OPENKERNEL_BUILDENV);
    k_print_char('\n');

    return 0;
}

static void __exit syskrnl_exit(void)
{
    k_print("[MODULE] SysKrnl module exited!\n");
}

void syskrnl_load()
{
    syskrnl_init();
}

KERNEL_MODULE(syskrnl, syskrnl_init, syskrnl_exit);