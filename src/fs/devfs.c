// devfs.c -- Device filesystem implementation (BSD-style)
// Provides /dev entries for block devices, character devices, etc.

#include "vfs.h"
#include "blockdev.h"
#include "common.h"
#include <stdint.h>
#include "kheap.h"

#ifndef SIZE_MAX
#define SIZE_MAX 0xFFFFFFFF
#endif

// Device filesystem data
typedef struct devfs_mount_data {
    // No persistent data needed for devfs
    int placeholder;
} devfs_mount_data_t;

// Device vnode data
typedef struct devfs_vnode_data {
    char name[VFS_MAXNAMELEN];      // Device name
    vtype_t dev_type;               // VBLK, VCHR, VREG
    int major;                      // Major device number
    int minor;                      // Minor device number
    size_t file_size;               // For special files like /dev/zero
    off_t position;                 // Current position for reading
} devfs_vnode_data_t;

// Device entries
typedef struct devfs_entry {
    char name[VFS_MAXNAMELEN];
    vtype_t type;
    int major;
    int minor;
    struct devfs_entry *next;
} devfs_entry_t;

// Static device list
static devfs_entry_t *device_list = NULL;

// Forward declarations
static int devfs_mount(mount_t *mp, const char *device, void *data);
static int devfs_unmount(mount_t *mp, int flags);
static int devfs_vop_lookup(vnode_t *dvp, vnode_t **vpp, const char *name);
static int devfs_vop_open(vnode_t *vp, int mode, vfs_context_t *ctx);
static int devfs_vop_close(vnode_t *vp, int flags, vfs_context_t *ctx);
static int devfs_vop_read(vnode_t *vp, void *buffer, size_t *size, off_t offset, vfs_context_t *ctx);
static int devfs_vop_write(vnode_t *vp, void *buffer, size_t *size, off_t offset, vfs_context_t *ctx);
static int devfs_vop_readdir(vnode_t *vp, void *buffer, size_t *size, off_t *offset);

// VFS operations table for devfs
vfsops_t devfs_vfsops = {
    .vfs_mount = devfs_mount,
    .vfs_unmount = devfs_unmount,
    .vfs_sync = NULL,
    .vfs_statfs = NULL
};

// Vnode operations table for devfs
static vnodeops_t devfs_vnodeops = {
    .vop_lookup = devfs_vop_lookup,
    .vop_create = NULL,
    .vop_open = devfs_vop_open,
    .vop_close = devfs_vop_close,
    .vop_read = devfs_vop_read,
    .vop_write = devfs_vop_write,
    .vop_getattr = NULL,
    .vop_setattr = NULL,
    .vop_readdir = devfs_vop_readdir,
    .vop_mkdir = NULL,
    .vop_rmdir = NULL,
    .vop_remove = NULL
};

// Add device entry to list
static void devfs_add_device(const char *name, vtype_t type, int major, int minor) {
    devfs_entry_t *entry = (devfs_entry_t*)kmalloc(sizeof(devfs_entry_t));
    if (!entry) {
        printf("devfs: Failed to allocate device entry\n");
        return;
    }
    
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->type = type;
    entry->major = major;
    entry->minor = minor;
    entry->next = device_list;
    device_list = entry;
    
    printf("devfs: Added device: %s (type=%d, major=%d, minor=%d)\n", 
           name, type, major, minor);
}

// Initialize device list
static void devfs_init_devices(void) {
    printf("devfs: Initializing device entries...\n");
    
    // Clear existing list
    device_list = NULL;
    
    // Add special devices
    devfs_add_device("null", VCHR, 2, 0);      // /dev/null
    devfs_add_device("zero", VCHR, 2, 1);      // /dev/zero
    devfs_add_device("console", VCHR, 3, 0);   // /dev/console
    devfs_add_device("tty", VCHR, 3, 1);       // /dev/tty
    
    // Add block devices from block device registry
    int num_devices = blockdev_count();
    for (int i = 0; i < num_devices; i++) {
        blockdev_t *bdev = blockdev_get(i);
        if (bdev) {
            devfs_add_device(bdev->name, VBLK, 1, bdev->minor);
        }
    }
    
    printf("devfs: Device initialization complete\n");
}

// Find device entry by name
static devfs_entry_t *devfs_find_device(const char *name) {
    for (devfs_entry_t *entry = device_list; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
    }
    return NULL;
}

