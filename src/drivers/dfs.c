#include "common.h"
#include "graphics.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include "ff.h"   // FatFs library
#include "vfs.h"  // Add VFS support
#include "blockdev.h"  // Add block device support

// Block device wrapper functions for hard drive
static int hd0_read(struct blockdev *dev, uint64_t sector, uint32_t count, void *buffer) {
    uint32_t result = hdc_read_sectors(sector, (uint8_t)count, (uint8_t*)buffer);
    return (result == 0) ? 0 : -1;  // Convert to VFS error format
}

static int hd0_write(struct blockdev *dev, uint64_t sector, uint32_t count, void *buffer) {
    uint32_t result = hdc_write_sectors(sector, (uint8_t)count, (uint8_t*)buffer);
    return (result == 0) ? 0 : -1;  // Convert to VFS error format
}

// External disk functions from disc.c
extern uint32_t hdc_read_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer);
extern uint32_t hdc_write_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer);

FATFS *rootfs = NULL;
FATFS *ramfs = NULL;

// Global current directory for VFS (since VFS uses absolute paths)
// Remove the old DFS current directory tracking - VFS handles this now
static char temp_parent_buffer[VFS_MAXPATHLEN]; // Only keep this for cd .. calculations

// Try a completely isolated current directory tracker to avoid corruption
char g_current_working_directory[VFS_MAXPATHLEN] = "/";

FRESULT scan_files(char *path);
FRESULT scan_folders(char *path);
int is_executable_filename(char *filename);

// Simple glob pattern matching function
int glob_match(const char *pattern, const char *string) {
    const char *p = pattern;
    const char *s = string;
    
    while (*p && *s) {
        if (*p == '*') {
            // Skip consecutive asterisks
            while (*p == '*') p++;
            
            // If pattern ends with *, it matches
            if (!*p) return 1;
            
            // Try to match the rest of the pattern with remaining string
            while (*s) {
                if (glob_match(p, s)) return 1;
                s++;
            }
            return 0;
        } else if (*p == '?' || *p == *s) {
            p++;
            s++;
        } else {
            return 0;
        }
    }
    
    // Skip trailing asterisks in pattern
    while (*p == '*') p++;
    
    // Match if both pattern and string are exhausted
    return !*p && !*s;
}

// Function to control disk I/O mode (from diskio.c)
void set_disk_sync_mode(int sync_mode);

// Function to get primary disk sector count (from disc.c)
uint32_t hdc_get_primary_sector_count(void);

PARTITION VolToPart[FF_VOLUMES] = {
	{0, 1}};

unsigned int fgetsize(FILE *fp);
void os_report_space();
unsigned int os_unmount();
unsigned int os_mount();
int os_fdisc(int argc, char **argv)
{
	uint32_t pls = hdc_get_primary_sector_count();
	if (argc == 2)
	{
		pls = strtoul(argv[1], NULL, 16);
	}
	os_unmount();
	BYTE work[FF_MAX_SS];
	LBA_t plist[] = {pls};
	f_fdisk(0, plist, work);
	// f_mkfs("0:", 0, work, sizeof work);
	// os_mount();
	int result = 0;
	return result;
}

unsigned int os_mount_hdc()
{
	// Check if already mounted
	if (rootfs != NULL)
	{
		printf("Warning: filesystem already mounted\n");
		return FR_OK;
	}

	rootfs = malloc(sizeof(FATFS));
	if (!rootfs)
	{
		printf("Error: Failed to allocate memory for filesystem\n");
		return FR_NOT_ENOUGH_CORE;
	}

	FRESULT r = f_mount(rootfs, "0:", 1);
	if (r != FR_OK)
	{
		printf("Filesystem mount error %d\n", r);
		free(rootfs);
		rootfs = NULL;
		return r;
	}

	char str[12];
	memset(&str[0], 0, 12);
	f_getlabel("0:", str, 0);
	int i;
	int have_volume = false;
	if (str[0] != 0)
		have_volume = true;
	if (have_volume)
	{
		printf("Volume :");
		for (i = 0; i < 12; i++)
		{

			putchar(str[i]);
			// printf("%c{%u}",str[i],str[i]);
		}

		putchar('\n');
	}

	return r;
}

