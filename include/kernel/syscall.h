#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <stdint.h>


#define OK_SYS_EXIT 0u
#define OK_SYS_WRITE  1u
#define OK_SYS_GETPID  2u
#define OK_SYS_YIELD  3u
#define OK_SYS_CONSOLE_WRITE 4u
#define OK_SYS_FORK   5u
#define OK_SYS_EXECVE 6u
#define OK_SYS_GETCHAR 7u
#define OK_SYS_CLEAR  8u
#define OK_SYS_MKDEV  9u
#define OK_SYS_READDIR 10u
#define OK_SYS_LOAD_OSO 11u

#define OK_DEVICE_CONSOLE 1u
#define OK_DEVICE_NULL    2u
#define OK_DEVICE_ZERO    3u
#define OK_DEVICE_RANDOM  4u
#define OK_DEVICE_KEYBOARD 5u

#define OK_STDIN_FILENO  0
#define OK_STDOUT_FILENO 1
#define OK_STDERR_FILENO 2

#define OK_SYS_PANIC 72

#define OK_SYS_FB_WRITE 12u
#define OK_SYS_FB_PUT_PIXEL 13u
#define OK_SYS_FB_CLEAR 14u
#define OK_SYS_FB_GET_INFO 15u
#define OK_SYS_DEVICE_READ 16u
#define OK_SYS_DEVICE_WRITE 17u
#define OK_SYS_NET_UDP_SEND 18u
#define OK_SYS_NET_UDP_RECEIVE 19u
#define OK_SYS_NET_GET_IPV4 20u
#define OK_SYS_MODULE_LOAD 21u
#define OK_SYS_MODULE_UNLOAD 22u
#define OK_SYS_VDSO 23u
#define OK_SYS_REBOOT 24u

#define OK_VDSO_ABI_VERSION 1u
#define OK_VDSO_PAGE_SIZE 4096u

struct ok_fb_info {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t bits_per_pixel;
};

struct ok_vdso_page {
	uint32_t abi_version;
	uint32_t size;
	volatile uint32_t current_pid;
	uint32_t syscall3;
	uint8_t reserved[OK_VDSO_PAGE_SIZE - 16u];
};



#define OK_EAGAIN 11
#define OK_EBADF  9
#define OK_EFAULT 14
#define OK_EIO    5
#define OK_ENOSYS 38
#define OK_ENOMEM 12

int32_t syscall_dispatch(uint32_t number, uint32_t arg1, uint32_t arg2,
						 uint32_t arg3);

#endif
