#ifndef USER_PROCESS_H
#define USER_PROCESS_H

#include <stdint.h>
#include <kernel/syscall.h>
#include <user/syscall.h>

typedef int32_t (*oso_entry_t)(void);

/*
 * Process ABI wrappers for OpenKernel's 32-bit int 0x80 interface.
 * fork() cooperatively clones the active stack but shares the kernel address
 * space. execve() and launch() load a named initramfs image.
 */
static inline int32_t fork(void) {
    return ok_syscall3(OK_SYS_FORK, 0, 0, 0);
}

static inline int32_t getpid(void) {
    return ok_getpid();
}

static inline void process_exit(int32_t status) {
    ok_exit(status);
}

/* Loads an OSO and returns its int32_t(void) entry, or NULL on failure. */
static inline oso_entry_t oso_load(const char *path) {
    int32_t result = ok_load_oso(path);
    return result < 0 ? 0 : (oso_entry_t)(uintptr_t)(uint32_t)result;
}

static inline int32_t execve(const char *path, char *const argv[],
                             char *const envp[]) {
    return ok_syscall3(OK_SYS_EXECVE,
                       (uint32_t)(uintptr_t)path,
                       (uint32_t)(uintptr_t)argv,
                       (uint32_t)(uintptr_t)envp);
}

static inline int32_t launch(const char *path, char *const argv[],
                             char *const envp[]) {
    return execve(path, argv, envp);
}

#endif