void os_report_space()
{
	if (!rootfs)
	{
		printf("Error: No filesystem mounted\n");
		return;
	}

	DWORD fre_clust, fre_sect, tot_sect;
	FRESULT r = f_getfree("", &fre_clust, &rootfs);
	tot_sect = (rootfs->n_fatent - 2) * rootfs->csize;
	fre_sect = fre_clust * rootfs->csize;
	printf("Format             : FAT32\n");
	printf("Total Space        : %u MB\n", ((tot_sect / 2) / 1024));
	printf("Available Space    : %u MB\n", ((fre_sect / 2) / 1024));
}

unsigned int os_mount_rdc()
{
	BYTE work[FF_MAX_SS];

	// Check if already mounted
	if (ramfs != NULL)
	{
		printf("Warning: RAM filesystem already mounted\n");
		return FR_OK;
	}

	ramfs = malloc(sizeof(FATFS));
	if (!ramfs)
	{
		printf("Error: Failed to allocate memory for RAM filesystem\n");
		return FR_NOT_ENOUGH_CORE;
	}

	FRESULT r = f_mount(ramfs, "1", 1);
	if (r != FR_OK)
	{
		printf("RAM filesystem mount error %d\n", r);
		free(ramfs);
		ramfs = NULL;
		return r;
	}

	// Create RAM disk filesystem
	LBA_t plist[] = {2048};
	f_fdisk(0, plist, work);

	DWORD fre_clust, fre_sect, tot_sect;
	r = f_mkfs("1", 0, work, sizeof(work));
	if (r != FR_OK)
	{
		printf("Error creating RAM disk filesystem %d\n", r);
		return r;
	}

	r = f_getfree("1", &fre_clust, &ramfs);
	tot_sect = (ramfs->n_fatent - 2) * ramfs->csize;
	fre_sect = fre_clust * ramfs->csize;
	printf("%10lu MB total drive space.\n%10lu MB available.\n", ((tot_sect / 2) / 1024), ((fre_sect / 2) / 1024));
	return r;
}

static uint64_t hd0_get_size(struct blockdev *dev) {
    extern uint32_t hdc_get_primary_sector_count(void);
    return (uint64_t)hdc_get_primary_sector_count();
}

static blockdev_ops_t hd0_ops = {
    .read = hd0_read,
    .write = hd0_write,
    .get_size = hd0_get_size
};

unsigned int os_mount()
{
	// First mount using traditional FatFs
	unsigned int result = os_mount_hdc();
	if (result != FR_OK)
	{
		printf("Failed to mount primary filesystem: %d\n", result);
		return result;
	}

	// Register hard drive as block device for VFS
	
	int reg_result = blockdev_register("hd0", BLOCKDEV_TYPE_IDE, &hd0_ops, NULL);
	
	if (reg_result < 0) {  // blockdev_register returns minor number on success, -1 on failure
		printf("Failed to register hd0 block device\n");
		return FR_OK;  // Continue with FatFs only
	}
	
	// Now register with VFS as root filesystem
	int vfs_result = vfs_mount("hd0", "/", "fat32", 0, NULL);
	if (vfs_result != 0) {
		printf("ERROR: VFS mount failed: %d\n", vfs_result);
		// Continue with traditional FatFs for now
	}

	// Optionally mount RAM drive - don't fail if this fails
	// os_mount_rdc();

	return FR_OK;
}

unsigned int os_unmount()
{
	unsigned int result = FR_OK;

	// Unmount primary filesystem
	if (rootfs)
	{
		FRESULT r = f_mount(NULL, "0:", 0);
		if (r != FR_OK)
		{
			printf("Warning: Error unmounting primary filesystem: %d\n", r);
			result = r;
		}
		free(rootfs);
		rootfs = NULL;
	}

	// Unmount RAM filesystem if present
	if (ramfs)
	{
		FRESULT r = f_mount(NULL, "1:", 0);
		if (r != FR_OK)
		{
			printf("Warning: Error unmounting RAM filesystem: %d\n", r);
			result = r;
		}
		free(ramfs);
		ramfs = NULL;
	}

	return result;
}

