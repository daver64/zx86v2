// blockdev.c -- BSD-style block device management
// Provides unified interface for multiple disk drives

#include "common.h"
#include "blockdev.h"
#include "string.h"
#include "stdio.h"

// Maximum number of block devices supported
#define MAX_BLOCK_DEVICES 16

// Block device registry
static blockdev_t block_devices[MAX_BLOCK_DEVICES];
static int num_block_devices = 0;

// Forward declarations from existing drivers
uint32_t hdc_read_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer);
uint32_t hdc_write_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer);
uint32_t hdc_get_primary_sector_count(void);
uint8_t hdc_identify_primary(void);

uint32_t rdc_read_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer);
uint32_t rdc_write_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer);

// Block device operations for RAM disk
static int ramdisk_read(blockdev_t *dev, uint64_t sector, uint32_t count, void *buffer) {
    return rdc_read_sectors(sector, count, buffer);
}

static int ramdisk_write(blockdev_t *dev, uint64_t sector, uint32_t count, void *buffer) {
    return rdc_write_sectors(sector, count, buffer);
}

static uint64_t ramdisk_get_size(blockdev_t *dev) {
    extern uint32_t ramdisc_sector_count;
    return ramdisc_sector_count;
}

// Block device operation tables
static blockdev_ops_t ramdisk_ops = {
    .read = ramdisk_read,
    .write = ramdisk_write,
    .get_size = ramdisk_get_size
};

// Initialize block device subsystem
void blockdev_init(void) {
    printf("Block device subsystem initializing...\n");
    
    // Clear device table
    memset(block_devices, 0, sizeof(block_devices));
    num_block_devices = 0;
    
    // Note: Primary IDE drive (hd0) is registered by dfs.c after filesystem mount
    
    // Register RAM disk if available
    extern uint32_t ramdisc_sector_count;
    if (ramdisc_sector_count > 0) {
        blockdev_register("ramdisk0", BLOCKDEV_TYPE_RAMDISK, &ramdisk_ops, NULL);
        printf("Registered /dev/ramdisk0 (RAM disk)\n");
    }
    
    printf("Block device initialization complete (%d devices)\n", num_block_devices);
}

// Register a new block device
int blockdev_register(const char *name, blockdev_type_t type, blockdev_ops_t *ops, void *private_data) {
    serial_puts("BLOCKDEV: Starting registration\n");
    
    if (num_block_devices >= MAX_BLOCK_DEVICES) {
        serial_puts("BLOCKDEV: Too many devices registered\n");
        printf("blockdev: Too many devices registered\n");
        return -1;
    }
    
    if (!name || !ops) {
        serial_puts("BLOCKDEV: Invalid parameters\n");
        printf("blockdev: Invalid parameters\n");
        return -1;
    }
    
    blockdev_t *dev = &block_devices[num_block_devices];
    
    // Initialize device structure
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';
    dev->type = type;
    dev->ops = ops;
    dev->private_data = private_data;
    dev->sector_size = 512;  // Standard sector size
    
    // Get device size from driver
    if (ops->get_size) {
        dev->total_sectors = ops->get_size(dev);
    } else {
        dev->total_sectors = 0;
    }
    
    // Assign device number
    dev->major = 1;  // Block device major number
    dev->minor = num_block_devices;
    
    printf("Registered block device: %s (%llu sectors, %llu MB)\n", 
           dev->name, 
           dev->total_sectors,
           (dev->total_sectors * dev->sector_size) / (1024 * 1024));
    
    serial_puts("BLOCKDEV: Registration complete\n");
    num_block_devices++;
    
    // Add serial debug for the registration info
    char debug_buf[128];
    snprintf(debug_buf, sizeof(debug_buf), "BLOCKDEV: Registered %s (minor=%d)\n", dev->name, dev->minor);
    serial_puts(debug_buf);
    
    return dev->minor;
}

// Find block device by name
blockdev_t *blockdev_find(const char *name) {
    if (!name) return NULL;
    
    for (int i = 0; i < num_block_devices; i++) {
        if (strcmp(block_devices[i].name, name) == 0) {
            return &block_devices[i];
        }
    }
    return NULL;
}

// Find block device by minor number
blockdev_t *blockdev_get(int minor) {
    if (minor < 0 || minor >= num_block_devices) {
        return NULL;
    }
    return &block_devices[minor];
}

// Read sectors from block device
int blockdev_read(blockdev_t *dev, uint64_t sector, uint32_t count, void *buffer) {
    if (!dev || !dev->ops || !dev->ops->read) {
        return -1;
    }
    
    // Validate sector range
    if (sector >= dev->total_sectors || sector + count > dev->total_sectors) {
        printf("blockdev: Read beyond device bounds\n");
        return -1;
    }
    
    return dev->ops->read(dev, sector, count, buffer);
}

// Write sectors to block device
int blockdev_write(blockdev_t *dev, uint64_t sector, uint32_t count, void *buffer) {
    if (!dev || !dev->ops || !dev->ops->write) {
        return -1;
    }
    
    // Validate sector range
    if (sector >= dev->total_sectors || sector + count > dev->total_sectors) {
        printf("blockdev: Write beyond device bounds\n");
        return -1;
    }
    
    return dev->ops->write(dev, sector, count, buffer);
}

// List all registered block devices
void blockdev_list(void) {
    static const char ide_desc[] = "IDE/ATA hard disk";
    static const char ram_desc[] = "RAM disk";
    static const char usb_desc[] = "USB mass storage";
    static const char floppy_desc[] = "Floppy disk";
    static const char unknown_desc[] = "(unknown device type)";
    
    printf("Block devices:\n");
    printf("%-12s %-8s %-12s %-8s %s\n", "Device", "Type", "Sectors", "Size", "Description");
    printf("--------------------------------------------------------\n");
    
    for (int i = 0; i < num_block_devices; i++) {
        blockdev_t *dev = &block_devices[i];
        const char *type_str = "Unknown";
        const char *desc_ptr = unknown_desc;
        
        switch (dev->type) {
            case BLOCKDEV_TYPE_IDE:
                type_str = "IDE";
                desc_ptr = ide_desc;
                break;
            case BLOCKDEV_TYPE_RAMDISK:
                type_str = "RAM";
                desc_ptr = ram_desc;
                break;
            case BLOCKDEV_TYPE_USB:
                type_str = "USB";
                desc_ptr = usb_desc;
                break;
            case BLOCKDEV_TYPE_FLOPPY:
                type_str = "FLOPPY";
                desc_ptr = floppy_desc;
                break;
            default:
                type_str = "Unknown";
                desc_ptr = unknown_desc;
                break;
        }
        
        uint64_t size_mb = (dev->total_sectors * dev->sector_size) / (1024 * 1024);
        
        printf("%-12s %-8s %-12llu %-8llu %s\n", 
               dev->name, type_str, dev->total_sectors, size_mb, desc_ptr);
    }
}

// Get total number of registered devices
int blockdev_count(void) {
    return num_block_devices;
}