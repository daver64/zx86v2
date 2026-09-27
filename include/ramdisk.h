// ramdisk.h -- RAM-based block device interface

#ifndef RAMDISK_H
#define RAMDISK_H

#include <stdint.h>
#include <stddef.h>

// Initialize the ramdisk subsystem
void ramdisk_init(void);

// Create a ramdisk device of specified size (0 = default size)
// Returns device ID on success, -1 on failure
int ramdisk_create(size_t size_bytes);

// Destroy a ramdisk device
// Returns 0 on success, -1 on failure
int ramdisk_destroy(int device_id);

#endif // RAMDISK_H