int os_mkfs(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Usage: mkfs <drive>\n");
		return 1;
	}

	BYTE work[FF_MAX_SS];
	FRESULT r = f_mkfs(argv[1], 0, work, sizeof(work));
	if (r != FR_OK)
	{
		printf("Error creating filesystem on %s: %d\n", argv[1], r);
		return r;
	}

	printf("Filesystem created successfully on %s\n", argv[1]);
	return 0;
}
int os_cp(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("Usage: cp <sourcefile> <destinationfile>\n");
		return 1;
	}

	// Validate parameters
	if (!argv[1] || !argv[2])
	{
		printf("Error: Invalid file names\n");
		return 1;
	}

	FILE *fps = fopen(argv[1], "r");
	if (!fps)
	{
		printf("Error: Cannot open source file '%s'\n", argv[1]);
		return 1;
	}

	unsigned int len = fgetsize(fps);
	if (len == 0)
	{
		printf("Warning: Source file '%s' is empty\n", argv[1]);
		fclose(fps);
		return 0;
	}

	unsigned char *buffer = malloc(len);
	if (!buffer)
	{
		printf("Error: Failed to allocate %u bytes for copy operation\n", len);
		fclose(fps);
		return 1;
	}

	size_t bytes_read = fread(buffer, 1, len, fps);
	fclose(fps);

	if (bytes_read != len)
	{
		printf("Error: Could only read %u of %u bytes from source file\n", (unsigned int)bytes_read, len);
		free(buffer);
		return 1;
	}

	FILE *fpd = fopen(argv[2], "w");
	if (!fpd)
	{
		printf("Error: Cannot create destination file '%s'\n", argv[2]);
		free(buffer);
		return 1;
	}

	size_t bytes_written = fwrite(buffer, 1, len, fpd);
	fclose(fpd);

	if (bytes_written != len)
	{
		printf("Error: Could only write %u of %u bytes to destination file\n", (unsigned int)bytes_written, len);
		free(buffer);
		return 1;
	}

	free(buffer);
	printf("Successfully copied '%s' to '%s' (%u bytes)\n", argv[1], argv[2], len);
	return 0;
}

int os_mv(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("Usage: mv <sourcefile> <destinationfile>\n");
		return 1;
	}

	// Validate parameters
	if (!argv[1] || !argv[2])
	{
		printf("Error: Invalid file names\n");
		return 1;
	}

	// Try using f_rename first (more efficient)
	FRESULT r = f_rename(argv[1], argv[2]);
	if (r == FR_OK)
	{
		printf("Successfully moved '%s' to '%s'\n", argv[1], argv[2]);
		return 0;
	}

	// If rename failed, fall back to copy + delete
	printf("Rename failed (%d), trying copy+delete...\n", r);

	// Use our copy function
	char *cp_args[] = {"cp", argv[1], argv[2]};
	int cp_result = os_cp(3, cp_args);
	if (cp_result != 0)
	{
		printf("Error: Copy operation failed\n");
		return cp_result;
	}

	// Delete original file
	r = f_unlink(argv[1]);
	if (r != FR_OK)
	{
		printf("Warning: Could not delete source file '%s' (error %d)\n", argv[1], r);
		return r;
	}

	printf("Successfully moved '%s' to '%s'\n", argv[1], argv[2]);
	return 0;
}

