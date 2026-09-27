// vfs.c -- BSD-style Virtual File System implementation
#include "vfs.h"
#include "blockdev.h"
#include "common.h"
#include "kheap.h"
#include <errno.h>

// Global VFS state
mount_t *vfs_mount_list = NULL;
vnode_t *vfs_root_vnode = NULL;
filedesc_t *current_filedesc = NULL;

// File descriptor table
static file_t *fd_table[VFS_MAXFD];
static int next_fd = 0;

// Registered filesystems
#define MAX_FILESYSTEMS 8
static struct {
    char name[32];
    vfsops_t *ops;
} filesystems[MAX_FILESYSTEMS];
static int num_filesystems = 0;

// Forward declarations
extern vfsops_t fat32_vfsops;  // Will be defined in fat_vfs.c
extern vfsops_t devfs_vfsops;  // Will be defined in devfs.c

// Initialize VFS subsystem
int vfs_init(void) {
    // Initialize file descriptor table
    memset(fd_table, 0, sizeof(fd_table));
    next_fd = 0;
    
    // Initialize mount list
    vfs_mount_list = NULL;
    vfs_root_vnode = NULL;
    
    // Initialize block device subsystem first
    blockdev_init();
    
    // Register built-in filesystems
    vfs_register_filesystem("fat32", &fat32_vfsops);
    vfs_register_filesystem("devfs", &devfs_vfsops);
    
    // Create default file descriptor table
    current_filedesc = (filedesc_t*)kmalloc(sizeof(filedesc_t));
    if (!current_filedesc) {
        return -1;
    }
    memset(current_filedesc, 0, sizeof(filedesc_t));
    
    // Don't mount root filesystem here - let os_mount() handle it
    // after the physical drive is properly initialized
    
    return 0;
}

// Register a filesystem
int vfs_register_filesystem(const char *name, vfsops_t *ops) {
    if (num_filesystems >= MAX_FILESYSTEMS) {
        printf("VFS: Too many filesystems registered\n");
        return -1;
    }
    
    if (!name || !ops) {
        printf("VFS: Invalid filesystem parameters\n");
        return -1;
    }
    
    strncpy(filesystems[num_filesystems].name, name, sizeof(filesystems[num_filesystems].name) - 1);
    filesystems[num_filesystems].name[sizeof(filesystems[num_filesystems].name) - 1] = '\0';
    filesystems[num_filesystems].ops = ops;
    
    printf("VFS: Registered filesystem: %s\n", name);
    num_filesystems++;
    return 0;
}

// Find filesystem by name
vfsops_t *vfs_find_filesystem(const char *name) {
    if (!name) return NULL;
    
    for (int i = 0; i < num_filesystems; i++) {
        if (strcmp(filesystems[i].name, name) == 0) {
            return filesystems[i].ops;
        }
    }
    return NULL;
}

// Mount a filesystem
int vfs_mount(const char *device, const char *path, const char *fstype, int flags, void *data) {
    if (!device || !path || !fstype) {
        printf("VFS: Invalid mount parameters\n");
        return -1;
    }
    
    printf("VFS: Attempting to mount %s at %s (type: %s)\n", device, path, fstype);
    
    // Find filesystem type
    vfsops_t *fsops = vfs_find_filesystem(fstype);
    if (!fsops) {
        serial_puts("VFS: Filesystem type not found: ");
        serial_puts(fstype);
        serial_puts("\n");
        printf("VFS: Filesystem type '%s' not found\n", fstype);
        return -1;
    }
    
    // Allocate mount structure
    mount_t *mp = (mount_t*)kmalloc(sizeof(mount_t));
    if (!mp) {
        return -1;
    }
    memset(mp, 0, sizeof(mount_t));
    
    // Initialize mount structure
    mp->mnt_op = fsops;
    strncpy(mp->mnt_path, path, sizeof(mp->mnt_path) - 1);
    strncpy(mp->mnt_device, device, sizeof(mp->mnt_device) - 1);
    strncpy(mp->mnt_fstype, fstype, sizeof(mp->mnt_fstype) - 1);
    mp->mnt_flags = flags;
    
    // Call filesystem mount function
    if (fsops->vfs_mount && fsops->vfs_mount(mp, device, data) != 0) {
        serial_puts("VFS: Mount function failed\n");
        kfree(mp);
        return -1;
    }
    
    // Add to mount list
    mp->mnt_next = vfs_mount_list;
    vfs_mount_list = mp;
    
    // Set as root if mounting at "/"
    if (strcmp(path, "/") == 0) {
        vfs_root_vnode = mp->mnt_rootvnode;
        printf("VFS: Root filesystem mounted successfully\n");
    }
    
    return 0;
}