// Mount devfs
static int devfs_mount(mount_t *mp, const char *device, void *data) {
    printf("devfs: Mounting device filesystem\n");
    
    // Initialize device list
    devfs_init_devices();
    
    // Allocate mount data
    devfs_mount_data_t *dmd = (devfs_mount_data_t*)kmalloc(sizeof(devfs_mount_data_t));
    if (!dmd) {
        printf("devfs: Failed to allocate mount data\n");
        return -1;
    }
    memset(dmd, 0, sizeof(devfs_mount_data_t));
    
    // Create root vnode for /dev
    vnode_t *root_vp = (vnode_t*)kmalloc(sizeof(vnode_t));
    if (!root_vp) {
        printf("devfs: Failed to allocate root vnode\n");
        kfree(dmd);
        return -1;
    }
    memset(root_vp, 0, sizeof(vnode_t));
    
    // Allocate vnode data for root
    devfs_vnode_data_t *vdata = (devfs_vnode_data_t*)kmalloc(sizeof(devfs_vnode_data_t));
    if (!vdata) {
        printf("devfs: Failed to allocate vnode data\n");
        kfree(root_vp);
        kfree(dmd);
        return -1;
    }
    memset(vdata, 0, sizeof(devfs_vnode_data_t));
    strcpy(vdata->name, "");
    vdata->dev_type = VDIR;
    
    // Initialize root vnode
    root_vp->v_type = VDIR;
    root_vp->v_op = &devfs_vnodeops;
    root_vp->v_mount = mp;
    root_vp->v_data = vdata;
    strcpy(root_vp->v_name, "dev");
    root_vp->v_size = 0;
    root_vp->v_usecount = 1;
    root_vp->v_parent = NULL;
    
    // Set mount data
    mp->mnt_data = dmd;
    mp->mnt_rootvnode = root_vp;
    
    printf("devfs: Mount successful\n");
    return 0;
}

// Unmount devfs
static int devfs_unmount(mount_t *mp, int flags) {
    if (!mp || !mp->mnt_data) {
        return -1;
    }
    
    printf("devfs: Unmounting device filesystem\n");
    
    // Free device list
    devfs_entry_t *entry = device_list;
    while (entry) {
        devfs_entry_t *next = entry->next;
        kfree(entry);
        entry = next;
    }
    device_list = NULL;
    
    // Free root vnode data
    if (mp->mnt_rootvnode && mp->mnt_rootvnode->v_data) {
        kfree(mp->mnt_rootvnode->v_data);
    }
    
    // Free root vnode
    if (mp->mnt_rootvnode) {
        kfree(mp->mnt_rootvnode);
    }
    
    // Free mount data
    kfree(mp->mnt_data);
    
    printf("devfs: Unmount successful\n");
    return 0;
}

// Lookup device by name
static int devfs_vop_lookup(vnode_t *dvp, vnode_t **vpp, const char *name) {
    if (!dvp || !vpp || !name || dvp->v_type != VDIR) {
        return -1;
    }
    
    // Find device entry
    devfs_entry_t *entry = devfs_find_device(name);
    if (!entry) {
        return -1;  // Device not found
    }
    
    // Create new vnode
    vnode_t *new_vp = (vnode_t*)kmalloc(sizeof(vnode_t));
    if (!new_vp) {
        return -1;
    }
    memset(new_vp, 0, sizeof(vnode_t));
    
    // Create vnode data
    devfs_vnode_data_t *vdata = (devfs_vnode_data_t*)kmalloc(sizeof(devfs_vnode_data_t));
    if (!vdata) {
        kfree(new_vp);
        return -1;
    }
    memset(vdata, 0, sizeof(devfs_vnode_data_t));
    strcpy(vdata->name, entry->name);
    vdata->dev_type = entry->type;
    vdata->major = entry->major;
    vdata->minor = entry->minor;
    vdata->file_size = (entry->type == VCHR && entry->minor == 1) ? SIZE_MAX : 0; // /dev/zero is infinite
    vdata->position = 0;
    
    // Initialize vnode
    new_vp->v_type = entry->type;
    new_vp->v_op = &devfs_vnodeops;
    new_vp->v_mount = dvp->v_mount;
    new_vp->v_data = vdata;
    strcpy(new_vp->v_name, name);
    new_vp->v_size = vdata->file_size;
    new_vp->v_usecount = 1;
    new_vp->v_parent = dvp;
    
    *vpp = new_vp;
    return 0;
}

// Open device
static int devfs_vop_open(vnode_t *vp, int mode, vfs_context_t *ctx) {
    serial_puts("devfs_vop_open: Called!\n");
    
    if (!vp || !vp->v_data) {
        serial_puts("devfs_vop_open: Invalid vnode or data\n");
        return -1;
    }
    
    devfs_vnode_data_t *vdata = (devfs_vnode_data_t*)vp->v_data;
    
    serial_puts("devfs: Opening device: ");
    serial_puts(vdata->name);
    serial_puts("\n");
    
    // For block devices, we might want to do additional setup
    if (vp->v_type == VBLK) {
        blockdev_t *bdev = blockdev_get(vdata->minor);
        if (!bdev) {
            printf("devfs: Block device not found: %d\n", vdata->minor);
            return -1;
        }
        printf("devfs: Opened block device: %s\n", bdev->name);
    }
    
    return 0;
}

// Close device
static int devfs_vop_close(vnode_t *vp, int flags, vfs_context_t *ctx) {
    if (!vp || !vp->v_data) {
        return -1;
    }
    
    devfs_vnode_data_t *vdata = (devfs_vnode_data_t*)vp->v_data;
    printf("devfs: Closing device: %s\n", vdata->name);
    
    return 0;
}