int os_rm(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Usage: rm <filename|pattern>\n");
		return 1;
	}

	// Validate parameter
	if (!argv[1])
	{
		printf("Error: Invalid filename\n");
		return 1;
	}

	const char *target = argv[1];
	int deleted_count = 0;
	int failed_count = 0;

	// Check if the target contains wildcards
	if (strchr(target, '*') || strchr(target, '?')) {
		// We have a pattern - need to find matching files
		const char *path = ".";
		const char *pattern = target;
		
		// Check if pattern includes a path
		char *last_slash = strrchr(target, '/');
		if (last_slash) {
			// Split path and pattern
			*last_slash = '\0';
			path = target;
			pattern = last_slash + 1;
			
			if (strlen(path) == 0) {
				path = "/";
			}
		}
		
		// Convert relative path to absolute if needed
		char abs_path[VFS_MAXPATHLEN];
		memset(abs_path, 0, sizeof(abs_path));
		
		if (path[0] != '/') {
			if (strcmp(path, ".") == 0) {
				strncpy(abs_path, g_current_working_directory, sizeof(abs_path) - 1);
				abs_path[sizeof(abs_path) - 1] = '\0';
			} else {
				if (g_current_working_directory[0]) {
					if (strcmp(g_current_working_directory, "/") == 0) {
						snprintf(abs_path, sizeof(abs_path), "/%s", path);
					} else {
						snprintf(abs_path, sizeof(abs_path), "%s/%s", g_current_working_directory, path);
					}
				} else {
					snprintf(abs_path, sizeof(abs_path), "/%s", path);
				}
			}
			path = abs_path;
		}
		
		// Open directory and find matching files
		int dir_fd = vfs_opendir(path);
		if (dir_fd < 0) {
			printf("Error: Cannot access directory '%s'\n", path);
			return 1;
		}
		
		printf("Removing files matching pattern '%s' in '%s':\n", pattern, path);
		
		char entry_name[VFS_MAXNAMELEN];
		size_t entry_size;
		
		while (1) {
			memset(entry_name, 0, sizeof(entry_name));
			entry_size = sizeof(entry_name) - 1;
			int result = vfs_readdir(dir_fd, entry_name, &entry_size);
			
			if (result != 0 || entry_size == 0) {
				break;
			}
			
			if (entry_size < sizeof(entry_name)) {
				entry_name[entry_size] = '\0';
			} else {
				entry_name[sizeof(entry_name) - 1] = '\0';
			}
			
			// Skip . and .. entries
			if (strcmp(entry_name, ".") == 0 || strcmp(entry_name, "..") == 0) {
				continue;
			}
			
			// Check if filename matches pattern
			if (glob_match(pattern, entry_name)) {
				// Build full path for deletion
				char full_path[VFS_MAXPATHLEN];
				memset(full_path, 0, sizeof(full_path));
				if (strcmp(path, "/") == 0) {
					snprintf(full_path, sizeof(full_path), "/%s", entry_name);
				} else {
					snprintf(full_path, sizeof(full_path), "%s/%s", path, entry_name);
				}
				
				// Check if it's a directory (don't delete directories with rm)
				int test_fd = vfs_opendir(full_path);
				if (test_fd >= 0) {
					vfs_closedir(test_fd);
					printf("Skipping directory '%s' (use rmdir)\n", entry_name);
					continue;
				}
				
				// Delete the file
				int delete_result = vfs_unlink(full_path);
				if (delete_result == 0) {
					printf("Deleted '%s'\n", entry_name);
					deleted_count++;
				} else {
					printf("Failed to delete '%s'\n", entry_name);
					failed_count++;
				}
			}
		}
		
		vfs_closedir(dir_fd);
		
		if (deleted_count == 0 && failed_count == 0) {
			printf("No files match pattern '%s'\n", pattern);
			return 1;
		} else {
			printf("Deleted %d files, %d failures\n", deleted_count, failed_count);
			return (failed_count > 0) ? 1 : 0;
		}
	} else {
		// Single file deletion (original logic)
		const char *filepath = argv[1];
		char abs_path[VFS_MAXPATHLEN];
		memset(abs_path, 0, sizeof(abs_path));
		
		// Special case: handle corrupted foo2 file
		if (strcmp(argv[1], "foo2") == 0) {
			serial_puts("RM: Attempting to clean up corrupted foo2 file\n");
			if (g_current_working_directory[0]) {
				if (strcmp(g_current_working_directory, "/") == 0) {
					snprintf(abs_path, sizeof(abs_path), "/foo2ú");
				} else {
					snprintf(abs_path, sizeof(abs_path), "%s/foo2ú", g_current_working_directory);
				}
			} else {
				snprintf(abs_path, sizeof(abs_path), "/foo2ú");
			}
			filepath = abs_path;
		} else if (filepath[0] != '/') {
			// Normal relative path - resolve using current directory
			if (g_current_working_directory[0]) {
				if (strcmp(g_current_working_directory, "/") == 0) {
					snprintf(abs_path, sizeof(abs_path), "/%s", filepath);
				} else {
					snprintf(abs_path, sizeof(abs_path), "%s/%s", g_current_working_directory, filepath);
				}
			} else {
				snprintf(abs_path, sizeof(abs_path), "/%s", filepath);
			}
			filepath = abs_path;
		}
		
		int result = vfs_unlink(filepath);
		
		if (result == 0) {
			printf("Successfully deleted '%s'\n", argv[1]);
			return 0;
		} else {
			printf("Error: Cannot delete '%s'\n", argv[1]);
			return 1;
		}
	}
}

