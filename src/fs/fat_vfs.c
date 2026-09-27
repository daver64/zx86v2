// fat_vfs.c -- FAT32 filesystem VFS adapter
// Wraps existing FatFs functionality with BSD-style VFS interface

#include "vfs.h"
#include "blockdev.h"
#include "ff.h"     // FatFs headers
#include "diskio.h"
#include "kheap.h"
#include <stdbool.h>

// External reference to the traditional FatFs mount from dfs.c
extern FATFS *rootfs;

// FAT32 filesystem data
typedef struct fat_mount_data {
    FATFS fatfs;                    // FatFs filesystem object
    blockdev_t *blockdev;           // Associated block device
    char device_name[32];           // Device name (e.g., "disk0")
    int drive_number;               // FatFs drive number (0:, 1:, etc.)
} fat_mount_data_t;

// FAT32 vnode data
typedef struct fat_vnode_data {
    char path[VFS_MAXPATHLEN];      // Full path in filesystem
    FIL file;                       // FatFs file object (for files)
    DIR dir;                        // FatFs directory object (for directories)
    FILINFO finfo;                  // File information
    uint8_t is_open;                // Whether file/dir is open
} fat_vnode_data_t;

// Forward declarations
static int fat_mount(mount_t *mp, const char *device, void *data);
static int fat_unmount(mount_t *mp, int flags);
static int fat_vop_lookup(vnode_t *dvp, vnode_t **vpp, const char *name);
static int fat_vop_create(vnode_t *dvp, vnode_t **vpp, const char *name, int mode);
static int fat_vop_open(vnode_t *vp, int mode, vfs_context_t *ctx);
static int fat_vop_close(vnode_t *vp, int flags, vfs_context_t *ctx);
static int fat_vop_read(vnode_t *vp, void *buffer, size_t *size, off_t offset, vfs_context_t *ctx);
static int fat_vop_write(vnode_t *vp, void *buffer, size_t *size, off_t offset, vfs_context_t *ctx);
static int fat_vop_readdir(vnode_t *vp, void *buffer, size_t *size, off_t *offset);
static int fat_vop_mkdir(vnode_t *dvp, vnode_t **vpp, const char *name, int mode);
static int fat_vop_remove(vnode_t *dvp, const char *name);
static int fat_vop_rmdir(vnode_t *dvp, const char *name);

// VFS operations table for FAT32
vfsops_t fat32_vfsops = {
    .vfs_mount = fat_mount,
    .vfs_unmount = fat_unmount,
    .vfs_sync = NULL,       // TODO: Implement sync
    .vfs_statfs = NULL      // TODO: Implement statfs
};

// Vnode operations table for FAT32
static vnodeops_t fat_vnodeops = {
    .vop_lookup = fat_vop_lookup,
    .vop_create = fat_vop_create,
    .vop_open = fat_vop_open,
    .vop_close = fat_vop_close,
    .vop_read = fat_vop_read,
    .vop_write = fat_vop_write,
    .vop_getattr = NULL,    // TODO: Implement getattr
    .vop_setattr = NULL,    // TODO: Implement setattr
    .vop_readdir = fat_vop_readdir,
    .vop_mkdir = fat_vop_mkdir,
    .vop_rmdir = fat_vop_rmdir,
    .vop_remove = fat_vop_remove
};

// Helper function to convert FatFs error to VFS error
static int fatfs_error_to_vfs(FRESULT fr) {
    switch (fr) {
        case FR_OK: return 0;
        case FR_NO_FILE: return -1;        // File not found
        case FR_NO_PATH: return -1;        // Path not found
        case FR_INVALID_NAME: return -1;   // Invalid name
        case FR_DENIED: return -1;         // Access denied
        case FR_EXIST: return -1;          // File exists
        case FR_INVALID_OBJECT: return -1; // Invalid object
        case FR_WRITE_PROTECTED: return -1; // Write protected
        case FR_INVALID_DRIVE: return -1;  // Invalid drive
        case FR_NOT_ENABLED: return -1;    // Volume not enabled
        case FR_NO_FILESYSTEM: return -1;  // No filesystem
        case FR_TIMEOUT: return -1;        // Timeout
        case FR_LOCKED: return -1;         // File locked
        case FR_NOT_ENOUGH_CORE: return -1; // Not enough memory
        case FR_TOO_MANY_OPEN_FILES: return -1; // Too many open files
        case FR_INVALID_PARAMETER: return -1;   // Invalid parameter
        default: return -1;
    }
}

