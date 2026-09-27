// blockdev.h -- BSD-style block device interface
#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include "common.h"

// Block device types
typedef enum {
    BLOCKDEV_TYPE_IDE = 0,
    BLOCKDEV_TYPE_RAMDISK = 1,
    BLOCKDEV_TYPE_USB = 2,
    BLOCKDEV_TYPE_FLOPPY = 3
} blockdev_type_t;

// Forward declaration
struct blockdev;

// Block device operations (similar to BSD's bdevsw)
typedef struct {
    int (*read)(struct blockdev *dev, uint64_t sector, uint32_t count, void *buffer);
    int (*write)(struct blockdev *dev, uint64_t sector, uint32_t count, void *buffer);
    uint64_t (*get_size)(struct blockdev *dev);
    int (*ioctl)(struct blockdev *dev, int cmd, void *arg);
} blockdev_ops_t;

// Block device structure (similar to BSD's disk structure)
typedef struct blockdev {
    char name[32];              // Device name (e.g., "disk0", "ramdisk0")
    blockdev_type_t type;       // Device type
    int major;                  // Major device number
    int minor;                  // Minor device number
    uint32_t sector_size;       // Sector size in bytes (usually 512)
    uint64_t total_sectors;     // Total number of sectors
    blockdev_ops_t *ops;        // Device operations
    void *private_data;         // Driver-specific data
} blockdev_t;

// Block device management functions
void blockdev_init(void);
int blockdev_register(const char *name, blockdev_type_t type, blockdev_ops_t *ops, void *private_data);
blockdev_t *blockdev_find(const char *name);
blockdev_t *blockdev_get(int minor);
int blockdev_read(blockdev_t *dev, uint64_t sector, uint32_t count, void *buffer);
int blockdev_write(blockdev_t *dev, uint64_t sector, uint32_t count, void *buffer);
void blockdev_list(void);
int blockdev_count(void);

#endif // BLOCKDEV_H