#include <stdint.h>
#include <kernel/syscall.h>
#include <kernel/process.h>
#include <driver/keyboard.h>
#include <driver/net.h>
#include <kvideo/fb.h>
#include "../fs/vfs.h"
#include <write/write.h>
#include <kernel.h>
#include <kernel/module.h>
#include <kernel/vdso.h>
#include <kernel/acpi.h>

#define OK_SYSCALL_WRITE_MAX   4096u
#define OK_SYSCALL_READDIR_MAX 1024u
#define OK_SYSCALL_DEVICE_IO_MAX 4096u

extern int execve(const char *filename, char *const argv[],
                  char *const envp[]);

static int32_t console_write(const char *buffer, uint32_t size) {
    int result;

    if (buffer == 0 && size != 0) {
        return -OK_EFAULT;
    }

    if (size > OK_SYSCALL_WRITE_MAX) {
        size = OK_SYSCALL_WRITE_MAX;
    }

    if (size == 0) {
        return 0;
    }

    result = vfs_console_write(buffer, size);

    return result < 0 ? -OK_EIO : result;
}

int32_t syscall_dispatch(uint32_t number,
                         uint32_t arg1,
                         uint32_t arg2,
                         uint32_t arg3) {
    switch (number) {
    case OK_SYS_EXIT:
        process_exit_current((int32_t)arg1);
        return 0;

    case OK_SYS_WRITE: {
        const char *buffer = (const char *)(uintptr_t)arg2;

        if (arg1 != OK_STDOUT_FILENO &&
            arg1 != OK_STDERR_FILENO) {
            return -OK_EBADF;
        }

        return console_write(buffer, arg3);
    }

    case OK_SYS_CONSOLE_WRITE:
        return console_write(
            (const char *)(uintptr_t)arg1,
            arg2
        );

    case OK_SYS_GETPID:
        return (int32_t)process_current_pid();

    case OK_SYS_VDSO:
        return (int32_t)(uintptr_t)vdso_page();

    case OK_SYS_LOAD_OSO:
        if (arg1 == 0) {
            return -OK_EFAULT;
        }
        return process_load_oso((const char *)(uintptr_t)arg1);

    case OK_SYS_YIELD:
        __asm__ volatile ("pause" ::: "memory");
        return 0;

    case OK_SYS_FORK:
        return process_fork_current();

    case OK_SYS_EXECVE:
        return execve(
            (const char *)(uintptr_t)arg1,
            (char *const *)(uintptr_t)arg2,
            (char *const *)(uintptr_t)arg3
        );

    case OK_SYS_GETCHAR: {
        char character;

        if (!keyboard_try_get_char(&character)) {
            return -OK_EAGAIN;
        }

        return (uint8_t)character;
    }

    case OK_SYS_CLEAR:
        k_clear_screen();
        return 0;

    case OK_SYS_FB_WRITE:
        if (arg2 == 0 && arg3 != 0) {
            return -OK_EFAULT;
        }
        if (arg3 > 1024u * 1024u) {
            return -22;
        }
        return fb_write_at(arg1, (const void *)(uintptr_t)arg2, arg3) < 0
                   ? -OK_EIO
                   : (int32_t)arg3;

    case OK_SYS_FB_PUT_PIXEL:
        return fb_put_rgb_pixel(arg1, arg2, arg3) == 0 ? 0 : -OK_EIO;

    case OK_SYS_FB_CLEAR:
        if (!fb_is_ready()) {
            return -OK_EIO;
        }
        fb_clear(arg1);
        return 0;

    case OK_SYS_FB_GET_INFO: {
        struct ok_fb_info *info = (struct ok_fb_info *)(uintptr_t)arg1;

        if (info == 0) {
            return -OK_EFAULT;
        }
        if (!fb_is_ready()) {
            return -OK_EIO;
        }
        info->width = fb_width();
        info->height = fb_height();
        info->pitch = fb_pitch();
        info->bits_per_pixel = fb_bits_per_pixel();
        return 0;
    }

    case OK_SYS_MKDEV:
        if (arg1 == 0) {
            return -OK_EFAULT;
        }

        return vfs_create_device(
                   (const char *)(uintptr_t)arg1,
                   arg2
               ) == 0
                   ? 0
                   : -22;

    case OK_SYS_READDIR:
        if (arg1 == 0 || arg2 == 0 || arg3 == 0) {
            return -OK_EFAULT;
        }

        if (arg3 > OK_SYSCALL_READDIR_MAX) {
            return -22;
        }

        return vfs_list_directory(
            (const char *)(uintptr_t)arg1,
            (char *)(uintptr_t)arg2,
            arg3
        );

    case OK_SYS_DEVICE_READ:
        if (arg1 == 0 || (arg2 == 0 && arg3 != 0)) {
            return -OK_EFAULT;
        }
        if (arg3 > OK_SYSCALL_DEVICE_IO_MAX) {
            return -22;
        }
        {
            int result = vfs_read_file(
                vfs_get_root(),
                (const char *)(uintptr_t)arg1,
                (char *)(uintptr_t)arg2,
                arg3
            );
            return result == -11 ? -OK_EAGAIN :
                   result < 0 ? -OK_EIO : result;
        }

    case OK_SYS_DEVICE_WRITE:
        if (arg1 == 0 || (arg2 == 0 && arg3 != 0)) {
            return -OK_EFAULT;
        }
        if (arg3 > OK_SYSCALL_DEVICE_IO_MAX) {
            return -22;
        }
        {
            int result = vfs_write_file(
                vfs_get_root(),
                (const char *)(uintptr_t)arg1,
                (const char *)(uintptr_t)arg2,
                arg3
            );
            return result == -11 ? -OK_EAGAIN :
                   result < 0 ? -OK_EIO : result;
        }

    case OK_SYS_NET_UDP_SEND: {
        const struct net_udp_send_request *request =
            (const struct net_udp_send_request *)(uintptr_t)arg1;

        if (request == 0 || (request->data == 0 && request->length != 0)) {
            return -OK_EFAULT;
        }
        if (request->length > NET_UDP_PAYLOAD_MAX) {
            return -22;
        }
        int result = net_udp_send(request->destination_ipv4,
                                 request->source_port,
                                 request->destination_port,
                                 request->data, request->length);
        return result == -11 ? -OK_EAGAIN :
               result < 0 ? -OK_EIO : result;
    }

    case OK_SYS_NET_UDP_RECEIVE: {
        struct net_udp_packet *packet =
            (struct net_udp_packet *)(uintptr_t)arg2;
        int result;

        if (packet == 0) {
            return -OK_EFAULT;
        }
        result = net_udp_receive((uint16_t)arg1, packet);
        return result == -11 ? -OK_EAGAIN :
               result < 0 ? -OK_EIO : result;
    }

    case OK_SYS_NET_GET_IPV4:
        return (int32_t)net_ipv4_address();

    case OK_SYS_MODULE_LOAD:
        if (arg1 == 0) {
            return -OK_EFAULT;
        }
        return module_load_from_initrd((const char *)(uintptr_t)arg1) == 0
                   ? 0
                   : -22;

    case OK_SYS_MODULE_UNLOAD:
        if (arg1 == 0) {
            return -OK_EFAULT;
        }
        return module_remove((const char *)(uintptr_t)arg1) == 0
                   ? 0
                   : -22;

    case OK_SYS_REBOOT:
        acpi_reboot();
        return 0;

    case OK_SYS_PANIC:
        panic("OpenKernel: panicc syscall invoked");
        break;

    default:
        (void)arg1;
        (void)arg2;
        (void)arg3;

        return -OK_ENOSYS;
    }
}