#include <stdint.h>

#include "../vfs.h"
#include "proc.h"
#include <kernel/metadata.h>
#include <memory/page.h>

/*
 * OpenKernel /proc filesystem.
 *
 * Virtual files are generated when read; no disk storage is used.
 */

static struct vfs_node proc_root;

static int
proc_read_version(struct vfs_node *node, char *buf, uint32_t size)
{
    const char *version = kernel_version;
    static const char prefix[] = "OpenKernel version ";
    uint32_t length = 0;
    uint32_t written = 0;

    (void)node;

    if (buf == 0)
        return -1;

    for (uint32_t i = 0; prefix[i] != '\0'; i++)
        length++;

    for (uint32_t i = 0; version[i] != '\0'; i++)
        length++;

    if (length < size)
        length++;

    if (size == 0)
        return 0;

    for (uint32_t i = 0; prefix[i] != '\0' && written < size; i++)
        buf[written++] = prefix[i];

    for (uint32_t i = 0; version[i] != '\0' && written < size; i++)
        buf[written++] = version[i];

    if (written < size)
        buf[written++] = '\n';

    return (int)written;
}

static int
proc_read_meminfo(struct vfs_node *node, char *buf, uint32_t size)
{
    static const char prefix[] = "Total pages: ";
    static const char middle[] = "\nFree pages: ";
    uint32_t used = 0;

    (void)node;
    if (buf == 0 && size != 0)
        return -1;

    for (uint32_t i = 0; prefix[i] != '\0' && used < size; i++)
        buf[used++] = prefix[i];

    uint32_t values[] = { memory_total_pages(), memory_free_pages() };
    for (uint32_t value_index = 0; value_index < 2; value_index++) {
        uint32_t value = values[value_index];
        char digits[10];
        uint32_t count = 0;
        do {
            digits[count++] = (char)('0' + value % 10u);
            value /= 10u;
        } while (value != 0 && count < sizeof(digits));
        while (count != 0 && used < size)
            buf[used++] = digits[--count];
        if (value_index == 0) {
            for (uint32_t i = 0; middle[i] != '\0' && used < size; i++)
                buf[used++] = middle[i];
        } else if (used < size) {
            buf[used++] = '\n';
        }
    }
    return (int)used;
}

int
procfs_init(void)
{
    struct vfs_node *version;

    /*
     * The mounted filesystem root is independent of the global VFS root.
     */
    proc_root.name[0] = '/';
    proc_root.name[1] = '\0';
    proc_root.flags = VFS_DIRECTORY;
    proc_root.parent = 0;
    proc_root.children = 0;
    proc_root.next = 0;
    proc_root.read = 0;
    proc_root.write = 0;
    proc_root.data = 0;
    proc_root.size = 0;

    version = vfs_create_file(&proc_root, "/version",
                              proc_read_version, 0);
    if (version == 0)
        return -1;
    if (vfs_create_file(&proc_root, "/meminfo", proc_read_meminfo, 0) == 0)
        return -1;

    if (vfs_mount_fs(vfs_get_root(), "proc", &proc_root, 0, 0) != 0)
        return -1;

    return 0;
}