#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>
#include <kernel/syscall.h>

/*
 * OpenKernel's current 32-bit software-interrupt ABI:
 * EAX = syscall number; EBX, ECX, EDX = arguments; EAX = result.
 * Negative results are -errno values. These helpers require i386 code.
 */
static inline int32_t ok_syscall3(uint32_t number, uint32_t arg1,
                                  uint32_t arg2, uint32_t arg3) {
    int32_t result;
    __asm__ volatile ("int $0x80"
                      : "=a" (result)
                      : "0" (number), "b" (arg1), "c" (arg2), "d" (arg3)
                      : "memory", "cc");
    return result;
}

static inline int32_t ok_write(int fd, const void *buffer, uint32_t size) {
    const uint8_t *bytes = (const uint8_t *)buffer;
    uint32_t written = 0;

    while (written < size) {
        int32_t result = ok_syscall3(OK_SYS_WRITE, (uint32_t)fd,
                                     (uint32_t)(uintptr_t)(bytes + written),
                                     size - written);
        if (result < 0) {
            return written != 0 ? (int32_t)written : result;
        }
        if (result == 0) {
            break;
        }
        written += (uint32_t)result;
    }
    return (int32_t)written;
}

static inline void ok_exit(int status)
{
    (void)ok_syscall3(
        OK_SYS_EXIT,
        (uint32_t)status,
        0,
        0
    );

    for (;;) {
        __asm__ volatile ("pause");
    }
}

static inline int32_t ok_console_write(const void *buffer, uint32_t size) {
    const uint8_t *bytes = (const uint8_t *)buffer;
    uint32_t written = 0;

    if (buffer == 0 && size != 0) {
        return -14;
    }
    while (written < size) {
        int32_t result = ok_syscall3(
            OK_SYS_CONSOLE_WRITE,
            (uint32_t)(uintptr_t)(bytes + written), size - written, 0);
        if (result < 0) {
            return written != 0 ? (int32_t)written : result;
        }
        if (result == 0) {
            break;
        }
        written += (uint32_t)result;
    }
    return (int32_t)written;
}

static inline int32_t ok_putchar(char character) {
    return ok_console_write(&character, 1);
}

static inline int32_t ok_puts(const char *text) {
    uint32_t length = 0;
    int32_t result;

    if (text == 0) {
        return -14;
    }
    while (text[length] != '\0') {
        length++;
    }
    result = ok_console_write(text, length);
    if (result < 0) {
        return result;
    }
    result = ok_putchar('\n');
    return result < 0 ? result : (int32_t)(length + 1);
}

static inline int32_t ok_getpid(void) {
    return ok_syscall3(OK_SYS_GETPID, 0, 0, 0);
}

static inline int32_t ok_fork(void)
{
    return ok_syscall3(OK_SYS_FORK, 0, 0, 0);
}

static inline int32_t ok_clear(void) {
    return ok_syscall3(OK_SYS_CLEAR, 0, 0, 0);
}

static inline void ok_panic(void)
{
    (void)ok_syscall3(OK_SYS_PANIC, 0, 0, 0);

    for (;;) {
        __asm__ volatile ("pause");
    }
}

static inline int32_t ok_fb_write(uint32_t offset, const void *buffer,
                                  uint32_t size) {
    if (buffer == 0 && size != 0) {
        return -OK_EFAULT;
    }
    return ok_syscall3(OK_SYS_FB_WRITE, offset,
                       (uint32_t)(uintptr_t)buffer, size);
}

static inline int32_t ok_fb_put_pixel(uint32_t x, uint32_t y,
                                      uint32_t rgb) {
    return ok_syscall3(OK_SYS_FB_PUT_PIXEL, x, y, rgb);
}

static inline int32_t ok_fb_clear(uint32_t rgb) {
    return ok_syscall3(OK_SYS_FB_CLEAR, rgb, 0, 0);
}

static inline int32_t ok_fb_get_info(struct ok_fb_info *info) {
    if (info == 0) {
        return -OK_EFAULT;
    }
    return ok_syscall3(OK_SYS_FB_GET_INFO,
                       (uint32_t)(uintptr_t)info, 0, 0);
}

static inline int32_t ok_mkdev(const char *path, uint32_t type) {
    return ok_syscall3(OK_SYS_MKDEV, (uint32_t)(uintptr_t)path, type, 0);
}

static inline int32_t ok_device_read(const char *path, void *buffer,
									uint32_t size) {
	if (path == 0 || (buffer == 0 && size != 0)) {
		return -OK_EFAULT;
	}
	return ok_syscall3(OK_SYS_DEVICE_READ, (uint32_t)(uintptr_t)path,
					   (uint32_t)(uintptr_t)buffer, size);
}

static inline int32_t ok_device_write(const char *path, const void *buffer,
									 uint32_t size) {
	if (path == 0 || (buffer == 0 && size != 0)) {
		return -OK_EFAULT;
	}
	return ok_syscall3(OK_SYS_DEVICE_WRITE, (uint32_t)(uintptr_t)path,
					   (uint32_t)(uintptr_t)buffer, size);
}

static inline uint32_t ok_net_get_ipv4(void) {
	return (uint32_t)ok_syscall3(OK_SYS_NET_GET_IPV4, 0, 0, 0);
}

static inline int32_t ok_listdir(const char *path, char *buffer,
                                 uint32_t capacity) {
    if (path == 0 || buffer == 0 || capacity == 0) {
        return -14;
    }
    return ok_syscall3(OK_SYS_READDIR, (uint32_t)(uintptr_t)path,
                       (uint32_t)(uintptr_t)buffer, capacity);
}

static inline int32_t ok_load_oso(const char *path) {
    if (path == 0 || path[0] == '\0') {
        return -14;
    }
    return ok_syscall3(OK_SYS_LOAD_OSO,
                       (uint32_t)(uintptr_t)path, 0, 0);
}

static inline int32_t ok_yield(void) {
    return ok_syscall3(OK_SYS_YIELD, 0, 0, 0);
}

static inline int32_t ok_getchar(void) {
    int32_t result;

    do {
        result = ok_syscall3(OK_SYS_GETCHAR, 0, 0, 0);
        if (result == -OK_EAGAIN) {
            ok_yield();
        }
    } while (result == -OK_EAGAIN);
    return result;
}

#endif
