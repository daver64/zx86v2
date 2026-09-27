// fasm_io.c -- File I/O wrappers for FASM integration
// Provides only the missing libc-style functions for FASM's external dependencies

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"

// Note: fasm_malloc, fasm_free, and getenv already exist in other files
// Only provide the file I/O functions that FASM needs

// File I/O wrappers - these map directly to ZX86's stdio functions
FILE* fasm_fopen(const char* filename, const char* mode) {
    // Debug output
    extern void serial_puts(const char *msg);
    serial_puts("FASM_FOPEN: Attempting to open file: ");
    serial_puts(filename);
    serial_puts(" with mode: ");
    serial_puts(mode);
    serial_puts("\r\n");
    
    FILE* result = fopen(filename, mode);
    
    if (result) {
        serial_puts("FASM_FOPEN: Successfully opened file\r\n");
    } else {
        serial_puts("FASM_FOPEN: Failed to open file\r\n");
    }
    
    return result;
}

int fasm_fclose(FILE* fp) {
    // Sync mode is set globally at startup, no need to toggle it
    return fclose(fp);
}

size_t fasm_fread(void* buffer, size_t size, size_t count, FILE* fp) {
    return fread(buffer, size, count, fp);
}

size_t fasm_fwrite(const void* buffer, size_t size, size_t count, FILE* fp) {
    return fwrite((void*)buffer, size, count, fp);
}

int fasm_fseek(FILE* fp, long offset, int whence) {
    return fseek(fp, offset, whence);
}

long fasm_ftell(FILE* fp) {
    return (long)ftell(fp);
}

// Time function wrapper
time_t fasm_time(time_t* timer) {
    // Simple time implementation - return a constant for now
    // In a real system, this would return actual system time
    time_t current_time = 1000000; // Dummy timestamp
    if (timer) {
        *timer = current_time;
    }
    return current_time;
}

// Exit function wrapper
void fasm_exit(int status) {
    // For kernel-embedded FASM, we can't actually exit
    // Just return to caller
    return;
}

// Console output for FASM's display functions
void libc_write(int fd, const void* buf, size_t count) {
    if (fd == 1 || fd == 2) { // stdout or stderr
        const char* str = (const char*)buf;
        for (size_t i = 0; i < count; i++) {
            putchar(str[i]);
        }
    }
}