// Mount FAT32 filesystem
static int fat_mount(mount_t *mp, const char *device, void *data) {
    printf("FAT32: Mounting %s\n", device);
    
    // Find block device
    blockdev_t *bdev = blockdev_find(device);
    if (!bdev) {
        printf("FAT32: Block device not found: %s\n", device);
        return -1;
    }
    
    // Allocate mount data
    fat_mount_data_t *fmd = (fat_mount_data_t*)kmalloc(sizeof(fat_mount_data_t));
    if (!fmd) {
        printf("FAT32: Failed to allocate mount data\n");
        return -1;
    }
    memset(fmd, 0, sizeof(fat_mount_data_t));
    
    fmd->blockdev = bdev;
    strncpy(fmd->device_name, device, sizeof(fmd->device_name) - 1);
    
    // Mount using FatFs
    // For hd0 (primary hard drive), reuse existing FatFs mount from drive 0:
    // since traditional mount already set it up
    int drive_num;
    char drive_path[8];
    bool need_unmount = false;  // Track if we need to unmount on failure
    
    if (strcmp(device, "hd0") == 0) {
        drive_num = 0;  // Use existing drive 0: mount
        printf("FAT32: Reusing existing FatFs mount for drive 0:\n");
        // Don't call f_mount again, the drive is already mounted
        // Just store the drive number for path operations
        fmd->drive_number = drive_num;
        sprintf(drive_path, "%d:", drive_num);
    } else {
        drive_num = bdev->minor;  // Use minor number as drive for other devices
        sprintf(drive_path, "%d:", drive_num);
        
        FRESULT fr = f_mount(&fmd->fatfs, drive_path, 1);
        if (fr != FR_OK) {
            printf("FAT32: FatFs mount failed: %d\n", fr);
            kfree(fmd);
            return -1;
        }
        fmd->drive_number = drive_num;
        need_unmount = true;  // We mounted it, so we should unmount on failure
    }
    
    // Create root vnode
    vnode_t *root_vp = (vnode_t*)kmalloc(sizeof(vnode_t));
    if (!root_vp) {
        printf("FAT32: Failed to allocate root vnode\n");
        if (need_unmount) {
            f_mount(NULL, drive_path, 0);  // Unmount only if we mounted it
        }
        kfree(fmd);
        return -1;
    }
    memset(root_vp, 0, sizeof(vnode_t));
    
    // Allocate vnode data for root
    fat_vnode_data_t *vdata = (fat_vnode_data_t*)kmalloc(sizeof(fat_vnode_data_t));
    if (!vdata) {
        printf("FAT32: Failed to allocate vnode data\n");
        kfree(root_vp);
        if (need_unmount) {
            f_mount(NULL, drive_path, 0);
        }
        kfree(fmd);
        return -1;
    }
    memset(vdata, 0, sizeof(fat_vnode_data_t));
    strcpy(vdata->path, "/");
    
    // Initialize root vnode
    root_vp->v_type = VDIR;
    root_vp->v_op = &fat_vnodeops;
    root_vp->v_mount = mp;
    root_vp->v_data = vdata;
    strcpy(root_vp->v_name, "/");
    root_vp->v_size = 0;
    root_vp->v_usecount = 1;
    root_vp->v_parent = NULL;
    
    // Set mount data
    mp->mnt_data = fmd;
    mp->mnt_rootvnode = root_vp;
    
    printf("FAT32: Mount successful for %s\n", device);
    return 0;
}

