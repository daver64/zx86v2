#include "common.h"
#include "graphics.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <vfs.h>

// Special FILE structures for standard streams
static FILE stdin_file = {-1, FREAD, 1};
static FILE stdout_file = {-2, FWRITE, 1}; 
static FILE stderr_file = {-3, FWRITE, 1};

FILE *stdin = &stdin_file;
FILE *stdout = &stdout_file;
FILE *stderr = &stderr_file;
FILE *path_fopen(const char *filename, const char *mode)
{
	FILE *fp;
	int argc;
	char **args;
	char *pvar = getenv("PATH");
	if (!pvar)
		return NULL;
	argc = string_split(pvar, ';', &args);
	FILE *fpresult = NULL;
	for (int i = 0; i < argc; i++)
	{
		char buffer[MAX_PATH];
		snprintf((char *)&buffer[0], MAX_PATH, "%s\\%s", args[i], filename);
		fp = fopen((char *)&buffer[0], mode);
		if (fp)
		{
			fpresult = fp;
			i = argc;
		}
	}
	for (int i = 0; i < argc; i++)
	{
		free(*(args + i));
	}
	free(args);
	return fpresult;
}

FILE *fopen(const char *filename, const char *mode)
{
	if (!filename || !mode) {
		return NULL;
	}
	
	FILE *fp = malloc(sizeof(FILE));
	if (!fp) {
		return NULL;
	}
	
	// Parse mode string to VFS flags
	int flags = 0;
	const char *p = mode;
	
	// Parse primary mode character
	switch (*p) {
		case 'r':
			flags = FREAD;
			p++;
			// Check for '+' (read/write)
			if (*p == '+') {
				flags |= FWRITE;
				p++;
			}
			break;
			
		case 'w':
			flags = FWRITE | FCREAT | FTRUNC;
			p++;
			// Check for '+' (read/write)
			if (*p == '+') {
				flags |= FREAD;
				p++;
			}
			break;
			
		case 'a':
			flags = FWRITE | FCREAT | FAPPEND;
			p++;
			// Check for '+' (read/write)
			if (*p == '+') {
				flags |= FREAD;
				p++;
			}
			break;
			
		default:
			free(fp);
			return NULL;
	}
	
	// Parse optional 'b' flag (binary mode)
	// Note: In most Unix-like systems, binary mode is the default
	// and 'b' is ignored, which is what we'll do here
	if (*p == 'b') {
		p++;
	}
	
	// Check for '+' after 'b' (e.g., "rb+")
	if (*p == '+' && !(flags & FWRITE)) {
		if (flags & FREAD) {
			flags |= FWRITE;
		}
		p++;
	}
	
	// Open file using VFS
	int fd = vfs_open(filename, flags, 0644);
	if (fd < 0) {
		free(fp);
		return NULL;
	}
	
	fp->vfs_fd = fd;
	fp->flags = flags;
	fp->is_special = 0;
	
	return fp;
}

int fclose(FILE *fp)
{
	if (!fp || fp->is_special) {
		return 0;  // Can't close standard streams
	}
	
	int result = vfs_close(fp->vfs_fd);
	free(fp);
	return result;
}

unsigned int ftell(FILE *fp)
{
	if (!fp || fp->is_special) {
		return 0;
	}
	
	// Use VFS seek to get current position
	off_t pos = vfs_lseek(fp->vfs_fd, 0, VFS_SEEK_CUR);
	return (pos >= 0) ? (unsigned int)pos : 0;
}

unsigned int fread(void *buffer, unsigned int size, unsigned int count, FILE *fp)
{
	if (!fp || !buffer || size == 0 || count == 0) {
		return 0;
	}
	
	// Handle standard input
	if (fp == stdin) {
		char *ptr = (char *)buffer;
		unsigned nchars = size * count;
		for (unsigned i = 0; i < nchars; i++) {
			ptr[i] = getchar();
			putchar(ptr[i]);
		}
		return count;
	}
	
	// Handle other special streams
	if (fp == stdout || fp == stderr) {
		return 0;
	}
	
	// Regular file - use VFS
	ssize_t bytes_read = vfs_read(fp->vfs_fd, buffer, size * count);
	return (bytes_read > 0) ? bytes_read / size : 0;
}

