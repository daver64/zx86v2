// fat32_format.c -- Simple FAT32 formatter for ramdisk

#include "common.h"
#include "blockdev.h"
#include "kheap.h"
#include <stdint.h>
#include <string.h>

// FAT32 Boot Sector structure
typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];              // Jump instruction
    char     oem[8];              // OEM identifier
    uint16_t bytes_per_sector;    // Bytes per sector
    uint8_t  sectors_per_cluster; // Sectors per cluster
    uint16_t reserved_sectors;    // Reserved sectors
    uint8_t  fat_count;           // Number of FATs
    uint16_t root_entries;        // Root directory entries (0 for FAT32)
    uint16_t total_sectors_16;    // Total sectors (0 for FAT32)
    uint8_t  media_type;          // Media type
    uint16_t fat_size_16;         // FAT size in sectors (0 for FAT32)
    uint16_t sectors_per_track;   // Sectors per track
    uint16_t heads;               // Number of heads
    uint32_t hidden_sectors;      // Hidden sectors
    uint32_t total_sectors_32;    // Total sectors
    uint32_t fat_size_32;         // FAT size in sectors
    uint16_t ext_flags;           // Extended flags
    uint16_t fs_version;          // Filesystem version
    uint32_t root_cluster;        // Root directory cluster
    uint16_t fs_info;             // FSInfo sector
    uint16_t backup_boot;         // Backup boot sector
    uint8_t  reserved[12];        // Reserved
    uint8_t  drive_number;        // Drive number
    uint8_t  reserved1;           // Reserved
    uint8_t  boot_signature;      // Boot signature
    uint32_t volume_id;           // Volume ID
    char     volume_label[11];    // Volume label
    char     fs_type[8];          // Filesystem type
} fat32_boot_sector_t;

// Format a block device with FAT32
int fat32_format(blockdev_t *bdev, const char *label) {
    if (!bdev) {
        printf("fat32_format: Invalid block device\n");
        return -1;
    }
    
    printf("fat32_format: Formatting %s as FAT32\n", bdev->name);
    
    // Calculate FAT32 parameters based on device size
    uint32_t total_sectors = bdev->total_sectors;
    uint32_t bytes_per_sector = bdev->sector_size;
    uint8_t sectors_per_cluster = 8;  // 4KB clusters for small devices
    uint16_t reserved_sectors = 32;   // Standard for FAT32
    uint8_t fat_count = 2;            // Two FATs
    
    // Calculate FAT size (simplified calculation)
    uint32_t data_sectors = total_sectors - reserved_sectors;
    uint32_t clusters = data_sectors / sectors_per_cluster;
    uint32_t fat_entries = clusters + 2;  // +2 for reserved entries
    uint32_t fat_size_sectors = (fat_entries * 4 + bytes_per_sector - 1) / bytes_per_sector;
    
    // Adjust for actual FAT size
    data_sectors = total_sectors - reserved_sectors - (fat_count * fat_size_sectors);
    clusters = data_sectors / sectors_per_cluster;
    
    printf("fat32_format: %u sectors, %u clusters, FAT size %u sectors\n", 
           total_sectors, clusters, fat_size_sectors);
    
    // Create boot sector
    fat32_boot_sector_t *boot = (fat32_boot_sector_t*)kmalloc(bytes_per_sector);
    if (!boot) {
        printf("fat32_format: Failed to allocate boot sector\n");
        return -1;
    }
    memset(boot, 0, bytes_per_sector);
    
    // Fill boot sector
    boot->jmp[0] = 0xEB;
    boot->jmp[1] = 0x58;
    boot->jmp[2] = 0x90;
    memcpy(boot->oem, "MSWIN4.1", 8);
    boot->bytes_per_sector = bytes_per_sector;
    boot->sectors_per_cluster = sectors_per_cluster;
    boot->reserved_sectors = reserved_sectors;
    boot->fat_count = fat_count;
    boot->root_entries = 0;  // FAT32 has no fixed root directory
    boot->total_sectors_16 = 0;  // Use 32-bit field
    boot->media_type = 0xF8;  // Hard disk
    boot->fat_size_16 = 0;  // Use 32-bit field
    boot->sectors_per_track = 63;
    boot->heads = 255;
    boot->hidden_sectors = 0;
    boot->total_sectors_32 = total_sectors;
    boot->fat_size_32 = fat_size_sectors;
    boot->ext_flags = 0;
    boot->fs_version = 0;
    boot->root_cluster = 2;  // Root directory starts at cluster 2
    boot->fs_info = 1;  // FSInfo in sector 1
    boot->backup_boot = 6;  // Backup boot sector
    boot->drive_number = 0x80;  // Hard disk
    boot->boot_signature = 0x29;
    boot->volume_id = 0x12345678;
    
    // Set volume label
    memset(boot->volume_label, ' ', 11);
    if (label) {
        size_t label_len = strlen(label);
        if (label_len > 11) label_len = 11;
        memcpy(boot->volume_label, label, label_len);
    } else {
        memcpy(boot->volume_label, "RAMDISK   ", 11);
    }
    
    memcpy(boot->fs_type, "FAT32   ", 8);
    
    // Boot sector signature
    ((uint8_t*)boot)[510] = 0x55;
    ((uint8_t*)boot)[511] = 0xAA;
    
    // Write boot sector
    int result = blockdev_write(bdev, 0, 1, boot);
    if (result != 0) {
        printf("fat32_format: Failed to write boot sector\n");
        kfree(boot);
        return -1;
    }
    
    // Write backup boot sector
    blockdev_write(bdev, 6, 1, boot);
    
    // Store media type before freeing boot sector
    uint8_t media_type = boot->media_type;
    
    kfree(boot);
    
    // Create and write empty FATs
    uint32_t fat_bytes = fat_size_sectors * bytes_per_sector;
    uint8_t *fat_data = (uint8_t*)kmalloc(fat_bytes);
    if (!fat_data) {
        printf("fat32_format: Failed to allocate FAT data\n");
        return -1;
    }
    memset(fat_data, 0, fat_bytes);
    
    // Set up FAT entries
    uint32_t *fat = (uint32_t*)fat_data;
    fat[0] = 0x0FFFFF00 | media_type;  // Media type in first entry
    fat[1] = 0x0FFFFFFF;  // End of chain marker
    fat[2] = 0x0FFFFFFF;  // Root directory end of chain
    
    // Write first FAT
    uint32_t fat_start = reserved_sectors;
    result = blockdev_write(bdev, fat_start, fat_size_sectors, fat_data);
    if (result != 0) {
        printf("fat32_format: Failed to write first FAT\n");
        kfree(fat_data);
        return -1;
    }
    
    // Write second FAT
    result = blockdev_write(bdev, fat_start + fat_size_sectors, fat_size_sectors, fat_data);
    if (result != 0) {
        printf("fat32_format: Failed to write second FAT\n");
        kfree(fat_data);
        return -1;
    }
    
    kfree(fat_data);
    
    // Clear root directory cluster
    uint32_t root_sector = reserved_sectors + (fat_count * fat_size_sectors);
    uint8_t *cluster_data = (uint8_t*)kmalloc(sectors_per_cluster * bytes_per_sector);
    if (cluster_data) {
        memset(cluster_data, 0, sectors_per_cluster * bytes_per_sector);
        blockdev_write(bdev, root_sector, sectors_per_cluster, cluster_data);
        kfree(cluster_data);
    }
    
    printf("fat32_format: Successfully formatted %s\n", bdev->name);
    return 0;
}