int os_ls(int argc, char **argv)
{
	extern void serial_puts(const char *msg);
	serial_puts("SERIAL: LS - Starting\n");
	
	const char *path = ".";  // Default to current directory
	const char *pattern = NULL;  // Pattern for glob matching
	
	if (argc == 2) {
		// Check if the argument contains wildcards
		if (strchr(argv[1], '*') || strchr(argv[1], '?')) {
			// We have a pattern
			char *last_slash = strrchr(argv[1], '/');
			if (last_slash) {
				// Split path and pattern
				*last_slash = '\0';
				path = argv[1];
				pattern = last_slash + 1;
				
				if (strlen(path) == 0) {
					path = "/";
				}
			} else {
				// Pattern in current directory
				pattern = argv[1];
				path = ".";
			}
		} else {
			// No pattern, just a path
			path = argv[1];
		}
	} else if (argc > 2) {
		printf("usage: ls [directory|pattern]\n");
		return 1;
	}
	
	// Convert relative path to absolute if needed
	char abs_path[VFS_MAXPATHLEN];
	memset(abs_path, 0, sizeof(abs_path));  // Clear the buffer first
	
	if (path[0] != '/') {
		// Handle special case of current directory
		if (strcmp(path, ".") == 0) {
			// Use current working directory directly
			strncpy(abs_path, g_current_working_directory, sizeof(abs_path) - 1);
			abs_path[sizeof(abs_path) - 1] = '\0';
		} else {
			// Relative path - resolve using current directory
			if (g_current_working_directory[0]) {
				if (strcmp(g_current_working_directory, "/") == 0) {
					snprintf(abs_path, sizeof(abs_path), "/%s", path);
				} else {
					snprintf(abs_path, sizeof(abs_path), "%s/%s", g_current_working_directory, path);
				}
			} else {
				// Fallback to root if no current directory
				snprintf(abs_path, sizeof(abs_path), "/%s", path);
			}
		}
		path = abs_path;
	}
	
	// Use VFS to open directory
	serial_puts("SERIAL: LS - About to open directory\n");
	int dir_fd = vfs_opendir(path);
	if (dir_fd < 0) {
		printf("Error: Cannot access directory '%s'\n", path);
		serial_puts("SERIAL: LS - Directory open failed\n");
		return 1;
	}
	serial_puts("SERIAL: LS - Directory opened successfully\n");

	if (pattern) {
		printf("Directory listing for '%s' matching '%s':\n", path, pattern);
	} else {
		printf("Directory listing for '%s':\n", path);
	}
	printf("%-30s\t%8s\n", "Name", "Type");
	printf("%-30s\t%8s\n", "----", "----");

	// Store original foreground color
	uint8_t ofgc = get_foreground_colour();
	disable_cursor();

	// Read directory entries
	char entry_name[VFS_MAXNAMELEN];
	size_t entry_size;
	int entry_count = 0;
	
	while (1) {
		// Clear the buffer before each read
		memset(entry_name, 0, sizeof(entry_name));
		entry_size = sizeof(entry_name) - 1;  // Leave room for null terminator
		int result = vfs_readdir(dir_fd, entry_name, &entry_size);
		
		if (result != 0 || entry_size == 0) {
			break;  // End of directory or error
		}
		
		// Ensure null termination
		if (entry_size < sizeof(entry_name)) {
			entry_name[entry_size] = '\0';
		} else {
			entry_name[sizeof(entry_name) - 1] = '\0';
		}
		
		// Skip . and .. entries
		if (strcmp(entry_name, ".") == 0 || strcmp(entry_name, "..") == 0) {
			continue;
		}
		
		// Apply pattern matching if pattern is specified
		if (pattern && !glob_match(pattern, entry_name)) {
			continue;
		}
		
		// Check if this entry is a directory by trying to open it
		char full_path[VFS_MAXPATHLEN];
		memset(full_path, 0, sizeof(full_path));
		if (strcmp(path, "/") == 0) {
			snprintf(full_path, sizeof(full_path), "/%s", entry_name);
		} else {
			snprintf(full_path, sizeof(full_path), "%s/%s", path, entry_name);
		}
		
		int test_fd = vfs_opendir(full_path);
		if (test_fd >= 0) {
			// It's a directory
			vfs_closedir(test_fd);
			set_foreground_colour(VGA_LIGHTBLUE);
			printf("%-30s\t%8s\n", entry_name, "<dir>");
			set_foreground_colour(VGA_LIGHTGREY);
			entry_count++;
		} else {
			// It's a file - check if executable
			if (is_executable_filename(entry_name)) {
				set_foreground_colour(VGA_RED);
				printf("%-30s\t%8s\n", entry_name, "exec");
				set_foreground_colour(VGA_LIGHTGREY);
			} else {
				printf("%-30s\t%8s\n", entry_name, "file");
			}
			entry_count++;
		}
	}

	printf("\nTotal entries: %d\n", entry_count);
	
	serial_puts("SERIAL: LS - About to close directory\n");
	vfs_closedir(dir_fd);
	serial_puts("SERIAL: LS - Directory closed, finishing\n");
	set_foreground_colour(ofgc);
	enable_cursor();
	
	// Ensure all output is flushed to screen before returning
	extern void terminal_flush(void);
	terminal_flush();
	
	return 0;
}

