#ifndef VFS_H
#define VFS_H

#include <stdint.h>

#define VFS_DIRECTORY 0x01
#define VFS_DEVICE    0x02
#define VFS_FILE      0x04
#define VFS_MOUNTED   0x08

#define VFS_DEVICE_CONSOLE 1u
#define VFS_DEVICE_NULL    2u
#define VFS_DEVICE_ZERO    3u
#define VFS_DEVICE_RANDOM  4u
#define VFS_DEVICE_KEYBOARD 5u
#define VFS_DEVICE_NETWORK 6u

#define VFS_MAX_PATH 256
#define VFS_NAME_SIZE 64

struct vfs_node {
    char name[VFS_NAME_SIZE];
    uint32_t flags;
    struct vfs_node *parent;
    struct vfs_node *children;
    struct vfs_node *next;
    int (*read)(struct vfs_node *node, char *buf, uint32_t size);
    int (*write)(struct vfs_node *node, const char *buf, uint32_t size);
    void *data;
    uint32_t size;
};

struct vfs_mount {
    char name[32];
    struct vfs_node *root;
    int (*read)(const char *path, void *buf, uint32_t size);
    int (*write)(const char *path, const void *buf, uint32_t size);
};

void vfs_init(void);
void mount_essential_folders(void);
struct vfs_node *vfs_get_root(void);
struct vfs_node *vfs_create_node(const char *name, uint32_t flags, struct vfs_node *parent);
struct vfs_node *vfs_lookup(struct vfs_node *root, const char *path);
struct vfs_node *vfs_mkdir(struct vfs_node *root, const char *path);
struct vfs_node *vfs_create_file(struct vfs_node *root, const char *path,
                                int (*reader)(struct vfs_node *node, char *buf, uint32_t size),
                                int (*writer)(struct vfs_node *node, const char *buf, uint32_t size));
int vfs_mount_fs(struct vfs_node *parent, const char *name, struct vfs_node *root,
                 int (*reader)(const char *path, void *buf, uint32_t size),
                 int (*writer)(const char *path, const void *buf, uint32_t size));
int vfs_exists(struct vfs_node *root, const char *path);
int vfs_read_file(struct vfs_node *root, const char *path, char *buf, uint32_t size);
int vfs_write_file(struct vfs_node *root, const char *path, const char *buf, uint32_t size);
int vfs_set_console(const char *path);
int vfs_console_write(const char *buffer, uint32_t size);
int vfs_create_device(const char *path, uint32_t type);
int vfs_list_directory(const char *path, char *buffer, uint32_t capacity);
int vfs_index_tar(uint32_t tar_address);

#endif