unsigned int fwrite(const void *buffer, unsigned int size, unsigned int count, FILE *fp)
{
	if (!fp || !buffer || size == 0 || count == 0) {
		return 0;
	}
	
	// Handle standard output/error
	if (fp == stdout || fp == stderr) {
		char *ptr = (char *)buffer;
		unsigned nchars = size * count;
		for (unsigned i = 0; i < nchars; i++) {
			putchar(ptr[i]);
		}
		return count;
	}
	
	// Handle standard input (can't write to it)
	if (fp == stdin) {
		return 0;
	}
	
	// Regular file - use VFS
	ssize_t bytes_written = vfs_write(fp->vfs_fd, buffer, size * count);
	return (bytes_written > 0) ? bytes_written / size : 0;
}

unsigned int fseek(FILE *fp, unsigned int offset, int origin)
{
	if (!fp || fp->is_special) {
		return 0;
	}
	
	// Convert standard C seek origins to VFS constants
	int vfs_whence;
	switch (origin) {
		case SEEK_SET:
			vfs_whence = VFS_SEEK_SET;
			break;
		case SEEK_CUR:
			vfs_whence = VFS_SEEK_CUR;
			break;
		case SEEK_END:
			vfs_whence = VFS_SEEK_END;
			break;
		default:
			return -1; // Invalid origin
	}
	
	// Use VFS seek
	off_t result = vfs_lseek(fp->vfs_fd, (off_t)offset, vfs_whence);
	return (result >= 0) ? 0 : -1; // Return 0 on success, -1 on error
}

/*
 * BSD-style asprintf - allocate and format string
 */
int asprintf(char **ret, const char *format, ...)
{
	va_list ap;
	int len;
	
	va_start(ap, format);
	len = vasprintf(ret, format, ap);
	va_end(ap);
	
	return len;
}

/*
 * BSD-style vasprintf - allocate and format string with va_list
 */
int vasprintf(char **ret, const char *format, va_list ap)
{
	va_list ap_copy;
	int len;
	char *str;
	
	// Make a copy of ap for the second vsnprintf call
	va_copy(ap_copy, ap);
	
	// Calculate required length
	len = vsnprintf(NULL, 0, format, ap);
	if (len < 0) {
		*ret = NULL;
		va_end(ap_copy);
		return -1;
	}
	
	// Allocate memory
	str = malloc(len + 1);
	if (!str) {
		*ret = NULL;
		va_end(ap_copy);
		return -1;
	}
	
	// Format the string
	len = vsnprintf(str, len + 1, format, ap_copy);
	va_end(ap_copy);
	
	if (len < 0) {
		free(str);
		*ret = NULL;
		return -1;
	}
	
	*ret = str;
	return len;
}

/*
 * BSD-style fgetln - get a line from a stream (without newline processing)
 */
char *fgetln(FILE *fp, size_t *len)
{
	static char *line_buffer = NULL;
	static size_t buffer_size = 0;
	size_t pos = 0;
	int c;
	
	if (!fp || fp->is_special) {
		if (len) *len = 0;
		return NULL;
	}
	
	// Start with a reasonable buffer size
	if (!line_buffer) {
		buffer_size = 128;
		line_buffer = malloc(buffer_size);
		if (!line_buffer) {
			if (len) *len = 0;
			return NULL;
		}
	}
	
	// Read characters until newline or EOF
	while ((c = fgetc(fp)) != EOF) {
		// Expand buffer if needed
		if (pos >= buffer_size - 1) {
			buffer_size *= 2;
			char *new_buffer = realloc(line_buffer, buffer_size);
			if (!new_buffer) {
				if (len) *len = 0;
				return NULL;
			}
			line_buffer = new_buffer;
		}
		
		line_buffer[pos++] = c;
		
		// Stop at newline (but include it in the result)
		if (c == '\n') {
			break;
		}
	}
	
	if (pos == 0) {
		if (len) *len = 0;
		return NULL;  // EOF with no data
	}
	
	if (len) *len = pos;
	return line_buffer;
}

/*
 * BSD-style fpurge - discard any buffered data
 */
int fpurge(FILE *fp)
{
	if (!fp || fp->is_special) {
		return 0;  // Nothing to purge for special streams
	}
	
	// In our VFS-based implementation, there's no buffering to purge
	// This is mainly a compatibility function
	return 0;
}

/*
 * BSD-style setbuffer - set buffer for stream
 */
void setbuffer(FILE *fp, char *buf, int size)
{
	// In our simple implementation, we don't support custom buffering
	// This is mainly for compatibility
	(void)fp;
	(void)buf;
	(void)size;
}

