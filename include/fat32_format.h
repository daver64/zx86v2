// fat32_format.h -- FAT32 formatting utility

#ifndef FAT32_FORMAT_H
#define FAT32_FORMAT_H

#include "blockdev.h"

// Format a block device with FAT32 filesystem
// Returns 0 on success, -1 on failure
int fat32_format(blockdev_t *bdev, const char *label);

#endif // FAT32_FORMAT_H