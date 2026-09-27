/*
 * unistd.c - POSIX system interface functions
 * ZX86v2 Operating System
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <vfs.h>
#include <syscall.h>

/*
 * Sleep for specified number of seconds
 * In our simple OS, this is a basic implementation
 */
unsigned int sleep(unsigned int seconds)
{
	// Simple busy wait implementation
	// In a real OS, this would yield to the scheduler
	volatile unsigned int count;
	for (unsigned int i = 0; i < seconds; i++) {
		for (count = 0; count < 1000000; count++) {
			// Busy wait approximating 1 second
		}
	}
	return 0;
}

/*
 * Sleep for specified number of microseconds
 */
int usleep(useconds_t usec)
{
	volatile unsigned int count;
	// Simple busy wait implementation
	for (count = 0; count < usec; count++) {
		// Approximate microsecond delay
	}
	return 0;
}

/*
 * Get current working directory
 */
char *getcwd(char *buf, size_t size)
{
	if (!buf) {
		if (size == 0) {
			size = VFS_MAXPATHLEN;
		}
		buf = malloc(size);
		if (!buf) {
			errno = ENOMEM;
			return NULL;
		}
	}
	
	// Use the existing os_getcwd function
	if (os_getcwd(buf, size) != 0) {
		errno = ERANGE;
		return NULL;
	}
	
	return buf;
}

/*
 * Change working directory
 */
int chdir(const char *path)
{
	if (!path) {
		errno = EFAULT;
		return -1;
	}
	
	// Use the existing os_chdir function
	int result = os_chdir(path);
	if (result != 0) {
		errno = ENOENT;
		return -1;
	}
	
	return 0;
}

/*
 * Note: getpid() is implemented in src/kernel/task.c
 * Not duplicated here to avoid linking conflicts
 */

/*
 * Get parent process ID
 */
pid_t getppid(void)
{
	return 0;  // Always return PID 0 (kernel) as parent
}

/*
 * Get user ID
 */
uid_t getuid(void)
{
	return 0;  // Always return root (0) for now
}

/*
 * Get effective user ID
 */
uid_t geteuid(void)
{
	return 0;  // Always return root (0) for now
}

/*
 * Get group ID
 */
gid_t getgid(void)
{
	return 0;  // Always return root group (0) for now
}

/*
 * Get effective group ID
 */
gid_t getegid(void)
{
	return 0;  // Always return root group (0) for now
}

/*
 * Test for access to a file
 * Basic implementation using our VFS
 */
int access(const char *pathname, int mode)
{
	FILE *fp;
	
	if (!pathname) {
		errno = EFAULT;
		return -1;
	}
	
	// For simplicity, just try to open the file
	fp = fopen(pathname, "r");
	if (fp) {
		fclose(fp);
		return 0;  // File exists and is readable
	}
	
	errno = ENOENT;
	return -1;
}

/*
 * Remove a file
 */
int unlink(const char *pathname)
{
	if (!pathname) {
		errno = EFAULT;
		return -1;
	}
	
	// Use the existing vfs_unlink function
	int result = vfs_unlink(pathname);
	if (result != 0) {
		errno = ENOENT;
		return -1;
	}
	
	return 0;
}

/*
 * Remove a directory
 */
int rmdir(const char *pathname)
{
	if (!pathname) {
		errno = EFAULT;
		return -1;
	}
	
	// For now, this is not implemented
	errno = ENOSYS;
	return -1;
}

/*
 * Create a directory
 */
int mkdir(const char *pathname, mode_t mode)
{
	if (!pathname) {
		errno = EFAULT;
		return -1;
	}
	
	// Ignore mode for now - create a simple argv array for os_mkdir
	char *argv[2];
	char *path_copy = malloc(strlen(pathname) + 1);
	if (!path_copy) {
		errno = ENOMEM;
		return -1;
	}
	strcpy(path_copy, pathname);
	
	argv[0] = "mkdir";
	argv[1] = path_copy;
	
	int result = os_mkdir(2, argv);
	free(path_copy);
	
	if (result != 0) {
		errno = (result == 1) ? EEXIST : ENOENT;
		return -1;
	}
	
	return 0;
}

/*
 * Check if file descriptor refers to a terminal
 */
int isatty(int fd)
{
	// stdin, stdout, stderr are considered terminals
	if (fd >= 0 && fd <= 2) {
		return 1;
	}
	
	errno = ENOTTY;
	return 0;
}

/*
 * Get terminal name
 */
char *ttyname(int fd)
{
	if (!isatty(fd)) {
		errno = ENOTTY;
		return NULL;
	}
	
	// Return a generic terminal name
	static char tty_name[] = "/dev/console";
	return tty_name;
}

/*
 * Thread-safe version of ttyname
 */
int ttyname_r(int fd, char *buf, size_t buflen)
{
	const char *name;
	size_t len;
	
	if (!buf || buflen == 0) {
		return EINVAL;
	}
	
	name = ttyname(fd);
	if (!name) {
		return errno;
	}
	
	len = strlen(name);
	if (len >= buflen) {
		return ERANGE;
	}
	
	strcpy(buf, name);
	return 0;
}

/*
 * Duplicate a file descriptor
 */
int dup(int oldfd)
{
	// For now, this is not implemented
	(void)oldfd;
	errno = ENOSYS;
	return -1;
}

/*
 * Duplicate a file descriptor to a specific fd
 */
int dup2(int oldfd, int newfd)
{
	// For now, this is not implemented
	(void)oldfd;
	(void)newfd;
	errno = ENOSYS;
	return -1;
}

/*
 * Close a file descriptor
 */
int close(int fd)
{
	// Use the existing fclose functionality
	// This is a simplified implementation
	if (fd < 0) {
		errno = EBADF;
		return -1;
	}
	
	// For stdin/stdout/stderr, don't actually close
	if (fd >= 0 && fd <= 2) {
		return 0;
	}
	
	// For now, this would need VFS integration
	errno = ENOSYS;
	return -1;
}

/*
 * Read from a file descriptor
 */
ssize_t read(int fd, void *buf, size_t count)
{
	// For now, this is not implemented
	// Would need integration with VFS
	(void)fd;
	(void)buf;
	(void)count;
	errno = ENOSYS;
	return -1;
}

/*
 * Note: write() is implemented in src/kernel/util.c
 * Not duplicated here to avoid linking conflicts
 */

/*
 * Position a file descriptor
 */
off_t lseek(int fd, off_t offset, int whence)
{
	// For now, this is not implemented
	(void)fd;
	(void)offset;
	(void)whence;
	errno = ENOSYS;
	return -1;
}

/*
 * Synchronize file data
 */
int fsync(int fd)
{
	// For now, this is not implemented
	(void)fd;
	errno = ENOSYS;
	return -1;
}

/*
 * Get page size
 */
int getpagesize(void)
{
	return 4096;  // Standard x86 page size
}

/*
 * Get system configuration values
 */
long sysconf(int name)
{
	switch (name) {
		case _SC_PAGE_SIZE:
			return 4096;
			
		case _SC_OPEN_MAX:
			return 256;  // Maximum open files
			
		case _SC_CLK_TCK:
			return 100;  // Clock ticks per second
			
		case _SC_NPROCESSORS_ONLN:
			return 1;    // Number of online processors
			
		default:
			errno = EINVAL;
			return -1;
	}
}