// ramdisk.c -- RAM-based block device implementation
// Creates a memory-backed block device that can be formatted with any filesystem

#include "common.h"
#include "blockdev.h"
#include "kheap.h"
#include "vfs.h"
#include <stdint.h>
#include <string.h>

// Ramdisk configuration
#define RAMDISK_SECTOR_SIZE     512
#define RAMDISK_DEFAULT_SIZE    (8 * 1024 * 1024)  // 8MB default
#define RAMDISK_MAX_DEVICES     4

// Ramdisk device structure
typedef struct ramdisk_device {
    uint8_t *data;              // Memory buffer for the ramdisk
    size_t size;                // Total size in bytes
    uint32_t sector_count;      // Number of sectors
    int device_id;              // Device ID for registration
    int in_use;                 // Whether this device is active
} ramdisk_device_t;

// Global ramdisk devices
static ramdisk_device_t ramdisk_devices[RAMDISK_MAX_DEVICES];
static int ramdisk_count = 0;

// Forward declarations
static int ramdisk_read(struct blockdev *dev, uint64_t sector, uint32_t count, void *buffer);
static int ramdisk_write(struct blockdev *dev, uint64_t sector, uint32_t count, void *buffer);
static uint64_t ramdisk_get_size(struct blockdev *dev);

// Block device operations for ramdisk
static blockdev_ops_t ramdisk_ops = {
    .read = ramdisk_read,
    .write = ramdisk_write,
    .get_size = ramdisk_get_size,
    .ioctl = NULL
};

// Read from ramdisk
static int ramdisk_read(struct blockdev *dev, uint64_t sector, uint32_t count, void *buffer) {
    ramdisk_device_t *ramdisk = (ramdisk_device_t*)dev->private_data;
    
    if (!ramdisk || !ramdisk->data || !buffer) {
        printf("ramdisk: Invalid parameters for read\n");
        return -1;
    }
    
    // Check bounds
    if (sector + count > ramdisk->sector_count) {
        printf("ramdisk: Read beyond device bounds (sector %llu, count %u, max %u)\n", 
               sector, count, ramdisk->sector_count);
        return -1;
    }
    
    // Calculate byte offset
    size_t byte_offset = sector * RAMDISK_SECTOR_SIZE;
    size_t byte_count = count * RAMDISK_SECTOR_SIZE;
    
    // Copy data from ramdisk memory to buffer
    memcpy(buffer, ramdisk->data + byte_offset, byte_count);
    
    return 0;
}

// Write to ramdisk
static int ramdisk_write(struct blockdev *dev, uint64_t sector, uint32_t count, void *buffer) {
    ramdisk_device_t *ramdisk = (ramdisk_device_t*)dev->private_data;
    
    if (!ramdisk || !ramdisk->data || !buffer) {
        printf("ramdisk: Invalid parameters for write\n");
        return -1;
    }
    
    // Check bounds
    if (sector + count > ramdisk->sector_count) {
        printf("ramdisk: Write beyond device bounds (sector %llu, count %u, max %u)\n", 
               sector, count, ramdisk->sector_count);
        return -1;
    }
    
    // Calculate byte offset
    size_t byte_offset = sector * RAMDISK_SECTOR_SIZE;
    size_t byte_count = count * RAMDISK_SECTOR_SIZE;
    
    // Copy data from buffer to ramdisk memory
    memcpy(ramdisk->data + byte_offset, buffer, byte_count);
    
    return 0;
}

// Get ramdisk size
static uint64_t ramdisk_get_size(struct blockdev *dev) {
    ramdisk_device_t *ramdisk = (ramdisk_device_t*)dev->private_data;
    
    if (!ramdisk) {
        return 0;
    }
    
    return ramdisk->sector_count;
}

// Create a new ramdisk device
int ramdisk_create(size_t size_bytes) {
    if (ramdisk_count >= RAMDISK_MAX_DEVICES) {
        printf("ramdisk: Maximum number of ramdisk devices reached\n");
        return -1;
    }
    
    // Round up size to sector boundary
    if (size_bytes == 0) {
        size_bytes = RAMDISK_DEFAULT_SIZE;
    }
    size_bytes = (size_bytes + RAMDISK_SECTOR_SIZE - 1) & ~(RAMDISK_SECTOR_SIZE - 1);
    
    printf("ramdisk: Creating ramdisk of size %u bytes (%u KB)\n", 
           (uint32_t)size_bytes, (uint32_t)(size_bytes / 1024));
    
    // Allocate memory for the ramdisk
    uint8_t *ramdisk_memory = (uint8_t*)kmalloc(size_bytes);
    if (!ramdisk_memory) {
        printf("ramdisk: Failed to allocate %u bytes for ramdisk\n", (uint32_t)size_bytes);
        return -1;
    }
    
    // Zero the memory
    memset(ramdisk_memory, 0, size_bytes);
    
    // Find free device slot
    int device_idx = -1;
    for (int i = 0; i < RAMDISK_MAX_DEVICES; i++) {
        if (!ramdisk_devices[i].in_use) {
            device_idx = i;
            break;
        }
    }
    
    if (device_idx == -1) {
        kfree(ramdisk_memory);
        return -1;
    }
    
    // Initialize ramdisk device structure
    ramdisk_device_t *ramdisk = &ramdisk_devices[device_idx];
    ramdisk->data = ramdisk_memory;
    ramdisk->size = size_bytes;
    ramdisk->sector_count = size_bytes / RAMDISK_SECTOR_SIZE;
    ramdisk->device_id = device_idx;
    ramdisk->in_use = 1;
    
    // Create device name
    char device_name[16];
    sprintf(device_name, "ram%d", device_idx);
    
    // Register with block device system
    int result = blockdev_register(device_name, BLOCKDEV_TYPE_RAMDISK, &ramdisk_ops, ramdisk);
    if (result < 0) {
        printf("ramdisk: Failed to register block device\n");
        kfree(ramdisk_memory);
        ramdisk->in_use = 0;
        return -1;
    }
    
    ramdisk_count++;
    
    printf("ramdisk: Created %s with %u sectors (%u KB)\n", 
           device_name, ramdisk->sector_count, (uint32_t)(size_bytes / 1024));
    
    return device_idx;
}

// Destroy a ramdisk device
int ramdisk_destroy(int device_id) {
    if (device_id < 0 || device_id >= RAMDISK_MAX_DEVICES) {
        return -1;
    }
    
    ramdisk_device_t *ramdisk = &ramdisk_devices[device_id];
    if (!ramdisk->in_use) {
        return -1;
    }
    
    printf("ramdisk: Destroying ram%d\n", device_id);
    
    // Free memory
    if (ramdisk->data) {
        kfree(ramdisk->data);
    }
    
    // Clear device structure
    memset(ramdisk, 0, sizeof(ramdisk_device_t));
    
    ramdisk_count--;
    
    // TODO: Unregister from block device system if that functionality exists
    
    return 0;
}

// Initialize ramdisk subsystem
void ramdisk_init(void) {
    printf("ramdisk: Initializing RAM disk subsystem\n");
    
    // Clear all device structures
    memset(ramdisk_devices, 0, sizeof(ramdisk_devices));
    ramdisk_count = 0;
    
    printf("ramdisk: Initialization complete\n");
}

// Get ramdisk device by ID
ramdisk_device_t *ramdisk_get(int device_id) {
    if (device_id < 0 || device_id >= RAMDISK_MAX_DEVICES) {
        return NULL;
    }
    
    if (!ramdisk_devices[device_id].in_use) {
        return NULL;
    }
    
    return &ramdisk_devices[device_id];
}