int os_mkdir(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("usage:\nmkdir <dirname>\n");
		return 1;
	}

	const char *dirname = argv[1];
	
	// Convert relative path to absolute if needed
	char abs_path[VFS_MAXPATHLEN];
	memset(abs_path, 0, sizeof(abs_path));  // Clear the buffer first
	
	if (dirname[0] != '/') {
		// Relative path - resolve using current directory
		if (g_current_working_directory[0]) {
			if (strcmp(g_current_working_directory, "/") == 0) {
				snprintf(abs_path, sizeof(abs_path), "/%s", dirname);
			} else {
				snprintf(abs_path, sizeof(abs_path), "%s/%s", g_current_working_directory, dirname);
			}
		} else {
			// Fallback to root if no current directory
			snprintf(abs_path, sizeof(abs_path), "/%s", dirname);
		}
		dirname = abs_path;
	}
	
	// Use VFS mkdir function to create directory
	int result = vfs_mkdir(dirname, 0755);
	
	if (result == 0)
	{
		printf("Directory '%s' created successfully\n", dirname);
		return 0;
	}
	else
	{
		printf("Error: Cannot create directory '%s'\n", dirname);
		return 1;
	}
}

int os_rmdir(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("usage:\nrmdir <dirname>\n");
		return 1;
	}

	const char *dirname = argv[1];
	char abs_path[VFS_MAXPATHLEN];
	memset(abs_path, 0, sizeof(abs_path));  // Clear the buffer first
	
	// Convert relative path to absolute if needed
	if (dirname[0] != '/') {
		// Relative path - use isolated global current directory
		const char *current_dir = g_current_working_directory;
		if (strcmp(current_dir, "/") == 0) {
			snprintf(abs_path, sizeof(abs_path), "/%s", dirname);
		} else {
			snprintf(abs_path, sizeof(abs_path), "%s/%s", current_dir, dirname);
		}
	} else {
		strncpy(abs_path, dirname, sizeof(abs_path) - 1);
		abs_path[sizeof(abs_path) - 1] = '\0';
	}

	int result = vfs_rmdir(abs_path);

	if (result == 0)
	{
		printf("Directory '%s' removed successfully\n", dirname);
		return 0;
	}
	else
	{
		// Map common error codes
		switch (result)
		{
		case -ENOENT:
			printf("Error: Directory '%s' not found\n", dirname);
			break;
		case -EACCES:
			printf("Error: Access denied - cannot remove directory '%s'\n", dirname);
			break;
		case -ENOTEMPTY:
			printf("Error: Directory '%s' is not empty\n", dirname);
			break;
		case -EROFS:
			printf("Error: Read-only filesystem - cannot remove directory '%s'\n", dirname);
			break;
		case -EINVAL:
			printf("Error: Invalid directory name '%s'\n", dirname);
			break;
		case -EIO:
			printf("Error: I/O error while removing directory '%s'\n", dirname);
			break;
		default:
			printf("Error: Failed to remove directory '%s' (error code %d)\n", dirname, result);
			break;
		}
		return 1;
	}
}

// Simple chdir function for internal use
int os_chdir(const char *path)
{
	if (!path) {
		return -1;
	}
	
	// Use the same logic as os_cd but without argc/argv
	char new_path[VFS_MAXPATHLEN];
	
	// Handle special directory names first
	if (strcmp(path, "..") == 0) {
		// Go up one directory - use static buffer to avoid stack corruption
		memset(temp_parent_buffer, 0, sizeof(temp_parent_buffer));
		strcpy(temp_parent_buffer, g_current_working_directory);
		
		char *last_slash = strrchr(temp_parent_buffer, '/');
		if (last_slash && last_slash != temp_parent_buffer) {
			*last_slash = '\0';  // Remove last component
			strcpy(new_path, temp_parent_buffer);
		} else {
			strcpy(new_path, "/");  // Already at root
		}
	} else if (path[0] == '/') {
		// Absolute path
		strcpy(new_path, path);
	} else {
		// Relative path
		if (strcmp(g_current_working_directory, "/") == 0) {
			snprintf(new_path, sizeof(new_path), "/%s", path);
		} else {
			snprintf(new_path, sizeof(new_path), "%s/%s", g_current_working_directory, path);
		}
	}
	
	// Check if directory exists using VFS
	int test_fd = vfs_opendir(new_path);
	if (test_fd < 0) {
		return -1;  // Directory doesn't exist
	}
	vfs_closedir(test_fd);
	
	// Update the isolated global directory
	strcpy(g_current_working_directory, new_path);
	
	return 0;
}