// Unmount a filesystem
int vfs_unmount(const char *path, int flags) {
    if (!path) {
        return -1;
    }
    
    printf("VFS: Unmounting %s\n", path);
    
    // Find mount point
    mount_t *mp = vfs_find_mount(path);
    if (!mp) {
        printf("VFS: Mount point not found: %s\n", path);
        return -1;
    }
    
    // Call filesystem unmount function
    if (mp->mnt_op->vfs_unmount && mp->mnt_op->vfs_unmount(mp, flags) != 0) {
        printf("VFS: Filesystem unmount operation failed\n");
        return -1;
    }
    
    // Remove from mount list
    if (vfs_mount_list == mp) {
        vfs_mount_list = mp->mnt_next;
    } else {
        mount_t *prev = vfs_mount_list;
        while (prev && prev->mnt_next != mp) {
            prev = prev->mnt_next;
        }
        if (prev) {
            prev->mnt_next = mp->mnt_next;
        }
    }
    
    // Free mount structure
    kfree(mp);
    
    printf("VFS: Unmount successful: %s\n", path);
    return 0;
}

// Find mount point for path
mount_t *vfs_find_mount(const char *path) {
    if (!path) return NULL;
    
    mount_t *best_match = NULL;
    size_t best_len = 0;
    
    // Find the longest matching mount path
    for (mount_t *mp = vfs_mount_list; mp; mp = mp->mnt_next) {
        size_t mp_len = strlen(mp->mnt_path);
        
        // Check if path starts with mount path
        if (strncmp(path, mp->mnt_path, mp_len) == 0) {
            // Exact match or path continues with '/' or is at end
            if (path[mp_len] == '\0' || path[mp_len] == '/' || mp_len == 1) {
                if (mp_len > best_len) {
                    best_match = mp;
                    best_len = mp_len;
                }
            }
        }
    }
    
    return best_match;
}

// Allocate a file descriptor
static int vfs_alloc_fd(void) {
    for (int i = 0; i < VFS_MAXFD; i++) {
        if (fd_table[i] == NULL) {
            return i;
        }
    }
    return -1; // No free descriptors
}

