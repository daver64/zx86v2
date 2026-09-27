//
// task.h - Defines the structures and prototypes needed to multitask.
//          Written for JamesM's kernel development tutorials.
//

#ifndef TASK_H
#define TASK_H

#include "common.h"
#include "paging.h"
#include "elf-32.h"

#define KERNEL_STACK_SIZE (0x10000) // Use a 2kb kernel stack.

// This structure defines a 'task' - a process.
typedef struct task
{
    int id;                           // Process ID.
    uint32_t esp, ebp;                // Stack and base pointers.
    uint32_t eip;                     // Instruction pointer.
    page_directory_t *page_directory; // Page directory.
    uint32_t kernel_stack;            // Kernel stack location.
    process_memory_layout_t *memory_layout; // Process memory layout for user space
    void *kernel_code_copy;           // Copy of ELF code in kernel memory for cross-CPU execution
    struct task *next;                // The next task in a linked list.
} task_t;

// Initialises the tasking system.
void initialise_tasking();

// Called by the timer hook, this changes the running process.
void switch_task();
// Forks the current process, spawning a new one with a different
// memory space.
int os_fork();

// Creates a new process from COM file data
int create_com_process(void *com_data, size_t com_size);

// Creates a new process from ELF executable data
int os_create_elf_process(void *elf_data, size_t elf_size);

// Creates a new process with isolated page directory
task_t *create_process_with_layout(void);

// Causes the current process' stack to be forcibly moved to a new location.
void move_stack(void *new_stack_start, uint32_t size);

// Returns the pid of the current process.
int getpid();

// AMP Process execution functions
task_t *get_task_by_pid(int pid);
int test_process_runner(int *pid_ptr);
int amp_exec_process(int pid, int wait_for_completion);

// Process cleanup function
void cleanup_process(task_t *task);

#endif
