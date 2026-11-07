#include <stdint.h>
#include <kernel/vdso.h>

typedef char vdso_page_must_be_one_page[
    sizeof(struct ok_vdso_page) == OK_VDSO_PAGE_SIZE ? 1 : -1];

static struct ok_vdso_page shared_vdso
    __attribute__((aligned(OK_VDSO_PAGE_SIZE)));

static int32_t vdso_syscall3(uint32_t number, uint32_t arg1,
                             uint32_t arg2, uint32_t arg3) {
    __asm__ volatile ("int $0x80"
                      : "+a" (number)
                      : "b" (arg1), "c" (arg2), "d" (arg3)
                      : "memory", "cc");
    return (int32_t)number;
}

void vdso_init(void) {
    shared_vdso.abi_version = OK_VDSO_ABI_VERSION;
    shared_vdso.size = sizeof(shared_vdso);
    shared_vdso.current_pid = 0;
    shared_vdso.syscall3 = (uint32_t)(uintptr_t)vdso_syscall3;
}

void vdso_set_current_pid(uint32_t pid) {
    shared_vdso.current_pid = pid;
}

const struct ok_vdso_page *vdso_page(void) {
    return &shared_vdso;
}