// Open a file
int vfs_open(const char *path, int flags, int mode) {
    if (!path) {
        return -1;
    }
    
    // Debug output for file creation attempts
    extern void serial_puts(const char *msg);
    
    // Resolve relative paths to absolute paths
    char abs_path[VFS_MAXPATHLEN];
    if (path[0] != '/') {
        // Handle relative path prefixes
        const char *clean_path = path;
        
        // Strip leading "./" if present
        if (path[0] == '.' && path[1] == '/') {
            clean_path = path + 2;
        }
        
        // Relative path - resolve using current directory
        extern char g_current_working_directory[];
        if (g_current_working_directory[0]) {
            if (strcmp(g_current_working_directory, "/") == 0) {
                snprintf(abs_path, sizeof(abs_path), "/%s", clean_path);
            } else {
                snprintf(abs_path, sizeof(abs_path), "%s/%s", g_current_working_directory, clean_path);
            }
        } else {
            // Fallback to root if no current directory
            snprintf(abs_path, sizeof(abs_path), "/%s", clean_path);
        }
        path = abs_path;
    }
    
    // Allocate file descriptor
    int fd = vfs_alloc_fd();
    if (fd < 0) {
        printf("VFS: No free file descriptors\n");
        return -1;
    }
    
    // Resolve path to vnode
    vnode_t *vp = vfs_lookup(path);
    if (!vp && !(flags & FCREAT)) {
        // Special case: if trying to access root and it's not mounted yet, 
        // return a more specific error instead of generic "file not found"
        if (strcmp(path, "/") == 0 && !vfs_root_vnode) {
            return -EAGAIN;  // Try again later
        }
        return -1;
    }
    
    // TODO: Handle file creation if FCREAT is set
    if (!vp) {
        if (flags & FCREAT) {
            // Basic file creation - find parent directory and create file
            vnode_t *parent_vp;
            char name[VFS_MAXNAMELEN];
            
            if (vfs_lookup_parent(path, &parent_vp, name) != 0) {
                printf("VFS: Parent directory not found for: %s\n", path);
                return -1;
            }
            
            if (!parent_vp || !parent_vp->v_op->vop_create) {
                printf("VFS: File creation not supported by filesystem\n");
                return -1;
            }
            
            // Call vnode create operation
            int result = parent_vp->v_op->vop_create(parent_vp, &vp, name, mode);
            if (result != 0 || !vp) {
                printf("VFS: Failed to create file: %s\n", path);
                return -1;
            }
        } else {
            printf("VFS: File not found and FCREAT not set: %s\n", path);
            return -1;
        }
    }
    
    // Allocate file structure
    file_t *fp = (file_t*)kmalloc(sizeof(file_t));
    if (!fp) {
        printf("VFS: Failed to allocate file structure\n");
        return -1;
    }
    
    // Initialize file structure
    fp->f_vnode = vp;
    fp->f_offset = 0;
    fp->f_flag = flags;
    fp->f_count = 1;
    
    // Call vnode open operation
    if (vp->v_op->vop_open) {
        vfs_context_t ctx = {0}; // TODO: Fill in proper context
        if (vp->v_op->vop_open(vp, flags, &ctx) != 0) {
            printf("VFS: Failed to open file: %s\n", path);
            kfree(fp);
            return -1;
        }
    }
    
    // Store in file descriptor table
    fd_table[fd] = fp;
    
    // Also store in current process file descriptor table
    if (current_filedesc) {
        current_filedesc->fd_ofiles[fd] = fp;
    }
    
    return fd;
}

// Close a file
int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAXFD || !fd_table[fd]) {
        return -1;
    }
    
    file_t *fp = fd_table[fd];
    vnode_t *vp = fp->f_vnode;
    
    // Call vnode close operation
    if (vp->v_op->vop_close) {
        vfs_context_t ctx = {0}; // TODO: Fill in proper context
        vp->v_op->vop_close(vp, fp->f_flag, &ctx);
    }
    
    // Free the vnode itself, unless it's a persistent mount-root/root vnode
    // (those are owned by the mount table and reused across lookups).
    bool vp_is_persistent = (vp == vfs_root_vnode) ||
        (vp->v_mount && vp->v_mount->mnt_rootvnode == vp);
    if (!vp_is_persistent) {
        kfree(vp->v_data);
        kfree(vp);
    }
    
    // Free file structure
    kfree(fp);
    fd_table[fd] = NULL;
    
    // Also clear from current process file descriptor table
    if (current_filedesc) {
        current_filedesc->fd_ofiles[fd] = NULL;
    }
    
    return 0;
}

