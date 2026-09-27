// vfs.h -- BSD-style Virtual File System interface
#ifndef VFS_H
#define VFS_H

#include "common.h"
#include <sys/types.h>

// Maximum path length (similar to BSD MAXPATHLEN)
#define VFS_MAXPATHLEN 1024
#define VFS_MAXNAMELEN 255

// Vnode types (similar to BSD)
typedef enum {
    VNON = 0,       // No type
    VREG = 1,       // Regular file
    VDIR = 2,       // Directory
    VCHR = 3,       // Character device
    VBLK = 4,       // Block device
    VLNK = 5,       // Symbolic link
    VFIFO = 6       // FIFO
} vtype_t;

// File flags (similar to BSD)
#define FREAD   0x0001
#define FWRITE  0x0002
#define FAPPEND 0x0008
#define FCREAT  0x0200
#define FTRUNC  0x0400
#define FEXCL   0x0800

// Seek constants (similar to POSIX)
#define VFS_SEEK_SET 0    // Seek from beginning of file
#define VFS_SEEK_CUR 1    // Seek from current position  
#define VFS_SEEK_END 2    // Seek from end of file

// Forward declarations
struct vnode;
struct mount;
struct vfs_context;

// VFS operations (similar to BSD vfsops)
typedef struct vfsops {
    int (*vfs_mount)(struct mount *mp, const char *device, void *data);
    int (*vfs_unmount)(struct mount *mp, int flags);
    int (*vfs_sync)(struct mount *mp, int waitfor);
    int (*vfs_statfs)(struct mount *mp, void *statfs);
} vfsops_t;

// Vnode operations (similar to BSD vnodeops)
typedef struct vnodeops {
    int (*vop_lookup)(struct vnode *dvp, struct vnode **vpp, const char *name);
    int (*vop_create)(struct vnode *dvp, struct vnode **vpp, const char *name, int mode);
    int (*vop_open)(struct vnode *vp, int mode, struct vfs_context *ctx);
    int (*vop_close)(struct vnode *vp, int flags, struct vfs_context *ctx);
    int (*vop_read)(struct vnode *vp, void *buffer, size_t *size, off_t offset, struct vfs_context *ctx);
    int (*vop_write)(struct vnode *vp, void *buffer, size_t *size, off_t offset, struct vfs_context *ctx);
    int (*vop_getattr)(struct vnode *vp, void *attr);
    int (*vop_setattr)(struct vnode *vp, void *attr);
    int (*vop_readdir)(struct vnode *vp, void *buffer, size_t *size, off_t *offset);
    int (*vop_mkdir)(struct vnode *dvp, struct vnode **vpp, const char *name, int mode);
    int (*vop_rmdir)(struct vnode *dvp, const char *name);
    int (*vop_remove)(struct vnode *dvp, const char *name);
} vnodeops_t;

// Mount point structure (similar to BSD mount)
typedef struct mount {
    struct vfsops *mnt_op;              // Filesystem operations
    struct vnode *mnt_vnodecovered;     // Vnode this mount covers
    struct vnode *mnt_rootvnode;        // Root vnode of mounted filesystem
    void *mnt_data;                     // Filesystem-specific data
    char mnt_path[VFS_MAXPATHLEN];      // Mount point path
    char mnt_device[VFS_MAXNAMELEN];    // Device name
    char mnt_fstype[32];                // Filesystem type name
    int mnt_flags;                      // Mount flags
    struct mount *mnt_next;             // Next mount in list
} mount_t;

// Vnode structure (similar to BSD vnode)
typedef struct vnode {
    vtype_t v_type;                     // Vnode type
    struct vnodeops *v_op;              // Vnode operations
    struct mount *v_mount;              // Mount point
    void *v_data;                       // Filesystem-specific data
    char v_name[VFS_MAXNAMELEN];        // File/directory name
    size_t v_size;                      // File size
    int v_usecount;                     // Reference count
    struct vnode *v_parent;             // Parent directory
} vnode_t;

// VFS context for operations
typedef struct vfs_context {
    int vc_uid;                         // User ID
    int vc_gid;                         // Group ID
    // Additional context fields as needed
} vfs_context_t;

// File descriptor structure
typedef struct file {
    struct vnode *f_vnode;              // Vnode for this file
    off_t f_offset;                     // Current file offset
    int f_flag;                         // File flags (FREAD, FWRITE, etc.)
    int f_count;                        // Reference count
} file_t;

// Per-process file descriptor table
#define VFS_MAXFD 256
typedef struct filedesc {
    struct file *fd_ofiles[VFS_MAXFD];  // Open files
    int fd_nfiles;                      // Number of open files
    char fd_cwd[VFS_MAXPATHLEN];        // Current working directory
} filedesc_t;

// VFS initialization and core functions
int vfs_init(void);
int vfs_mount(const char *device, const char *path, const char *fstype, int flags, void *data);
int vfs_unmount(const char *path, int flags);
mount_t *vfs_find_mount(const char *path);
vnode_t *vfs_lookup(const char *path);

// Vnode management
vnode_t *vnode_alloc(vtype_t type);
void vnode_free(vnode_t *vp);
int vnode_get(vnode_t *vp);
int vnode_put(vnode_t *vp);

// File operations
int vfs_open(const char *path, int flags, int mode);
int vfs_close(int fd);
ssize_t vfs_read(int fd, void *buffer, size_t size);
ssize_t vfs_write(int fd, const void *buffer, size_t size);
off_t vfs_lseek(int fd, off_t offset, int whence);
size_t vfs_fsize(int fd);
int vfs_unlink(const char *path);

// Directory operations
int vfs_mkdir(const char *path, int mode);
int vfs_rmdir(const char *path);
int vfs_opendir(const char *path);
int vfs_readdir(int fd, void *buffer, size_t *size);
int vfs_closedir(int fd);

// Path resolution
int vfs_namei(const char *path, vnode_t **result);
int vfs_lookup_parent(const char *path, vnode_t **parent, char *name);

// Filesystem registration
int vfs_register_filesystem(const char *name, vfsops_t *ops);
vfsops_t *vfs_find_filesystem(const char *name);

// Mount listing
void vfs_list_mounts(void);

// Global VFS state
extern mount_t *vfs_mount_list;
extern vnode_t *vfs_root_vnode;
extern filedesc_t *current_filedesc;

#endif // VFS_H