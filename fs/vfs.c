#include "vfs.h"
#include <kernel/tar.h>
#include <write/write.h>
#include <driver/keyboard.h>
#include <driver/net.h>
#include "proc/proc.h"
#include <fs/sys/sysfs.h>
#include <memory/page.h>

#define MAX_VFS_NODES 256
#define MAX_VFS_MOUNTS 16
static struct vfs_node node_pool[MAX_VFS_NODES];
static struct vfs_mount mount_pool[MAX_VFS_MOUNTS];
static uint32_t node_count = 0;
static uint32_t mount_count = 0;
static struct vfs_node root_node = { "/", VFS_DIRECTORY, 0, 0, 0, 0, 0, 0, 0 };
static struct vfs_node dev_root;
static struct vfs_node *active_console;
static uint32_t random_state = 0x4F70656Bu;

static int console_device_write(struct vfs_node *node, const char *buffer,
                                uint32_t size) {
    (void)node;
    if (buffer == 0 && size != 0) {
        return -1;
    }
    for (uint32_t index = 0; index < size; index++) {
        k_print_char(buffer[index]);
    }
    return (int)size;
}

static int device_read(struct vfs_node *node, char *buffer, uint32_t size) {
    uint32_t type = (uint32_t)(uintptr_t)node->data;

    if (buffer == 0 && size != 0) {
        return -1;
    }
    if (type == VFS_DEVICE_NULL || type == VFS_DEVICE_CONSOLE) {
        return 0;
    }
    if (type == VFS_DEVICE_KEYBOARD) {
        uint32_t copied = 0;
        while (copied < size) {
            char character;
            if (!keyboard_try_get_char(&character)) {
                break;
            }
            buffer[copied++] = character;
        }
        return (int)copied;
    }
    if (type == VFS_DEVICE_NETWORK) {
        return net_device_read(buffer, size);
    }
    for (uint32_t index = 0; index < size; index++) {
        if (type == VFS_DEVICE_RANDOM) {
            random_state = random_state * 1664525u + 1013904223u;
            buffer[index] = (char)(random_state >> 24);
        } else {
            buffer[index] = 0;
        }
    }
    return (int)size;
}

static int device_write(struct vfs_node *node, const char *buffer,
                        uint32_t size) {
    uint32_t type = (uint32_t)(uintptr_t)node->data;

    if (buffer == 0 && size != 0) {
        return -1;
    }
    if (type == VFS_DEVICE_CONSOLE) {
        return console_device_write(node, buffer, size);
    }
    if (type == VFS_DEVICE_NETWORK) {
        return net_device_write(buffer, size);
    }
    return (int)size;
}

static void vfs_reset_node(struct vfs_node *node) {
    if (node == 0) {
        return;
    }
    node->name[0] = '\0';
    node->flags = 0;
    node->parent = 0;
    node->children = 0;
    node->next = 0;
    node->read = 0;
    node->write = 0;
    node->data = 0;
    node->size = 0;
}

