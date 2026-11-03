#include <stdint.h>
#include <stddef.h>
#include "../vfs.h"
#include <fs/sys/sysfs.h>
#include <memory/page.h>
#include <generated/version.h>

static struct vfs_node sys_root;

static int copy_text(char *buffer, uint32_t size, const char *text) {
    uint32_t length = 0;

    if (buffer == 0 && size != 0) {
        return -1;
    }
    while (text[length] != '\0') {
        length++;
    }
    if (length > size) {
        length = size;
    }
    for (uint32_t index = 0; index < length; index++) {
        buffer[index] = text[index];
    }
    return (int)length;
}

static int copy_number(char *buffer, uint32_t size, uint32_t value) {
    char digits[10];
    uint32_t count = 0;
    uint32_t written = 0;

    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(digits));
    while (count != 0 && written < size) {
        buffer[written++] = digits[--count];
    }
    if (written < size) {
        buffer[written++] = '\n';
    }
    return (int)written;
}

static int read_name(struct vfs_node *node, char *buffer, uint32_t size) {
    (void)node;
    return copy_text(buffer, size, "OpenKernel\n");
}

static int read_version(struct vfs_node *node, char *buffer, uint32_t size) {
    (void)node;
    return copy_text(buffer, size, OPENKERNEL_VERSION "\n");
}

static int read_total_pages(struct vfs_node *node, char *buffer,
                            uint32_t size) {
    (void)node;
    return copy_number(buffer, size, memory_total_pages());
}

static int read_free_pages(struct vfs_node *node, char *buffer,
                           uint32_t size) {
    (void)node;
    return copy_number(buffer, size, memory_free_pages());
}

int sysfs_init(void) {
    sys_root.name[0] = '/';
    sys_root.name[1] = '\0';
    sys_root.flags = VFS_DIRECTORY;
    sys_root.parent = 0;
    sys_root.children = 0;
    sys_root.next = 0;
    sys_root.read = 0;
    sys_root.write = 0;
    sys_root.data = 0;
    sys_root.size = 0;

    if (vfs_create_file(&sys_root, "/kernel/name", read_name, 0) == 0 ||
        vfs_create_file(&sys_root, "/kernel/version", read_version, 0) == 0 ||
        vfs_create_file(&sys_root, "/memory/total_pages", read_total_pages, 0) == 0 ||
        vfs_create_file(&sys_root, "/memory/free_pages", read_free_pages, 0) == 0) {
        return -1;
    }
    return vfs_mount_fs(vfs_get_root(), "sys", &sys_root, 0, 0);
}