// Read from device
static int devfs_vop_read(vnode_t *vp, void *buffer, size_t *size, off_t offset, vfs_context_t *ctx) {
    if (!vp || !vp->v_data || !buffer || !size) {
        return -1;
    }
    
    devfs_vnode_data_t *vdata = (devfs_vnode_data_t*)vp->v_data;
    
    if (vp->v_type == VBLK) {
        // Block device read
        blockdev_t *bdev = blockdev_get(vdata->minor);
        if (!bdev) {
            return -1;
        }
        
        // Convert byte offset to sector offset
        uint64_t sector = offset / bdev->sector_size;
        uint32_t count = (*size + bdev->sector_size - 1) / bdev->sector_size;
        
        // Allocate sector-aligned buffer
        void *sector_buffer = (void*)kmalloc(count * bdev->sector_size);
        if (!sector_buffer) {
            return -1;
        }
        
        // Read sectors
        int result = blockdev_read(bdev, sector, count, sector_buffer);
        if (result != 0) {
            kfree(sector_buffer);
            return -1;
        }
        
        // Copy requested data
        size_t copy_size = *size;
        if (copy_size > count * bdev->sector_size) {
            copy_size = count * bdev->sector_size;
        }
        
        memcpy(buffer, (uint8_t*)sector_buffer + (offset % bdev->sector_size), copy_size);
        *size = copy_size;
        
        kfree(sector_buffer);
        return 0;
        
    } else if (vp->v_type == VCHR) {
        // Character device read
        if (vdata->major == 2) {  // Special devices
            if (vdata->minor == 0) {
                // /dev/null - read returns 0 bytes
                *size = 0;
                return 0;
            } else if (vdata->minor == 1) {
                // /dev/zero - read returns zeros
                memset(buffer, 0, *size);
                return 0;
            }
        } else if (vdata->major == 3) {  // Terminal devices
            // TODO: Implement terminal read
            printf("devfs: Terminal read not implemented\n");
            *size = 0;
            return 0;
        }
    }
    
    return -1;
}

// Write to device
static int devfs_vop_write(vnode_t *vp, void *buffer, size_t *size, off_t offset, vfs_context_t *ctx) {
    if (!vp || !vp->v_data || !buffer || !size) {
        return -1;
    }
    
    devfs_vnode_data_t *vdata = (devfs_vnode_data_t*)vp->v_data;
    
    if (vp->v_type == VBLK) {
        // Block device write
        blockdev_t *bdev = blockdev_get(vdata->minor);
        if (!bdev) {
            return -1;
        }
        
        // Convert byte offset to sector offset
        uint64_t sector = offset / bdev->sector_size;
        uint32_t count = (*size + bdev->sector_size - 1) / bdev->sector_size;
        
        // For simplicity, require sector-aligned writes for now
        if (offset % bdev->sector_size != 0) {
            printf("devfs: Unaligned block writes not supported\n");
            return -1;
        }
        
        // Write sectors
        int result = blockdev_write(bdev, sector, count, buffer);
        if (result != 0) {
            return -1;
        }
        
        return 0;
        
    } else if (vp->v_type == VCHR) {
        // Character device write
        if (vdata->major == 2) {  // Special devices
            if (vdata->minor == 0 || vdata->minor == 1) {
                // /dev/null and /dev/zero - write succeeds but discards data
                return 0;
            }
        } else if (vdata->major == 3) {  // Terminal devices
            // Write to console
            for (size_t i = 0; i < *size; i++) {
                putchar(((char*)buffer)[i]);
            }
            return 0;
        }
    }
    
    return -1;
}

// Read device directory
static int devfs_vop_readdir(vnode_t *vp, void *buffer, size_t *size, off_t *offset) {
    if (!vp || !vp->v_data || !buffer || !size || vp->v_type != VDIR) {
        serial_puts("devfs_readdir: Invalid parameters\n");
        return -1;
    }
    
    serial_puts("devfs_readdir: Called with offset=");
    char offset_buf[16];
    sprintf(offset_buf, "%d", (int)*offset);
    serial_puts(offset_buf);
    serial_puts("\n");
    
    // Walk the device list to find the entry at the current offset
    devfs_entry_t *entry = device_list;
    int current_index = 0;
    
    // Skip entries until we reach the desired offset
    while (entry && current_index < *offset) {
        entry = entry->next;
        current_index++;
    }
    
    // Check if we have more entries
    if (!entry) {
        serial_puts("devfs_readdir: No more entries\n");
        *size = 0;  // No more entries
        return 0;
    }
    
    serial_puts("devfs_readdir: Found device: ");
    serial_puts(entry->name);
    serial_puts("\n");
    
    // Return current entry name
    size_t name_len = strlen(entry->name);
    if (name_len >= *size) {
        serial_puts("devfs_readdir: Buffer too small\n");
        return -1;  // Buffer too small
    }
    
    strcpy((char*)buffer, entry->name);
    *size = name_len + 1;
    
    // Increment offset for next call
    (*offset)++;
    
    return 0;
}