// Read from a file
ssize_t vfs_read(int fd, void *buffer, size_t size) {
    if (fd < 0 || fd >= VFS_MAXFD || !fd_table[fd] || !buffer) {
        return -1;
    }
    
    file_t *fp = fd_table[fd];
    vnode_t *vp = fp->f_vnode;
    
    if (!(fp->f_flag & FREAD)) {
        printf("VFS: File not open for reading\n");
        return -1;
    }
    
    if (!vp->v_op->vop_read) {
        printf("VFS: Read operation not supported\n");
        return -1;
    }
    
    // Debug: Log read operation
    if (size <= 10) { // Only log for small reads (like fgetc)
        char debug_msg[128];
        snprintf(debug_msg, sizeof(debug_msg), "VFS_READ: fd=%d, offset=%ld, size=%zu\n", fd, (long)fp->f_offset, size);
        serial_puts(debug_msg);
    }
    
    // Call vnode read operation
    vfs_context_t ctx = {0}; // TODO: Fill in proper context
    size_t bytes_read = size;
    int result = vp->v_op->vop_read(vp, buffer, &bytes_read, fp->f_offset, &ctx);
    
    if (result == 0) {
        // Debug: Log what was read for small reads
        if (size <= 10 && bytes_read > 0) {
            char debug_msg[128];
            unsigned char first_byte = ((unsigned char*)buffer)[0];
            snprintf(debug_msg, sizeof(debug_msg), "VFS_READ: read %zu bytes, first_byte=0x%02X ('%c'), new_offset=%ld\n", 
                     bytes_read, first_byte, (first_byte >= 32 && first_byte < 127) ? first_byte : '?', 
                     (long)(fp->f_offset + bytes_read));
            serial_puts(debug_msg);
        }
        
        fp->f_offset += bytes_read;
        return bytes_read;
    }

    return -1;
}// Write to a file
ssize_t vfs_write(int fd, const void *buffer, size_t size) {
    if (fd < 0 || fd >= VFS_MAXFD || !fd_table[fd] || !buffer) {
        return -1;
    }
    
    file_t *fp = fd_table[fd];
    vnode_t *vp = fp->f_vnode;
    
    if (!(fp->f_flag & FWRITE)) {
        printf("VFS: File not open for writing\n");
        return -1;
    }
    
    if (!vp->v_op->vop_write) {
        printf("VFS: Write operation not supported\n");
        return -1;
    }
    
    // Call vnode write operation
    vfs_context_t ctx = {0}; // TODO: Fill in proper context
    size_t bytes_written = size;
    int result = vp->v_op->vop_write(vp, (void*)buffer, &bytes_written, fp->f_offset, &ctx);
    
    if (result == 0) {
        fp->f_offset += bytes_written;
        return bytes_written;
    }

    return -1;
}

// Seek in a file
off_t vfs_lseek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= VFS_MAXFD || !fd_table[fd]) {
        return -1;
    }
    
    file_t *fp = fd_table[fd];
    vnode_t *vp = fp->f_vnode;
    
    if (!vp) {
        return -1;
    }
    
    off_t new_offset;
    switch (whence) {
        case VFS_SEEK_SET:
            new_offset = offset;
            break;
        case VFS_SEEK_CUR:
            new_offset = fp->f_offset + offset;
            break;
        case VFS_SEEK_END:
            new_offset = vp->v_size + offset;
            break;
        default:
            return -1; // Invalid whence
    }
    
    // Check bounds
    if (new_offset < 0) {
        return -1;
    }
    
    // Update file offset
    fp->f_offset = new_offset;
    return new_offset;
}

// Get file size
size_t vfs_fsize(int fd) {
    if (fd < 0 || fd >= VFS_MAXFD || !fd_table[fd]) {
        return 0;
    }
    
    file_t *fp = fd_table[fd];
    vnode_t *vp = fp->f_vnode;
    
    if (!vp) {
        return 0;
    }
    
    return vp->v_size;
}

// Unlink (delete) a file
int vfs_unlink(const char *path) {
    if (!path) {
        return -1;
    }
    
    // Find parent directory and get name
    vnode_t *parent_vp;
    char name[VFS_MAXNAMELEN];
    
    if (vfs_lookup_parent(path, &parent_vp, name) != 0) {
        serial_puts("VFS: Parent directory not found for: ");
        serial_puts(path);
        serial_puts("\n");
        return -1;
    }
    
    if (!parent_vp->v_op->vop_remove) {
        serial_puts("VFS: remove operation not supported\n");
        return -1;
    }
    
    // Call vnode remove operation
    int result = parent_vp->v_op->vop_remove(parent_vp, name);
    
    return result;
}

// Create directory
int vfs_mkdir(const char *path, int mode) {
    if (!path) {
        return -1;
    }
    
    // Find parent directory and get name
    vnode_t *parent_vp;
    char name[VFS_MAXNAMELEN];
    
    if (vfs_lookup_parent(path, &parent_vp, name) != 0) {
        printf("VFS: Parent directory not found for: %s\n", path);
        return -1;
    }
    
    if (!parent_vp->v_op->vop_mkdir) {
        printf("VFS: mkdir operation not supported\n");
        return -1;
    }
    
    // Call vnode mkdir operation
    vnode_t *new_vp;
    int result = parent_vp->v_op->vop_mkdir(parent_vp, &new_vp, name, mode);
    
    return result;
}