static int vfs_name_is_valid(const char *name) {
    if (name == 0 || name[0] == '\0') {
        return 0;
    }
    for (int index = 0; name[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
        if (name[index] == '/' && index != 0) {
            return 0;
        }
    }
    return 1;
}

static const char *vfs_next_path_component(const char *path, char *component, uint32_t component_size) {
    uint32_t index = 0;
    uint32_t component_index = 0;

    while (path[index] == '/') {
        index++;
    }
    while (path[index] != '\0' && path[index] != '/' && component_index + 1 < component_size) {
        component[component_index++] = path[index++];
    }
    component[component_index] = '\0';
    while (path[index] == '/') {
        index++;
    }
    return path + index;
}

struct vfs_node *vfs_get_root(void) {
    return &root_node;
}

struct vfs_node *vfs_create_node(const char *name, uint32_t flags, struct vfs_node *parent) {
    struct vfs_node *node;
    int index = 0;

    if (node_count >= MAX_VFS_NODES || !vfs_name_is_valid(name) || parent == 0) {
        return 0;
    }

    node = &node_pool[node_count++];
    vfs_reset_node(node);
    while (name[index] != '\0' && index < VFS_NAME_SIZE - 1) {
        node->name[index] = name[index];
        index++;
    }
    node->name[index] = '\0';
    node->flags = flags;
    node->parent = parent;
    node->children = 0;
    node->next = parent->children;
    parent->children = node;
    return node;
}

struct vfs_node *vfs_lookup(struct vfs_node *root, const char *path) {
    struct vfs_node *current = root;
    char component[VFS_NAME_SIZE];
    const char *rest = path;

    if (root == 0 || path == 0 || path[0] != '/') {
        return 0;
    }

    while (*rest != '\0') {
        if (current->flags & VFS_MOUNTED) {
            struct vfs_mount *mount = (struct vfs_mount *)current->data;
            if (mount == 0 || mount->root == 0) {
                return 0;
            }
            current = mount->root;
        }
        rest = vfs_next_path_component(rest, component, sizeof(component));
        if (component[0] == '\0') {
            break;
        }

        struct vfs_node *child = current->children;
        int found = 0;
        while (child != 0) {
            if (vfs_name_is_valid(child->name) && child->name[0] == component[0] &&
                (child->name[1] == '\0' || child->name[1] == component[1])) {
                int match = 1;
                for (int index = 0; component[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
                    if (child->name[index] != component[index]) {
                        match = 0;
                        break;
                    }
                }
                if (match) {
                    current = child;
                    found = 1;
                    break;
                }
            }
            child = child->next;
        }
        if (!found) {
            return 0;
        }
    }
    if (current->flags & VFS_MOUNTED) {
        struct vfs_mount *mount = (struct vfs_mount *)current->data;
        return mount != 0 ? mount->root : 0;
    }
    return current;
}

struct vfs_node *vfs_mkdir(struct vfs_node *root, const char *path) {
    struct vfs_node *parent = root;
    char component[VFS_NAME_SIZE];
    const char *rest = path;

    if (root == 0 || path == 0 || path[0] != '/') {
        return 0;
    }

    while (*rest != '\0') {
        rest = vfs_next_path_component(rest, component, sizeof(component));
        if (component[0] == '\0') {
            break;
        }
        if (rest[0] == '\0') {
            struct vfs_node *child = parent->children;
            while (child != 0) {
                if (child->name[0] == component[0] && child->flags & VFS_DIRECTORY) {
                    int match = 1;
                    for (int index = 0; component[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
                        if (child->name[index] != component[index]) {
                            match = 0;
                            break;
                        }
                    }
                    if (match) {
                        return child;
                    }
                }
                child = child->next;
            }
            return vfs_create_node(component, VFS_DIRECTORY, parent);
        }

        struct vfs_node *child = parent->children;
        int found = 0;
        while (child != 0) {
            int match = 1;
            for (int index = 0; component[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
                if (child->name[index] != component[index]) {
                    match = 0;
                    break;
                }
            }
            if (match && (child->flags & VFS_DIRECTORY)) {
                parent = child;
                found = 1;
                break;
            }
            child = child->next;
        }
        if (!found) {
            struct vfs_node *new_dir = vfs_create_node(component, VFS_DIRECTORY, parent);
            if (new_dir == 0) {
                return 0;
            }
            parent = new_dir;
        }
    }

    return parent;
}

struct vfs_node *vfs_create_file(struct vfs_node *root, const char *path,
                                int (*reader)(struct vfs_node *node, char *buf, uint32_t size),
                                int (*writer)(struct vfs_node *node, const char *buf, uint32_t size)) {
    struct vfs_node *parent = root;
    char component[VFS_NAME_SIZE];
    const char *rest = path;
    struct vfs_node *file = 0;

    if (root == 0 || path == 0 || path[0] != '/') {
        return 0;
    }

    while (*rest != '\0') {
        rest = vfs_next_path_component(rest, component, sizeof(component));
        if (component[0] == '\0') {
            break;
        }
        if (rest[0] == '\0') {
            struct vfs_node *node = parent->children;
            while (node != 0) {
                if (node->name[0] == component[0] && node->name[1] == component[1]) {
                    int match = 1;
                    for (int index = 0; component[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
                        if (node->name[index] != component[index]) {
                            match = 0;
                            break;
                        }
                    }
                    if (match) {
                        return node;
                    }
                }
                node = node->next;
            }
            file = vfs_create_node(component, VFS_FILE, parent);
            if (file != 0) {
                file->read = reader;
                file->write = writer;
                file->data = 0;
            }
            return file;
        }

        struct vfs_node *child = parent->children;
        int found = 0;
        while (child != 0) {
            int match = 1;
            for (int index = 0; component[index] != '\0' && index < VFS_NAME_SIZE - 1; index++) {
                if (child->name[index] != component[index]) {
                    match = 0;
                    break;
                }
            }
            if (match && (child->flags & VFS_DIRECTORY)) {
                parent = child;
                found = 1;
                break;
            }
            child = child->next;
        }
        if (!found) {
            parent = vfs_create_node(component, VFS_DIRECTORY, parent);
            if (parent == 0) {
                return 0;
            }
        }
    }
    return file;
}

static int __internal_mount(struct vfs_node *parent, const char *name,
                            struct vfs_node *root,
                            int (*reader)(const char *path, void *buf,
                                          uint32_t size),
                            int (*writer)(const char *path, const void *buf,
                                          uint32_t size)) {
    struct vfs_node *mount_node;
    struct vfs_node *child;
    struct vfs_mount *mount;
    uint32_t index = 0;

    if (parent == 0 || !(parent->flags & VFS_DIRECTORY) || name == 0 ||
        name[0] == '\0' || root == 0 || mount_count >= MAX_VFS_MOUNTS) {
        return -1;
    }
    while (name[index] != '\0') {
        if (name[index] == '/' || index >= sizeof(mount->name) - 1) {
            return -1;
        }
        index++;
    }
    for (child = parent->children; child != 0; child = child->next) {
        uint32_t character = 0;
        while (name[character] != '\0' &&
               child->name[character] == name[character]) {
            character++;
        }
        if (name[character] == '\0' && child->name[character] == '\0') {
            return -1;
        }
    }

    mount_node = vfs_create_node(name, VFS_DIRECTORY | VFS_MOUNTED, parent);
    if (mount_node == 0) {
        return -1;
    }
    mount = &mount_pool[mount_count++];
    for (index = 0; index < sizeof(mount->name); index++) {
        mount->name[index] = name[index];
        if (name[index] == '\0') {
            break;
        }
    }
    mount->root = root;
    mount->read = reader;
    mount->write = writer;
    mount_node->data = mount;
    mount_node->read = 0;
    mount_node->write = 0;
    return 0;
}

int vfs_mount_fs(struct vfs_node *parent, const char *name, struct vfs_node *root,
                 int (*reader)(const char *path, void *buf, uint32_t size),
                 int (*writer)(const char *path, const void *buf, uint32_t size)) {
    return __internal_mount(parent, name, root, reader, writer);
}

int vfs_exists(struct vfs_node *root, const char *path) {
    return vfs_lookup(root, path) != 0;
}

int vfs_read_file(struct vfs_node *root, const char *path, char *buf, uint32_t size) {
    struct vfs_node *node = vfs_lookup(root, path);

    if (node == 0 || buf == 0) {
        return -1;
    }
    if (node->flags & VFS_MOUNTED) {
        struct vfs_mount *mount = (struct vfs_mount *)node->data;
        return mount != 0 && mount->read != 0
                   ? mount->read("/", buf, size) : -1;
    }
    if (node->read == 0) {
        return -1;
    }
    return node->read(node, buf, size);
}

int vfs_write_file(struct vfs_node *root, const char *path, const char *buf, uint32_t size) {
    struct vfs_node *node = vfs_lookup(root, path);

    if (node == 0 || buf == 0) {
        return -1;
    }
    if (node->flags & VFS_MOUNTED) {
        struct vfs_mount *mount = (struct vfs_mount *)node->data;
        return mount != 0 && mount->write != 0
                   ? mount->write("/", buf, size) : -1;
    }
    if (node->write == 0) {
        return -1;
    }
    return node->write(node, buf, size);
}

int vfs_set_console(const char *path) {
    struct vfs_node *node = vfs_lookup(vfs_get_root(), path);
    static const char tty_prefix[] = "/device/tty";

    if (path == 0) {
        return -1;
    }
    if (node == 0) {
        uint32_t index = 0;
        while (tty_prefix[index] != '\0' && path[index] == tty_prefix[index]) {
            index++;
        }
        if (tty_prefix[index] != '\0' || path[index] == '\0') {
            return -1;
        }
        while (path[index] >= '0' && path[index] <= '9') {
            index++;
        }
        if (path[index] != '\0') {
            return -1;
        }
        node = vfs_lookup(vfs_get_root(), "/device/console");
    }
    if (node == 0 || !(node->flags & VFS_DEVICE) || node->write == 0) {
        return -1;
    }
    active_console = node;
    return 0;
}

int vfs_console_write(const char *buffer, uint32_t size) {
    if (active_console == 0 || active_console->write == 0 ||
        (buffer == 0 && size != 0)) {
        return -1;
    }
    return active_console->write(active_console, buffer, size);
}

int vfs_create_device(const char *path, uint32_t type) {
    struct vfs_node *node;
    struct vfs_node *parent;
    char parent_path[VFS_MAX_PATH];
    uint32_t path_length = 0;
    uint32_t separator = 0;
    uint32_t name_length;

    if (path == 0 || path[0] != '/' || path[1] == '\0' ||
        (type != VFS_DEVICE_CONSOLE && type != VFS_DEVICE_NULL &&
         type != VFS_DEVICE_ZERO && type != VFS_DEVICE_RANDOM &&
         type != VFS_DEVICE_KEYBOARD && type != VFS_DEVICE_NETWORK)) {
        return -1;
    }
    while (path[path_length] != '\0' && path_length < sizeof(parent_path)) {
        if (path[path_length] == '/') {
            separator = path_length;
        }
        path_length++;
    }
    if (path_length >= sizeof(parent_path) || separator + 1 == path_length) {
        return -1;
    }
    name_length = path_length - separator - 1;
    if (name_length >= VFS_NAME_SIZE) {
        return -1;
    }
    if (separator == 0) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        for (uint32_t index = 0; index < separator; index++) {
            parent_path[index] = path[index];
        }
        parent_path[separator] = '\0';
    }
    parent = vfs_lookup(vfs_get_root(), parent_path);
    if (parent == 0 || !(parent->flags & VFS_DIRECTORY)) {
        return -1;
    }
    node = vfs_lookup(vfs_get_root(), path);
    if (node != 0 && !(node->flags & VFS_DEVICE)) {
        return -1;
    }
    if (node == 0) {
        char name[VFS_NAME_SIZE];
        for (uint32_t index = 0; index < name_length; index++) {
            name[index] = path[separator + index + 1];
        }
        name[name_length] = '\0';
        node = vfs_create_node(name, VFS_FILE | VFS_DEVICE, parent);
    }
    if (node == 0) {
        return -1;
    }

    node->flags = VFS_FILE | VFS_DEVICE;
    node->read = device_read;
    node->write = device_write;
    node->data = (void *)(uintptr_t)type;
    if (type == VFS_DEVICE_CONSOLE && active_console == 0) {
        active_console = node;
    }
    return 0;
}

int vfs_list_directory(const char *path, char *buffer, uint32_t capacity) {
    struct vfs_node *directory;
    uint32_t used = 0;

    if (buffer == 0 || capacity == 0) {
        return -1;
    }
    directory = vfs_lookup(vfs_get_root(), path);
    if (directory == 0 || !(directory->flags & VFS_DIRECTORY)) {
        return -1;
    }

    for (struct vfs_node *child = directory->children; child != 0;
         child = child->next) {
        uint32_t length = 0;
        while (length < VFS_NAME_SIZE && child->name[length] != '\0') {
            length++;
        }
        uint32_t suffix = (child->flags & VFS_DIRECTORY) ? 1u : 0u;
        if (used + length + suffix + 1 >= capacity) {
            buffer[used] = '\0';
            return -2;
        }
        for (uint32_t index = 0; index < length; index++) {
            buffer[used++] = child->name[index];
        }
        if (suffix != 0) {
            buffer[used++] = '/';
        }
        buffer[used++] = '\n';
    }
    buffer[used] = '\0';
    return (int)used;
}

static uint32_t vfs_tar_size(const char *field) {
    uint32_t size = 0;

    for (uint32_t index = 0; index < 11; index++) {
        if (field[index] >= '0' && field[index] <= '7') {
            size = size * 8 + (uint32_t)(field[index] - '0');
        }
    }
    return size;
}

int vfs_index_tar(uint32_t tar_address) {
    struct tar_header *header = (struct tar_header *)(uintptr_t)tar_address;
    int indexed = 0;

    if (header == 0 || header->magic[0] != 'u' || header->magic[1] != 's' ||
        header->magic[2] != 't' || header->magic[3] != 'a' ||
        header->magic[4] != 'r') {
        return -1;
    }

    while (header->name[0] != '\0') {
        char path[VFS_MAX_PATH];
        uint32_t path_length = 1;
        uint32_t prefix_length = 0;
        uint32_t name_offset = 0;
        int valid = 1;

        path[0] = '/';
        path[1] = '\0';

        while (prefix_length < sizeof(header->prefix) &&
               header->prefix[prefix_length] != '\0') {
            if (path_length + 1 >= sizeof(path)) {
                valid = 0;
                break;
            }
            path[path_length++] = header->prefix[prefix_length++];
        }
        if (valid && prefix_length != 0) {
            if (path_length + 1 >= sizeof(path)) {
                valid = 0;
            } else {
                path[path_length++] = '/';
            }
        }

        if (header->name[0] == '.' && header->name[1] == '/') {
            name_offset = 2;
        }
        while (valid && header->name[name_offset] != '\0') {
            if (path_length + 1 >= sizeof(path)) {
                valid = 0;
                break;
            }
            path[path_length++] = header->name[name_offset++];
        }
        path[path_length] = '\0';

        if (valid && path_length > 1) {
            if (header->typeflag == '5') {
                if (vfs_mkdir(vfs_get_root(), path) == 0) {
                    return -1;
                }
                indexed++;
            } else if (header->typeflag == '\0' || header->typeflag == '0') {
                if (vfs_create_file(vfs_get_root(), path, 0, 0) == 0) {
                    return -1;
                }
                indexed++;
            }
        }

        uint32_t size = vfs_tar_size(header->size);
        tar_address += 512 + ((size + 511) / 512) * 512;
        header = (struct tar_header *)(uintptr_t)tar_address;
    }

    return indexed;
}

void mount_essential_folders(void) {
    static int mounted;

    if (mounted) {
        return;
    }
    mounted = 1;
    dev_root.name[0] = '/';
    dev_root.name[1] = '\0';
    dev_root.flags = VFS_DIRECTORY;
    dev_root.parent = 0;
    dev_root.children = 0;
    dev_root.next = 0;
    dev_root.read = 0;
    dev_root.write = 0;
    dev_root.data = 0;
    dev_root.size = 0;

    if (vfs_mount_fs(vfs_get_root(), "dev", &dev_root, 0, 0) != 0 ||
        vfs_mount_fs(vfs_get_root(), "device", &dev_root, 0, 0) != 0) {
        mounted = 0;
        k_print("[VFS] Failed to mount devtmpfs.\n");
        return;
    }

    if (vfs_create_device("/dev/console", VFS_DEVICE_CONSOLE) != 0 ||
        vfs_create_device("/dev/null", VFS_DEVICE_NULL) != 0 ||
        vfs_create_device("/dev/zero", VFS_DEVICE_ZERO) != 0 ||
        vfs_create_device("/dev/random", VFS_DEVICE_RANDOM) != 0 ||
        vfs_create_device("/dev/keyboard", VFS_DEVICE_KEYBOARD) != 0 ||
        vfs_create_device("/dev/fb0", VFS_DEVICE_CONSOLE) != 0) {
        k_print("[VFS] Failed to populate devtmpfs.\n");
        return;
    }
    for (char terminal = '0'; terminal <= '9'; terminal++) {
        char path[] = "/dev/tty0";
        path[8] = terminal;
        if (vfs_create_device(path, VFS_DEVICE_CONSOLE) != 0) {
            k_print("[VFS] Failed to create a tty device.\n");
            return;
        }
    }
    k_print("[VFS] Mounted devtmpfs at /dev.\n");
}

void vfs_init(void) {
    static int initialised = 0;

    if (initialised) {
        return;
    }
    initialised = 1;
    root_node.parent = 0;
    root_node.children = 0;
    root_node.next = 0;
    root_node.flags = VFS_DIRECTORY;
    root_node.name[0] = '/';
    root_node.name[1] = '\0';
    mount_essential_folders();

    if (procfs_init() != 0) {
        k_print("[VFS] Failed to initialize /proc.\n");
    } else {
        k_print("[VFS] Mounted /proc.\n");
    }
    if (sysfs_init() != 0) {
        k_print("[VFS] Failed to initialize /sys.\n");
    } else {
        k_print("[VFS] Mounted /sys.\n");
    }
}