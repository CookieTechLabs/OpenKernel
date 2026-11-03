#include <write/write.h>
#include <ksys/version.h>
#include <kernel/time.h>
#include <ksys/system-info.h>
#include <kernel.h>
#include <kernel/module.h>

void info_module_init(void) {
    k_print("----------------------------------------\n");
    k_print("[MODULE] Loaded: System Information\n");
    k_print("[MODULE] OpenKernel extension active.\n");
    k_print("----------------------------------------\n");
    sleep_ms(3000);
    k_print("----------------------------------------\n");
    k_print("Kernel version: ");
    k_print(K_VER);
    k_print("----------------------------------------\n");
    sleep_ms(2000);
    k_print("----------------------------------------\n");
    k_print("Codename: ");
    k_print(CODENAME);
    k_print("----------------------------------------\n");
    sleep_ms(2000);
    k_print("----------------------------------------\n");
    k_print("Release: ");
    k_print(RELEASE);
    k_print("----------------------------------------\n");
    sleep_ms(2000);
    k_print("----------------------------------------\n");
    k_print("Check out the kernel source at: ");
    k_print(GITHUB_REPO);
    k_print("----------------------------------------\n");
    sleep_ms(2000);
    k_print("Info complete: ");
    k_print_char('y');
}

static int __init info_module_start(void) {
    info_module_init();
    return 0;
}

static void __exit info_module_stop(void) {
}

KERNEL_MODULE(info, info_module_start, info_module_stop);