int os_cd(int argc, char **argv)
{
	if (argc == 0)
	{
		// Print current directory from isolated global
		printf("%s\n", g_current_working_directory);
		return 0;
	}
	if (argc != 2 && argc != 0)
	{
		printf("usage:\ncd <dirname>\n");
		return 1;
	}

	const char *target = argv[1];
	char new_path[VFS_MAXPATHLEN];
	memset(new_path, 0, sizeof(new_path));  // Clear buffer before use
	
	// Handle special directory names first
	if (strcmp(target, "..") == 0) {
		// Go up one directory - use static buffer to avoid stack corruption
		memset(temp_parent_buffer, 0, sizeof(temp_parent_buffer));
		strncpy(temp_parent_buffer, g_current_working_directory, sizeof(temp_parent_buffer) - 1);
		temp_parent_buffer[sizeof(temp_parent_buffer) - 1] = '\0';
		
		if (strcmp(temp_parent_buffer, "/") == 0) {
			// Already at root
			return 0;
		}
		
		// Find last slash and truncate there
		char *last_slash = strrchr(temp_parent_buffer, '/');
		if (last_slash != NULL && last_slash != temp_parent_buffer) {
			*last_slash = '\0';
		} else {
			// Set to root
			memset(temp_parent_buffer, 0, sizeof(temp_parent_buffer));
			temp_parent_buffer[0] = '/';
			temp_parent_buffer[1] = '\0';
		}
		
		// Update isolated global tracker only
		memset(g_current_working_directory, 0, sizeof(g_current_working_directory));
		strncpy(g_current_working_directory, temp_parent_buffer, sizeof(g_current_working_directory) - 1);
		g_current_working_directory[sizeof(g_current_working_directory) - 1] = '\0';
		
		serial_puts("cd: changed to ");
		serial_puts(temp_parent_buffer);
		serial_puts("\n");
		
		// Check if corruption happens immediately
		serial_puts("cd: final check - g_cwd is ");
		serial_puts(g_current_working_directory);
		serial_puts("\n");
		
		return 0;
	}
	
	if (strcmp(target, ".") == 0) {
		// Stay in current directory
		return 0;
	}

	// Handle absolute vs relative paths
	serial_puts("cd: target = ");
	serial_puts(target);
	serial_puts("\n");
	serial_puts("cd: target[0] = ");
	if (target[0] == '/') {
		serial_puts("'/' (absolute path)\n");
	} else {
		serial_puts("not '/' (relative path)\n");
	}
	
	if (target[0] == '/') {
		// Absolute path
		strncpy(new_path, target, sizeof(new_path) - 1);
		new_path[sizeof(new_path) - 1] = '\0';
		serial_puts("cd: using absolute path: ");
		serial_puts(new_path);
		serial_puts("\n");
	} else {
		// Relative path - append to current directory
		const char *current_dir = g_current_working_directory;
		if (strcmp(current_dir, "/") == 0) {
			snprintf(new_path, sizeof(new_path), "/%s", target);
		} else {
			snprintf(new_path, sizeof(new_path), "%s/%s", current_dir, target);
		}
		serial_puts("cd: using relative path: ");
		serial_puts(new_path);
		serial_puts("\n");
	}
	
	// Check if the target directory exists by trying to open it
	int test_fd = vfs_opendir(new_path);
	if (test_fd < 0) {
		printf("cd: cannot access '%s': No such file or directory\n", target);
		return 1;
	}
	
	vfs_closedir(test_fd);
	
	// Update isolated global tracker only
	int path_len = strlen(new_path);
	if (path_len < sizeof(g_current_working_directory)) {
		memset(g_current_working_directory, 0, sizeof(g_current_working_directory));
		strcpy(g_current_working_directory, new_path);
	}
	
	serial_puts("cd: set g_cwd to ");
	serial_puts(new_path);
	serial_puts("\n");
	
	serial_puts("cd: changed to ");
	serial_puts(new_path);
	serial_puts("\n");
	
	// Check if corruption happens immediately
	serial_puts("cd: final check - g_cwd is ");
	serial_puts(g_current_working_directory);
	serial_puts("\n");
	
	return 0;
}

