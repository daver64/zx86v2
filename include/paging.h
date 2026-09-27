// paging.h -- Defines the interface for and structures relating to paging.
//             Written for JamesM's kernel development tutorials.

#ifndef PAGING_H
#define PAGING_H

#include "common.h"
#include "isr.h"

typedef struct page
{
    uint32_t present    : 1;   // Page present in memory
    uint32_t rw         : 1;   // Read-only if clear, readwrite if set
    uint32_t user       : 1;   // Supervisor level only if clear
    uint32_t accessed   : 1;   // Has the page been accessed since last refresh?
    uint32_t dirty      : 1;   // Has the page been written to since last refresh?
    uint32_t unused     : 7;   // Amalgamation of unused and reserved bits
    uint32_t frame      : 20;  // Frame address (shifted right 12 bits)
} page_t;

typedef struct page_table
{
    page_t pages[1024];
} page_table_t;

typedef struct page_directory
{
    /**
       Array of pointers to pagetables.
    **/
    page_table_t *tables[1024];
    /**
       Array of pointers to the pagetables above, but gives their *physical*
       location, for loading into the CR3 register.
    **/
    uint32_t tablesPhysical[1024];

    /**
       The physical address of tablesPhysical. This comes into play
       when we get our kernel heap allocated and the directory
       may be in a different location in virtual memory.
    **/
    uint32_t physicalAddr;
} page_directory_t;

typedef struct process_memory_layout {
    uint32_t code_start;    // Where executable is loaded
    uint32_t code_end;      // End of code section
    uint32_t heap_start;    // Start of heap
    uint32_t heap_end;      // Current end of heap
    uint32_t stack_top;     // Top of stack (grows down)
    uint32_t stack_bottom;  // Bottom of stack
} process_memory_layout_t;

/**
   Sets up the environment, page directories etc and
   enables paging.
**/
void initialise_paging();

/**
   Causes the specified page directory to be loaded into the
   CR3 register.
**/
void switch_page_directory(page_directory_t *new);

/**
   Retrieves a pointer to the page required.
   If make == 1, if the page-table in which this page should
   reside isn't created, create it!
**/
page_t *get_page(uint32_t address, int make, page_directory_t *dir);

/**
   Handler for page faults.
**/
void page_fault(registers_t *regs);

/**
   Makes a copy of a page directory.
**/
page_directory_t *clone_directory(page_directory_t *src);

/**
   Creates a page directory for a new process with standard layout.
**/
page_directory_t *create_process_page_directory(void);

/**
   Cleans up a process page directory and frees all user memory.
**/
void cleanup_process_page_directory(page_directory_t *dir);

/**
   Simple, safe cleanup of process page directory.
**/
void simple_cleanup_process_page_directory(page_directory_t *dir);

/**
   Check if it's safe to free a page directory using kfree().
**/
int is_safe_to_free_page_directory(page_directory_t *dir);

/**
   Process deferred cleanup of queued page tables safely.
**/
void process_deferred_page_table_cleanup(void);

/**
   Timer-based deferred cleanup (called from timer interrupt).
**/
void timer_process_deferred_cleanup(void);

/**
   Initialize the deferred cleanup system.
**/
void init_deferred_cleanup(void);

/**
   Loads a COM file into a process-specific memory space.
**/
int load_com_file_to_process(page_directory_t *dir, void *com_data, size_t com_size, process_memory_layout_t *layout);

/**
   Sets up memory layout for COM files at the correct virtual address.
**/
int setup_com_memory_layout(page_directory_t *dir, process_memory_layout_t *layout);

/**
   Maps a range of virtual addresses to physical frames.
**/
int map_memory_range(page_directory_t *dir, uint32_t virt_start, uint32_t virt_end, int is_user, int is_writeable);

/**
   Expands process heap within safe boundaries.
**/
int expand_process_heap(page_directory_t *dir, process_memory_layout_t *layout, uint32_t new_size);

/**
   Validates if an address is within process boundaries.
**/
int validate_process_address(process_memory_layout_t *layout, uint32_t addr, uint32_t size);

/**
   Gets memory usage statistics for a process.
**/
void get_process_memory_stats(process_memory_layout_t *layout, uint32_t *code_size, uint32_t *heap_size, uint32_t *stack_size);

/**
   Sets up memory layout for ELF files at the standard address.
**/
int setup_elf_memory_layout(page_directory_t *dir, process_memory_layout_t *layout);

/**
   Loads an ELF program into a process-specific memory space.
**/
int load_elf_program_to_process(page_directory_t *dir, void *elf_data, size_t elf_size, process_memory_layout_t *layout);

void alloc_frame(page_t *page, int is_kernel, int is_writeable);
void free_frame(page_t *page);
void map_framebuffer(uint32_t framebuffer_addr, uint32_t size);
uint32_t get_kernel_physical();
uint32_t get_page_frame_count();
uint32_t get_kernel_heap();
uint32_t get_kernel_heap_end();
uint32_t get_libc_heap_start();
uint32_t get_libc_heap_end();
uint32_t get_libc_heap_size();

#endif