// Unmount FAT32 filesystem
static int fat_unmount(mount_t *mp, int flags) {
    if (!mp || !mp->mnt_data) {
        return -1;
    }
    
    fat_mount_data_t *fmd = (fat_mount_data_t*)mp->mnt_data;
    
    printf("FAT32: Unmounting %s\n", fmd->device_name);
    
    // Only unmount from FatFs if it's not drive 0: (which is the traditional mount)
    if (fmd->drive_number != 0) {
        char drive_path[8];
        sprintf(drive_path, "%d:", fmd->drive_number);
        f_mount(NULL, drive_path, 0);
    } else {
        printf("FAT32: Keeping traditional FatFs mount for drive 0:\n");
    }
    
    // Free root vnode data
    if (mp->mnt_rootvnode && mp->mnt_rootvnode->v_data) {
        kfree(mp->mnt_rootvnode->v_data);
    }
    
    // Free root vnode
    if (mp->mnt_rootvnode) {
        kfree(mp->mnt_rootvnode);
    }
    
    // Free mount data
    kfree(fmd);
    
    printf("FAT32: Unmount successful\n");
    return 0;
}

// Lookup a file/directory in FAT32
static int fat_vop_lookup(vnode_t *dvp, vnode_t **vpp, const char *name) {
    if (!dvp || !vpp || !name || dvp->v_type != VDIR) {
        return -1;
    }
    
    // Validate name length and content to detect corruption
    size_t name_len = strlen(name);
    if (name_len == 0 || name_len >= VFS_MAXNAMELEN) {
        return -1;
    }
    
    // Check for any non-printable characters that might indicate corruption
    for (size_t i = 0; i < name_len; i++) {
        if (name[i] < 0x20 || name[i] > 0x7E) {
            return -1;
        }
    }
    
    fat_vnode_data_t *parent_data = (fat_vnode_data_t*)dvp->v_data;
    if (!parent_data) {
        return -1;
    }
    
    // Construct full path
    char full_path[VFS_MAXPATHLEN];
    memset(full_path, 0, sizeof(full_path));
    if (strcmp(parent_data->path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "/%s", name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", parent_data->path, name);
    }
    
    // Convert to FatFs path format
    fat_mount_data_t *fmd = (fat_mount_data_t*)dvp->v_mount->mnt_data;
    char fatfs_path[VFS_MAXPATHLEN];
    memset(fatfs_path, 0, sizeof(fatfs_path));
    
    // Handle root directory specially for FatFs
    if (strcmp(full_path, "/") == 0) {
        snprintf(fatfs_path, sizeof(fatfs_path), "%d:", fmd->drive_number);  // Root is just "0:"
    } else {
        snprintf(fatfs_path, sizeof(fatfs_path), "%d:%s", fmd->drive_number, full_path);
    }
    
    // Check if file/directory exists
    FILINFO finfo;
    FRESULT fr = f_stat(fatfs_path, &finfo);
    if (fr != FR_OK) {
        return -1;  // File not found
    }
    
    // Create new vnode
    vnode_t *new_vp = (vnode_t*)kmalloc(sizeof(vnode_t));
    if (!new_vp) {
        return -1;
    }
    memset(new_vp, 0, sizeof(vnode_t));
    
    // Create vnode data
    fat_vnode_data_t *vdata = (fat_vnode_data_t*)kmalloc(sizeof(fat_vnode_data_t));
    if (!vdata) {
        kfree(new_vp);
        return -1;
    }
    memset(vdata, 0, sizeof(fat_vnode_data_t));
    strcpy(vdata->path, full_path);
    vdata->finfo = finfo;
    
    // Initialize vnode
    new_vp->v_type = (finfo.fattrib & AM_DIR) ? VDIR : VREG;
    new_vp->v_op = &fat_vnodeops;
    new_vp->v_mount = dvp->v_mount;
    new_vp->v_data = vdata;
    strcpy(new_vp->v_name, name);
    new_vp->v_size = finfo.fsize;
    new_vp->v_usecount = 1;
    new_vp->v_parent = dvp;
    
    *vpp = new_vp;
    return 0;
}

// Open a file/directory
static int fat_vop_open(vnode_t *vp, int mode, vfs_context_t *ctx) {
    if (!vp || !vp->v_data) {
        return -1;
    }
    
    fat_vnode_data_t *vdata = (fat_vnode_data_t*)vp->v_data;
    fat_mount_data_t *fmd = (fat_mount_data_t*)vp->v_mount->mnt_data;
    
    if (vdata->is_open) {
        return 0;  // Already open
    }
    
    // Convert to FatFs path
    char fatfs_path[VFS_MAXPATHLEN];
    
    // Handle root directory specially for FatFs
    if (strcmp(vdata->path, "/") == 0) {
        snprintf(fatfs_path, sizeof(fatfs_path), "%d:", fmd->drive_number);  // Root is just "0:"
    } else {
        snprintf(fatfs_path, sizeof(fatfs_path), "%d:%s", fmd->drive_number, vdata->path);
    }
    
    FRESULT fr;
    
    if (vp->v_type == VDIR) {
        // Open directory
        fr = f_opendir(&vdata->dir, fatfs_path);
    } else {
        // Open file
        BYTE fatfs_mode = 0;
        if (mode & FREAD) fatfs_mode |= FA_READ;
        if (mode & FWRITE) fatfs_mode |= FA_WRITE;
        
        // Handle creation and truncation flags
        if (mode & FTRUNC) {
            fatfs_mode |= FA_CREATE_ALWAYS;  // Truncate means create new or overwrite existing
        } else if (mode & FCREAT) {
            fatfs_mode |= FA_OPEN_ALWAYS;    // Create if doesn't exist, open if exists
        } else {
            fatfs_mode |= FA_OPEN_EXISTING;  // Must exist
        }
        
        fr = f_open(&vdata->file, fatfs_path, fatfs_mode);
    }
    
    if (fr != FR_OK) {
        return fatfs_error_to_vfs(fr);
    }
    
    vdata->is_open = 1;
    return 0;
}

// Close a file/directory
static int fat_vop_close(vnode_t *vp, int flags, vfs_context_t *ctx) {
    if (!vp || !vp->v_data) {
        return -1;
    }
    
    fat_vnode_data_t *vdata = (fat_vnode_data_t*)vp->v_data;
    
    if (!vdata->is_open) {
        return 0;  // Already closed
    }
    
    FRESULT fr;
    
    if (vp->v_type == VDIR) {
        // Close directory
        fr = f_closedir(&vdata->dir);
    } else {
        // Close file
        fr = f_close(&vdata->file);
    }
    
    vdata->is_open = 0;
    return fatfs_error_to_vfs(fr);
}

// Read from a file
static int fat_vop_read(vnode_t *vp, void *buffer, size_t *size, off_t offset, vfs_context_t *ctx) {
    if (!vp || !vp->v_data || !buffer || !size || vp->v_type != VREG) {
        return -1;
    }
    
    fat_vnode_data_t *vdata = (fat_vnode_data_t*)vp->v_data;
    
    if (!vdata->is_open) {
        printf("FAT32: File not open for reading\n");
        return -1;
    }
    
    // Debug: Log FAT32 read operations for small reads
    if (*size <= 10) {
        char debug_msg[128];
        snprintf(debug_msg, sizeof(debug_msg), "FAT32_READ: offset=%ld, size=%zu, file_pos=%lu\n", 
                 (long)offset, *size, (unsigned long)vdata->file.fptr);
        serial_puts(debug_msg);
    }
    
    // Seek to offset
    FRESULT fr = f_lseek(&vdata->file, offset);
    if (fr != FR_OK) {
        char debug_msg[64];
        snprintf(debug_msg, sizeof(debug_msg), "FAT32_READ: f_lseek failed, fr=%d\n", fr);
        serial_puts(debug_msg);
        return fatfs_error_to_vfs(fr);
    }
    
    // Debug: Log position after seek for small reads
    if (*size <= 10) {
        char debug_msg[128];
        snprintf(debug_msg, sizeof(debug_msg), "FAT32_READ: after f_lseek, file_pos=%lu\n", 
                 (unsigned long)vdata->file.fptr);
        serial_puts(debug_msg);
    }
    
    // Read data
    UINT bytes_read;
    fr = f_read(&vdata->file, buffer, *size, &bytes_read);
    if (fr != FR_OK) {
        char debug_msg[64];
        snprintf(debug_msg, sizeof(debug_msg), "FAT32_READ: f_read failed, fr=%d\n", fr);
        serial_puts(debug_msg);
        return fatfs_error_to_vfs(fr);
    }
    
    // Debug: Log what was read for small reads
    if (*size <= 10 && bytes_read > 0) {
        char debug_msg[128];
        unsigned char first_byte = ((unsigned char*)buffer)[0];
        snprintf(debug_msg, sizeof(debug_msg), "FAT32_READ: read %u bytes, first_byte=0x%02X ('%c'), final_pos=%lu\n", 
                 bytes_read, first_byte, (first_byte >= 32 && first_byte < 127) ? first_byte : '?',
                 (unsigned long)vdata->file.fptr);
        serial_puts(debug_msg);
    }
    
    *size = bytes_read;
    return 0;
}

// Write to a file
static int fat_vop_write(vnode_t *vp, void *buffer, size_t *size, off_t offset, vfs_context_t *ctx) {
    if (!vp || !vp->v_data || !buffer || !size || vp->v_type != VREG) {
        return -1;
    }
    
    fat_vnode_data_t *vdata = (fat_vnode_data_t*)vp->v_data;
    
    if (!vdata->is_open) {
        printf("FAT32: File not open for writing\n");
        return -1;
    }
    
    // Seek to offset
    FRESULT fr = f_lseek(&vdata->file, offset);
    if (fr != FR_OK) {
        return fatfs_error_to_vfs(fr);
    }
    
    // Write data
    UINT bytes_written;
    fr = f_write(&vdata->file, buffer, *size, &bytes_written);
    if (fr != FR_OK) {
        return fatfs_error_to_vfs(fr);
    }
    
    *size = bytes_written;
    return 0;
}

// Read directory entries
static int fat_vop_readdir(vnode_t *vp, void *buffer, size_t *size, off_t *offset) {
    if (!vp || !vp->v_data || !buffer || !size || vp->v_type != VDIR) {
        return -1;
    }
    
    fat_vnode_data_t *vdata = (fat_vnode_data_t*)vp->v_data;
    
    if (!vdata->is_open) {
        return -1;
    }
    
    // Read next directory entry
    FILINFO finfo;
    FRESULT fr = f_readdir(&vdata->dir, &finfo);
    if (fr != FR_OK) {
        return fatfs_error_to_vfs(fr);
    }
    
    // Check if end of directory
    if (finfo.fname[0] == 0) {
        *size = 0;  // No more entries
        return 0;
    }
    
    // Simple directory entry format (just filename for now)
    size_t name_len = strlen(finfo.fname);
    if (name_len >= *size) {
        return -1;  // Buffer too small
    }
    
    strcpy((char*)buffer, finfo.fname);
    *size = name_len + 1;
    
    return 0;
}

// Create file
static int fat_vop_create(vnode_t *dvp, vnode_t **vpp, const char *name, int mode) {
    if (!dvp || !vpp || !name || dvp->v_type != VDIR) {
        return -1;
    }
    
    extern void serial_puts(const char *msg);
    serial_puts("FAT_VOP_CREATE: Creating file: ");
    serial_puts(name);
    serial_puts("\r\n");
    
    fat_vnode_data_t *parent_data = (fat_vnode_data_t*)dvp->v_data;
    fat_mount_data_t *fmd = (fat_mount_data_t*)dvp->v_mount->mnt_data;
    
    serial_puts("FAT_VOP_CREATE: Parent path: ");
    serial_puts(parent_data->path);
    serial_puts("\r\n");
    
    // Construct full path
    char full_path[VFS_MAXPATHLEN];
    memset(full_path, 0, sizeof(full_path));  // Clear buffer
    if (strcmp(parent_data->path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "/%s", name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", parent_data->path, name);
    }
    
    serial_puts("FAT_VOP_CREATE: Full VFS path: ");
    serial_puts(full_path);
    serial_puts("\r\n");
    
    // Convert to FatFs path (remove leading slash for FatFs)
    char fatfs_path[VFS_MAXPATHLEN];
    memset(fatfs_path, 0, sizeof(fatfs_path));  // Clear buffer
    if (full_path[0] == '/') {
        strcpy(fatfs_path, full_path + 1);
    } else {
        strcpy(fatfs_path, full_path);
    }
    
    serial_puts("FAT_VOP_CREATE: FatFS path: ");
    serial_puts(fatfs_path);
    serial_puts("\r\n");
    
    // Create the file using FatFs (overwrite if exists)
    FIL temp_file;
    FRESULT fr = f_open(&temp_file, fatfs_path, FA_CREATE_ALWAYS | FA_WRITE);
    
    serial_puts("FAT_VOP_CREATE: f_open result: ");
    // Simple error code display
    if (fr == FR_OK) serial_puts("FR_OK");
    else if (fr == FR_EXIST) serial_puts("FR_EXIST (file already exists)");
    else if (fr == FR_NO_PATH) serial_puts("FR_NO_PATH (path not found)");
    else if (fr == FR_INVALID_NAME) serial_puts("FR_INVALID_NAME");
    else if (fr == FR_DENIED) serial_puts("FR_DENIED (access denied)");
    else if (fr == FR_NOT_READY) serial_puts("FR_NOT_READY");
    else if (fr == FR_WRITE_PROTECTED) serial_puts("FR_WRITE_PROTECTED");
    else {
        char error_str[16];
        sprintf(error_str, "ERROR_%d", (int)fr);
        serial_puts(error_str);
    }
    serial_puts("\r\n");
    
    if (fr != FR_OK) {
        printf("FAT: Failed to create file %s (error %d)\n", fatfs_path, fr);
        return fatfs_error_to_vfs(fr);
    }
    
    // Close the file immediately - we just needed to create it
    f_close(&temp_file);
    
    // Now create a vnode for the newly created file
    vnode_t *vp = (vnode_t*)kmalloc(sizeof(vnode_t));
    if (!vp) {
        return -1;
    }
    memset(vp, 0, sizeof(vnode_t));
    
    // Create vnode data
    fat_vnode_data_t *vdata = (fat_vnode_data_t*)kmalloc(sizeof(fat_vnode_data_t));
    if (!vdata) {
        kfree(vp);
        return -1;
    }
    memset(vdata, 0, sizeof(fat_vnode_data_t));
    
    // Initialize vnode
    vp->v_mount = dvp->v_mount;
    vp->v_op = &fat_vnodeops;
    vp->v_data = vdata;
    vp->v_type = VREG;  // Regular file
    
    // Initialize vnode data
    strcpy(vdata->path, full_path);
    
    // Get file info to set size
    FILINFO finfo;
    fr = f_stat(fatfs_path, &finfo);
    if (fr == FR_OK) {
        vp->v_size = finfo.fsize;
    } else {
        vp->v_size = 0;
    }
    
    *vpp = vp;
    return 0;
}

// Create directory
static int fat_vop_mkdir(vnode_t *dvp, vnode_t **vpp, const char *name, int mode) {
    if (!dvp || !vpp || !name || dvp->v_type != VDIR) {
        return -1;
    }
    
    fat_vnode_data_t *parent_data = (fat_vnode_data_t*)dvp->v_data;
    fat_mount_data_t *fmd = (fat_mount_data_t*)dvp->v_mount->mnt_data;
    
    // Construct full path
    char full_path[VFS_MAXPATHLEN];
    if (strcmp(parent_data->path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "/%s", name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", parent_data->path, name);
    }
    
    // Convert to FatFs path
    char fatfs_path[VFS_MAXPATHLEN];
    
    // Handle root directory specially for FatFs
    if (strcmp(full_path, "/") == 0) {
        snprintf(fatfs_path, sizeof(fatfs_path), "%d:", fmd->drive_number);  // Root is just "0:"
    } else {
        snprintf(fatfs_path, sizeof(fatfs_path), "%d:%s", fmd->drive_number, full_path);
    }
    
    // Create directory
    FRESULT fr = f_mkdir(fatfs_path);
    if (fr != FR_OK) {
        printf("FAT32: mkdir failed for %s: %d\n", fatfs_path, fr);
        return fatfs_error_to_vfs(fr);
    }
    
    // Create vnode for new directory (optional)
    // For now, just return success
    *vpp = NULL;
    
    return 0;
}

// Remove file
static int fat_vop_remove(vnode_t *dvp, const char *name) {
    if (!dvp || !name || dvp->v_type != VDIR) {
        serial_puts("FAT: remove invalid parameters\n");
        return -1;
    }
    
    fat_vnode_data_t *parent_data = (fat_vnode_data_t*)dvp->v_data;
    fat_mount_data_t *fmd = (fat_mount_data_t*)dvp->v_mount->mnt_data;
    
    // Construct full path
    char full_path[VFS_MAXPATHLEN];
    memset(full_path, 0, sizeof(full_path));
    if (strcmp(parent_data->path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "/%s", name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", parent_data->path, name);
    }
    
    // Convert to FatFs path - use smaller buffer and be more defensive
    char fatfs_path[128];  // Smaller buffer
    memset(fatfs_path, 0, sizeof(fatfs_path));
    
    // For FatFS, we need to handle root files specially
    if (strcmp(full_path, "/") == 0) {
        strncpy(fatfs_path, "0:", sizeof(fatfs_path) - 1);
    } else {
        // Strip leading slash for files in root, but keep for subdirectories
        const char *fatfs_relative_path = (full_path[0] == '/' && full_path[1] != '\0') ? full_path + 1 : full_path;
        
        // Build path very carefully with manual construction
        // First add drive number - we know it should be 0
        fatfs_path[0] = '0';
        fatfs_path[1] = ':';
        fatfs_path[2] = '\0';
        
        // Now append the relative path if there's room
        int drive_len = 2;  // Length of "0:"
        int remaining = sizeof(fatfs_path) - drive_len - 1;  // Space for null terminator
        int rel_len = strlen(fatfs_relative_path);
        
        if (rel_len < remaining) {
            strncpy(fatfs_path + drive_len, fatfs_relative_path, rel_len);
            fatfs_path[drive_len + rel_len] = '\0';  // Explicit null termination
        } else {
            strncpy(fatfs_path + drive_len, fatfs_relative_path, remaining - 1);
            fatfs_path[sizeof(fatfs_path) - 1] = '\0';
        }
    }
    // Ensure null termination multiple times
    fatfs_path[sizeof(fatfs_path) - 1] = '\0';
    fatfs_path[127] = '\0';  // Extra safety
    
    // Remove the file using FatFs
    FRESULT fr = f_unlink(fatfs_path);
    if (fr != FR_OK) {
        return fatfs_error_to_vfs(fr);
    }
    
    return 0;
}

static int fat_vop_rmdir(vnode_t *dvp, const char *name) {
    if (!dvp || !name || dvp->v_type != VDIR) {
        serial_puts("FAT: rmdir invalid parameters\n");
        return -1;
    }
    
    fat_vnode_data_t *parent_data = (fat_vnode_data_t*)dvp->v_data;
    fat_mount_data_t *fmd = (fat_mount_data_t*)dvp->v_mount->mnt_data;
    
    // Construct full path
    char full_path[VFS_MAXPATHLEN];
    if (strcmp(parent_data->path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "/%s", name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", parent_data->path, name);
    }
    
    // Convert to FatFs path
    char fatfs_path[VFS_MAXPATHLEN];
    snprintf(fatfs_path, sizeof(fatfs_path), "%d:%s", fmd->drive_number, full_path);
    
    serial_puts("FAT: Attempting to remove directory: ");
    serial_puts(fatfs_path);
    serial_puts("\n");
    
    // Remove the directory using FatFs
    FRESULT fr = f_unlink(fatfs_path);
    if (fr != FR_OK) {
        serial_puts("FAT: Failed to remove directory ");
        serial_puts(fatfs_path);
        serial_puts(" (error ");
        char error_str[16];
        snprintf(error_str, sizeof(error_str), "%d", fr);
        serial_puts(error_str);
        serial_puts(")\n");
        return fatfs_error_to_vfs(fr);
    }
    
    serial_puts("FAT: Successfully removed directory ");
    serial_puts(fatfs_path);
    serial_puts("\n");
    return 0;
}