// Simple path lookup (stub implementation)
vnode_t *vfs_lookup(const char *path) {
    if (!path || !vfs_root_vnode) {
        return NULL;
    }
    
    // Resolve relative paths to absolute paths (same logic as vfs_open)
    char abs_path[VFS_MAXPATHLEN];
    if (path[0] != '/') {
        // Handle relative path prefixes
        const char *clean_path = path;
        
        // Strip leading "./" if present
        if (path[0] == '.' && path[1] == '/') {
            clean_path = path + 2;
        }
        
        // Relative path - resolve using current directory
        extern char g_current_working_directory[];
        if (g_current_working_directory[0]) {
            if (strcmp(g_current_working_directory, "/") == 0) {
                snprintf(abs_path, sizeof(abs_path), "/%s", clean_path);
            } else {
                snprintf(abs_path, sizeof(abs_path), "%s/%s", g_current_working_directory, clean_path);
            }
        } else {
            // Fallback to root if no current directory
            snprintf(abs_path, sizeof(abs_path), "/%s", clean_path);
        }
        path = abs_path;
    }
    
    // Handle root directory
    if (strcmp(path, "/") == 0) {
        return vfs_root_vnode;
    }
    
    // Start from root and walk the path
    vnode_t *current_vp = vfs_root_vnode;
    
    // Skip leading slash
    const char *p = path;
    if (p[0] == '/') {
        p++;
    }
    
    // Walk through path components using a safer approach
    while (p && *p) {
        // Find the end of current component
        const char *slash = strchr(p, '/');
        size_t component_len;
        
        if (slash) {
            component_len = slash - p;
        } else {
            component_len = strlen(p);
        }
        
        // Validate component length
        if (component_len == 0 || component_len >= VFS_MAXNAMELEN) {
            return NULL;
        }
        
        // Extract component safely into a separate buffer
        char component[VFS_MAXNAMELEN];
        memset(component, 0, sizeof(component));
        strncpy(component, p, component_len);
        component[component_len] = '\0';
        
        // Look up this component in current directory
        vnode_t *child_vp = NULL;
        if (!current_vp->v_op->vop_lookup) {
            return NULL;  // No lookup operation
        }
        
        int result = current_vp->v_op->vop_lookup(current_vp, &child_vp, component);
        if (result != 0 || !child_vp) {
            return NULL;  // Component not found
        }
        
        // Check if this path is a mount point
        char current_path[VFS_MAXPATHLEN];
        size_t path_len = p + component_len - path;
        if (path_len < sizeof(current_path)) {
            strncpy(current_path, path, path_len);
            current_path[path_len] = '\0';
            
            // Look for mount point
            for (mount_t *mp = vfs_mount_list; mp; mp = mp->mnt_next) {
                if (strcmp(mp->mnt_path, current_path) == 0) {
                    // child_vp was just allocated by vop_lookup above but we don't
                    // need it - the mount's persistent root vnode takes its place.
                    if (child_vp != mp->mnt_rootvnode) {
                        kfree(child_vp->v_data);
                        kfree(child_vp);
                    }
                    child_vp = mp->mnt_rootvnode;
                    break;
                }
            }
        }
        
        // Move to next component
        current_vp = child_vp;
        p = slash ? (slash + 1) : NULL;
    }
    
    return current_vp;
}