int os_getcwd(char *buffer, int maxlen)
{
	if (!buffer || maxlen <= 0)
	{
		return 1; // Error: invalid parameters
	}

	// Use completely isolated current directory tracker
	const char *cwd_source = g_current_working_directory;
	
	// Check for corruption at start of getcwd
	if (strstr(g_current_working_directory, "\xDB") || strstr(g_current_working_directory, "\xFF") || strstr(g_current_working_directory, "\xDC")) {
		serial_puts("CRITICAL: g_current_working_directory corrupted, resetting to root\n");
		strcpy(g_current_working_directory, "/");
		cwd_source = "/";
	}
	
	int len = strlen(cwd_source);
	if (len >= maxlen) {
		return 1; // Buffer too small
	}
	
	// Clear the entire buffer first
	memset(buffer, 0, maxlen);
	
	// Use strcpy since we already checked the length
	strcpy(buffer, cwd_source);
	
	// Additional safety check after copy
	buffer[maxlen - 1] = '\0';
	
	return 0;
}

int os_chdrive(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("usage:\nchdrive <drv>\n");
		return 1;
	}

	FRESULT r1 = f_chdrive(argv[1]);
	FRESULT r2 = f_chdir("/");
	
	// Also update our VFS directory tracking when drive changes
	if (r1 == FR_OK && r2 == FR_OK) {
		strcpy(g_current_working_directory, "/");
	}

	return (r1 == FR_OK && r2 == FR_OK) ? 0 : 1;
}

FRESULT scan_folders(char *path)
{
	// Use synchronous I/O for directory operations to avoid FatFS conflicts
	set_disk_sync_mode(1);

	// disable_interrupts();  // Removed: breaks interrupt-driven disk I/O
	FRESULT res;
	DIR dir;
	UINT i;
	static FILINFO fno;

	uint8_t ofgc = get_foreground_colour();
	set_foreground_colour(VGA_LIGHTBLUE);
	res = f_opendir(&dir, path); /* Open the directory */
	if (res == FR_OK)
	{
		for (;;)
		{
			res = f_readdir(&dir, &fno); /* Read a directory item */
			if (res != FR_OK || fno.fname[0] == 0)
				break; /* Break on error or end of dir */
			if (fno.fattrib & AM_DIR)
			{
				for (int i = 0; i < 12; i++)
					fno.fname[i] = tolower(fno.fname[i]);
				printf("%-30s\t%8s\n", fno.fname, "<dir>");
			}
		}
		f_closedir(&dir);
	}
	set_foreground_colour(ofgc);
	// enable_interrupts();  // Removed: breaks interrupt-driven disk I/O

	// Reset to async I/O mode after directory operations
	set_disk_sync_mode(0);

	return res;
}
int is_executable_filename(char *filename)
{
	return (strstr(filename, ".elf") != NULL || strstr(filename, ".com") != NULL);
}

FRESULT scan_files(char *path)
{
	// Use synchronous I/O for directory operations to avoid FatFS conflicts
	set_disk_sync_mode(1);

	// disable_interrupts();  // Removed: breaks interrupt-driven disk I/O
	disable_cursor();
	// disable_blink();
	FRESULT res;
	DIR dir;
	UINT i;
	static FILINFO fno;

	uint8_t ofgc = get_foreground_colour();

	set_foreground_colour(VGA_LIGHTGREY);
	res = f_opendir(&dir, path); /* Open the directory */
	if (res == FR_OK)
	{
		for (;;)
		{
			res = f_readdir(&dir, &fno); /* Read a directory item */
			if (res != FR_OK || fno.fname[0] == 0)
				break; /* Break on error or end of dir */
			if (fno.fattrib & AM_DIR)
			{	/* It is a directory */
				//	printf("[%s]\n",fno.fname);
				// i = strlen(path);
				// sprintf(&path[i], "/%s", fno.fname);
				// res = scan_files(path);
				// if (res != FR_OK) break;
				// path[i] = 0;
			}
			else
			{ /* It is a file. */
				for (int i = 0; i < 12; i++)
					fno.fname[i] = tolower(fno.fname[i]);
				if (is_executable_filename(fno.fname))
				{
					set_foreground_colour(VGA_RED);
					printf("%-30s\t%8u\n", fno.fname, fno.fsize);
					set_foreground_colour(VGA_LIGHTGREY);
				}
				else
				{
					printf("%-30s\t%8u\n", fno.fname, fno.fsize);
				}
			}
		}
		f_closedir(&dir);
	}
	set_foreground_colour(ofgc);
	enable_cursor();
	// enable_interrupts();  // Removed: breaks interrupt-driven disk I/O
	printf("\n");

	// enable_blink();

	// Reset to async I/O mode after directory operations
	set_disk_sync_mode(0);

	return res;
}