/*
 * BSD-style setlinebuf - set line buffering
 */
void setlinebuf(FILE *fp)
{
	// In our simple implementation, we don't support different buffering modes
	// This is mainly for compatibility
	(void)fp;
}

/*
 * getdelim - read delimited strings (basis for getline)
 */
ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
	char *line;
	size_t len = 0;
	size_t capacity;
	int c;
	
	if (!lineptr || !n || !stream) {
		return -1;
	}
	
	// Initialize buffer if needed
	if (!*lineptr || *n == 0) {
		capacity = 128;
		line = malloc(capacity);
		if (!line) {
			return -1;
		}
		*lineptr = line;
		*n = capacity;
	} else {
		line = *lineptr;
		capacity = *n;
	}
	
	// Read characters until delimiter or EOF
	while ((c = fgetc(stream)) != EOF) {
		// Expand buffer if needed
		if (len >= capacity - 1) {
			capacity *= 2;
			char *new_line = realloc(line, capacity);
			if (!new_line) {
				return -1;
			}
			line = new_line;
			*lineptr = line;
			*n = capacity;
		}
		
		line[len++] = c;
		
		// Stop at delimiter
		if (c == delim) {
			break;
		}
	}
	
	if (len == 0 && c == EOF) {
		return -1;  // EOF with no data
	}
	
	// Null-terminate
	line[len] = '\0';
	
	return len;
}

int fflush(FILE *fp)
{
	if (!fp || fp->is_special) {
		return 0;  // Nothing to flush for special streams
	}
	
	// VFS doesn't have flush implemented yet
	// In a full implementation, this would sync buffers to disk
	return 0;
}

int fputc(int ch, FILE *fp)
{
	if (!fp) {
		return EOF;
	}
	
	if (fp == stdin) {
		return EOF;  // Can't write to stdin
	}
	
	if (fp == stderr || fp == stdout) {
		putchar(ch);
		return ch;
	}
	
	// Write single character using VFS
	char c = (char)ch;
	ssize_t result = vfs_write(fp->vfs_fd, &c, 1);
	return (result == 1) ? ch : EOF;
}

int fputs(const char *buffer, FILE *fp)
{
	if (!fp || !buffer) {
		return EOF;
	}
	
	if (fp == stdin) {
		return EOF;  // Can't write to stdin
	}
	
	if (fp == stderr || fp == stdout) {
		puts(buffer);
		return 0;
	}
	
	// Write string using VFS
	size_t len = strlen(buffer);
	ssize_t result = vfs_write(fp->vfs_fd, buffer, len);
	return (result == len) ? 0 : EOF;
}

int fgetc(FILE *fp)
{
	char c;
	if (fp == stderr || fp == stdout)
	{
		return 0;
	}
	
	// Check if we successfully read a character
	unsigned int items_read = fread(&c, 1, 1, fp);
	if (items_read == 0) {
		return EOF;  // No data read, return EOF
	}
	
	return (int)(unsigned char)c;  // Cast to unsigned char first to handle 0xFF correctly
}

int feof(FILE *fp)
{
	if (!fp || fp->is_special) {
		return 0;  // Special streams never reach EOF
	}
	
	// VFS doesn't have EOF checking implemented yet
	// In a full implementation, this would check end-of-file status
	return 0;
}

unsigned int fgetsize(FILE *fp)
{
	if (!fp || fp->is_special) {
		return 0;
	}
	
	// Use VFS to get file size
	return vfs_fsize(fp->vfs_fd);
}
void clearerr(FILE *fp)
{
	// VFS doesn't have error state tracking implemented yet
	// In a full implementation, this would clear error flags
	return;
}

int getline(char **lineptr, size_t *n, FILE *fp)
{
	if ((*lineptr) == NULL)
		(*lineptr) = malloc(1024);
	memset((*lineptr), 0, 1024);
	int count;
	int size = 1024;
	int c = 0;
	for (count = 0; c != '\n' && count < size - 1; count++)
	{
		c = fgetc(fp);
		if (fp == stdin)
		{
			putchar(c);
		}
		int eof = feof(fp);
		if (eof)
		{
			if (count == 0)
				return -1;
			break;
		}

		(*lineptr)[count] = (char)c;
	}

	(*lineptr)[count] = '\0';
	if (n)
	{
		*n = count;
	}
	return count;
}