// Parse parent directory and filename from path
int vfs_lookup_parent(const char *path, vnode_t **parent, char *name) {
    if (!path || !name) {
        return -1;
    }
    
    // Extract filename from path
    const char *filename = strrchr(path, '/');
    if (!filename) {
        // No slash found - file is in current directory
        strncpy(name, path, VFS_MAXNAMELEN - 1);
        name[VFS_MAXNAMELEN - 1] = '\0';
        if (parent) {
            *parent = vfs_root_vnode; // Fallback to root
        }
        return 0;
    }
    
    // Copy filename
    filename++; // Skip the '/'
    
    // Clear name buffer first
    memset(name, 0, VFS_MAXNAMELEN);
    strncpy(name, filename, VFS_MAXNAMELEN - 1);
    name[VFS_MAXNAMELEN - 1] = '\0';
    
    // Find parent directory
    if (parent) {
        if (filename == path + 1) {
            // Parent is root directory (path like "/file.txt")
            *parent = vfs_root_vnode;
        } else {
            // Extract parent path
            char parent_path[VFS_MAXPATHLEN];
            memset(parent_path, 0, sizeof(parent_path));
            size_t parent_len = filename - path - 1; // Length without trailing slash
            strncpy(parent_path, path, parent_len);
            parent_path[parent_len] = '\0';
            
            // Look up parent directory
            *parent = vfs_lookup(parent_path);
            if (!*parent) {
                printf("VFS: Parent directory not found: %s\n", parent_path);
                return -1;
            }
        }
    }
    
    return 0;
}

// List mounted filesystems
void vfs_list_mounts(void) {
    printf("Mounted filesystems:\n");
    printf("%-16s %-12s %-8s %s\n", "Device", "Mount Point", "Type", "Flags");
    printf("----------------------------------------------------\n");
    
    for (mount_t *mp = vfs_mount_list; mp; mp = mp->mnt_next) {
        printf("%-16s %-12s %-8s 0x%04x\n", 
               mp->mnt_device, mp->mnt_path, mp->mnt_fstype, mp->mnt_flags);
    }
}

// Directory operations using file descriptors
int vfs_opendir(const char *path) {
    // First, lookup the vnode to check if it's a directory
    vnode_t *vp = vfs_lookup(path);
    if (!vp) {
        return -1;
    }
    
    // Check if it's actually a directory
    if (vp->v_type != VDIR) {
        return -1;  // Not a directory
    }
    
    return vfs_open(path, 0, 0);  // Use regular open for directories with 3 args
}

int vfs_readdir(int fd, void *buffer, size_t *size) {
    if (fd < 0 || fd >= VFS_MAXFD || !current_filedesc->fd_ofiles[fd]) {
        return -EBADF;
    }
    
    file_t *fp = current_filedesc->fd_ofiles[fd];
    if (!fp->f_vnode || !fp->f_vnode->v_op->vop_readdir) {
        return -ENOTDIR;
    }
    
    // Call vnode readdir operation
    size_t actual_size = *size;
    off_t offset = fp->f_offset;
    int result = fp->f_vnode->v_op->vop_readdir(fp->f_vnode, buffer, &actual_size, &offset);
    if (result == 0) {
        fp->f_offset = offset;
        *size = actual_size;  // Return actual bytes read
        int return_value = (actual_size == 0) ? -1 : 0;  // Return -1 for end of directory
        return return_value;
    }
    return result;
}

int vfs_closedir(int fd) {
    return vfs_close(fd);  // Use regular close for directories
}

int vfs_rmdir(const char *path) {
    serial_puts("VFS: rmdir called for path: ");
    serial_puts(path);
    serial_puts("\n");
    
    char parent_path[VFS_MAXPATHLEN];
    char name[VFS_MAXNAMELEN];
    
    // Parse parent directory and filename
    int result = vfs_lookup_parent(path, NULL, name);
    if (result < 0) {
        serial_puts("VFS: Failed to parse parent path for: ");
        serial_puts(path);
        serial_puts("\n");
        return result;
    }
    
    serial_puts("VFS: Parsed name: ");
    serial_puts(name);
    serial_puts("\n");
    
    // Get parent directory
    vnode_t *parent_vp;
    result = vfs_lookup_parent(path, &parent_vp, name);
    if (result < 0) {
        serial_puts("VFS: Failed to lookup parent for: ");
        serial_puts(path);
        serial_puts("\n");
        return result;
    }
    
    if (!parent_vp->v_op->vop_rmdir) {
        serial_puts("VFS: rmdir operation not supported\n");
        return -ENOTSUP;
    }
    
    serial_puts("VFS: Calling vop_rmdir for name: ");
    serial_puts(name);
    serial_puts("\n");
    
    // Call vnode rmdir operation
    return parent_vp->v_op->vop_rmdir(parent_vp, name);
}