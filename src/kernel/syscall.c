// syscall.c -- Defines the implementation of a system call system.
//              Written for JamesM's kernel development tutorials.

#include "syscall.h"
#include "isr.h"
#include "amp.h"
#include "graphics.h"
#include "kheap.h"
#include "string.h"
#include "stdio.h"

// Forward declaration to avoid circular include
int os_create_elf_process(void *elf_data, size_t elf_size);
int os_exec_process(int pid);
int os_exit_process(int exit_code);

// External assembly function
extern int fasm_main(int argc, char **argv);

// Wrapper for fasm syscall
int os_fasm(int argc, char **argv) {
    // Debug: print FASM arguments and current working directory
    extern char g_current_working_directory[];
    extern void serial_puts(const char *msg);
    
    serial_puts("FASM: Called with argc=");
    // Simple number to string for argc
    char argc_str[16];
    sprintf(argc_str, "%d", argc);
    serial_puts(argc_str);
    serial_puts("\r\n");
    
    for (int i = 0; i < argc; i++) {
        serial_puts("FASM: argv[");
        char i_str[16];
        sprintf(i_str, "%d", i);
        serial_puts(i_str);
        serial_puts("] = ");
        serial_puts(argv[i]);
        serial_puts("\r\n");
    }
    
    serial_puts("FASM: Current working directory = ");
    serial_puts(g_current_working_directory);
    serial_puts("\r\n");
    
    return fasm_main(argc, argv);
}

// Wrapper for virtual memory syscalls
void *os_virtual_alloc(uint32_t address, size_t size) {
    void *result = virtual_alloc(address, size);
    return result;
}

int os_virtual_free(void *ptr) {
    virtual_free(ptr);
    return 0;
}

// AMP syscall wrappers
int os_amp_is_initialized() {
    return amp_is_initialized() ? 1 : 0;
}

int os_amp_get_cpu_count() {
    return amp_get_cpu_count();
}

int os_amp_print_status() {
    amp_print_status();
    return 0;
}

static void syscall_handler(registers_t *regs);
#define ZX86_SYSCALL_GETC (1)
#define ZX86_SYSCALL_LS (4)

DEFN_SYSCALL1(os_puts, 0, const char *);
DEFN_SYSCALL1(os_getc, 1, int);
DEFN_SYSCALL1(os_putc, 2, int);
DEFN_SYSCALL0(os_asctime, 3);
DEFN_SYSCALL2(os_ls, 4, int, char **);
DEFN_SYSCALL2(os_rmdir, 5, int, char **);
DEFN_SYSCALL2(os_mkdir, 6, int, char **);
DEFN_SYSCALL2(os_cd, 7, int, char **);
DEFN_SYSCALL2(os_getcwd, 8, char *, int);
DEFN_SYSCALL2(os_chdrive, 9, int, char **);
DEFN_SYSCALL2(os_fasm, 10, int, char **);
DEFN_SYSCALL0(os_fork, 11);
DEFN_SYSCALL2(os_create_elf_process, 12, void*, size_t);
DEFN_SYSCALL1(os_exec_process, 13, int);
DEFN_SYSCALL1(os_exit_process, 14, int);
DEFN_SYSCALL2(os_virtual_alloc, 15, uint32_t, size_t);
DEFN_SYSCALL1(os_virtual_free, 16, void*);
DEFN_SYSCALL0(os_amp_is_initialized, 17);
DEFN_SYSCALL0(os_amp_get_cpu_count, 18);
DEFN_SYSCALL0(os_amp_print_status, 19);
void *syscalls[20] =
    {
        &os_puts,
        &os_getc,
        &os_putc,
        &os_asctime,
        &os_ls,
        &os_rmdir,
        &os_mkdir,
        &os_cd,
        &os_getcwd,
        &os_chdrive,
        &os_fasm,
        &os_fork,
        &os_create_elf_process,
        &os_exec_process,
        &os_exit_process,
        &os_virtual_alloc,
        &os_virtual_free,
        &os_amp_is_initialized,
        &os_amp_get_cpu_count,
        &os_amp_print_status};
uint32_t num_syscalls = 20;

void initialise_syscalls()
{
    // Register our syscall handler.
    register_interrupt_handler(0x80, &syscall_handler);
}

void syscall_handler(registers_t *regs)
{
    // Firstly, check if the requested syscall number is valid.
    // The syscall number is found in EAX.
    if (regs->eax >= num_syscalls)
        return;

    // Get the required syscall location.
    void *location = syscalls[regs->eax];

    // We don't know how many parameters the function wants, so we just
    // push them all onto the stack in the correct order. The function will
    // use all the parameters it wants, and we can pop them all back off afterwards.
    int ret;
    if (regs->eax == 1)
    {
        enable_interrupts();
        regs->eax = os_getc(0);
    }

    else
    {
        asm volatile(" \
            push %1; \
            push %2; \
            push %3; \
            push %4; \
            push %5; \
            call *%6; \
            pop %%ebx; \
            pop %%ebx; \
            pop %%ebx; \
            pop %%ebx; \
            pop %%ebx; \
            " : "=a"(ret) : "r"(regs->edi), "r"(regs->esi), "r"(regs->edx), "r"(regs->ecx), "r"(regs->ebx), "r"(location));
        regs->eax = ret;
    }
}
