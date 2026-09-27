// amp.c -- Asymmetric Multiprocessing implementation for zx86v2
// Function offloading to secondary CPUs

#include "amp.h"
#include "task.h"
#include "paging.h"
#include "common.h"
#include "timer.h"
#include "graphics.h"
#include "paging.h"
#include "stdio.h"
#include "string.h"
#include "stddef.h"
#include "descriptor_tables.h"

// External symbols from ap_startup.asm
extern uint8_t ap_startup_begin[];
extern uint8_t ap_startup_end[];

// Forward declarations
void ipi_function_ready_handler(registers_t *regs);
extern void register_interrupt_handler(uint8_t n, isr_t handler);

// Global variables
volatile amp_workspace_t *amp_workspace = NULL;
bool amp_initialized = false;
uint32_t lapic_base_address = 0xFEE00000;  // Default LAPIC address

// Shared memory globals
static void* shared_memory_base = NULL;
bool shared_memory_initialized = false;

// CPU stack size for secondary CPUs (64KB each)
#define CPU_STACK_SIZE 0x10000

// Secondary CPU stacks
static uint32_t cpu_stacks[MAX_CPUS];

// Round-robin CPU selection
static uint8_t next_cpu_id = 1;

// Forward declarations
static bool detect_apic(void);
static bool start_secondary_cpu(uint8_t cpu_id);
static void setup_cpu_stacks(void);
static uint8_t select_compute_cpu(void);
static uint32_t find_free_function_slot(void);
static void cleanup_completed_functions(void);
uint32_t apic_read(uint32_t reg);
void apic_send_ipi(uint8_t target_cpu, uint8_t vector);

/**
 * Initialize the AMP system
 */
bool amp_init(void) {
    extern void serial_puts(const char *msg);
    
    if (amp_initialized) {
        return true;
    }
    
    // Detect and initialize APIC
    if (!detect_apic()) {
        printf("AMP: ERROR - No APIC detected\n");
        return false;
    }

    // Now that LAPIC is mapped, we can initialize it
    if (!apic_init()) {
        printf("AMP: ERROR - Failed to initialize APIC\n");
        return false;
    }
    
    // Allocate shared workspace in kernel space accessible to all CPUs
    amp_workspace = (volatile amp_workspace_t*)kmalloc_a(sizeof(amp_workspace_t));
    if (!amp_workspace) {
        printf("AMP: ERROR - Failed to allocate workspace\n");
        return false;
    }
    printf("AMP: Workspace allocated at virtual 0x%08X\n", (uint32_t)amp_workspace);
    
    // Initialize workspace
    memset((void*)amp_workspace, 0, sizeof(amp_workspace_t));
    amp_workspace->next_task_id = 1;
    amp_workspace->queue_head = 0;
    amp_workspace->queue_tail = 0;
    
    // Initialize CPU info - exclude CPU0 from AMP system
    // CPU0 handles syscalls directly and doesn't participate in AMP work distribution
    for (int i = 0; i < MAX_CPUS; i++) {
        if (i == 0) {
            // CPU0 is not part of AMP workspace - skip initialization
            memset((void*)&amp_workspace->cpu_info[i], 0, sizeof(cpu_info_t));
            continue;
        }
        
        amp_workspace->cpu_info[i].cpu_id = i;
        amp_workspace->cpu_info[i].online = 0;  // Secondary CPUs start offline
        amp_workspace->cpu_info[i].role = COMPUTE_CPU_1 + (i - 1);
        amp_workspace->cpu_info[i].status = 0;  // IDLE
        amp_workspace->cpu_info[i].functions_executed = 0;
        amp_workspace->cpu_info[i].last_heartbeat = 0;
    }
    
    // Register IPI handler for function ready notifications
    extern void register_interrupt_handler(uint8_t n, isr_t handler);
    extern void idt_set_gate_public(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
    
    // First register the software handler
    register_interrupt_handler(IPI_FUNCTION_READY, &ipi_function_ready_handler);
    
    // Then set up the IDT entry to actually call our handler
    extern void isr128();  // Generic ISR stub
    idt_set_gate_public(IPI_FUNCTION_READY, (uint32_t)isr128, 0x08, 0x8E);
    
    printf("AMP: Registered IPI handler for vector 0x%02X\n", IPI_FUNCTION_READY);
    
    // Verify the handler was registered on CPU 0
    extern idt_entry_t idt_entries[256];
    idt_entry_t *entry = &idt_entries[IPI_FUNCTION_READY];
    uint32_t handler_addr = entry->base_lo | (entry->base_hi << 16);
    printf("AMP: CPU 0 - IDT entry 0x%02X: base=0x%08X, flags=0x%02X\n", 
           IPI_FUNCTION_READY, handler_addr, entry->flags);
    
    // Mark AMP as initialized so secondary CPUs can proceed
    amp_initialized = true;

    printf("AMP: Basic initialization complete. Secondary CPUs will be started after shared memory setup.\n");
    
    return true;
}

/**
 * Start secondary CPUs after shared memory is ready
 */
bool amp_start_secondary_cpus(void) {
    if (!amp_initialized) {
        printf("AMP: ERROR - AMP not initialized before starting secondary CPUs\n");
        return false;
    }

    printf("AMP: Setting up CPU stacks...\n");
    setup_cpu_stacks();

    // Start all secondary CPUs with proper synchronization
    // Limit to MAX_BOOT_CPUS to avoid trying to start CPUs that don't exist
    int cpus_started = 0;
    int max_cpus_to_start = (MAX_BOOT_CPUS < MAX_CPUS) ? MAX_BOOT_CPUS : MAX_CPUS;
    
    printf("AMP: Starting secondary CPUs - attempting to start %d secondary CPUs (max available: %d)\n", 
           max_cpus_to_start - 1, MAX_CPUS - 1);
    
    for (int cpu_id = 1; cpu_id < max_cpus_to_start; cpu_id++) {
        printf("AMP: Starting CPU %d...\n", cpu_id);
        
        // Clear heartbeat before starting
        amp_workspace->cpu_info[cpu_id].last_heartbeat = 0;
        
        if (start_secondary_cpu(cpu_id)) {
            printf("AMP: CPU %d startup sequence completed\n", cpu_id);
            
            // Wait for CPU to reach main loop and send heartbeat
            printf("AMP: Waiting for CPU %d heartbeat...\n", cpu_id);
            uint32_t timeout = 0;
            while (amp_workspace->cpu_info[cpu_id].last_heartbeat == 0 && timeout < 100000000) {
                timeout++;
                if (timeout % 10000000 == 0) {
                    printf("AMP: Still waiting for CPU %d heartbeat...\n", cpu_id);
                }
            }
            
            if (amp_workspace->cpu_info[cpu_id].last_heartbeat > 0) {
                printf("AMP: CPU %d is alive and running\n", cpu_id);
                amp_workspace->cpu_info[cpu_id].online = 1;
                cpus_started++;
                
                // Short delay to let CPU settle before starting next one
                for (volatile int delay = 0; delay < 5000000; delay++) {
                    // Brief settling delay
                }
            } else {
                printf("AMP: ERROR - CPU %d failed to send heartbeat (timeout)\n", cpu_id);
            }
        } else {
            printf("AMP: ERROR - Failed to start CPU %d\n", cpu_id);
        }
    }
    
    printf("AMP: Started %d out of %d secondary CPUs\n", cpus_started, MAX_CPUS - 1);
    serial_puts("SERIAL: AMP - initialization complete\n");
    amp_print_status();
    
    return true;
}

/**
 * Shutdown AMP system
 */
void amp_shutdown(void) {
    if (!amp_initialized) {
        return;
    }
    
    printf("AMP: Shutting down secondary CPUs...\n");
    
    // Send shutdown IPI to all secondary CPUs
    for (int i = 1; i < MAX_CPUS; i++) {
        if (amp_workspace->cpu_info[i].online) {
            apic_send_ipi(i, IPI_SHUTDOWN);
            amp_workspace->cpu_info[i].online = 0;
        }
    }
    
    // Give CPUs time to shutdown (simple delay using tick counter)
    uint32_t start_time = get_tick_count();
    while ((get_tick_count() - start_time) < 10) {  // ~100ms delay
        pause_cpu();
    }
    
    amp_initialized = false;
    printf("AMP: Shutdown complete\n");
}

/**
 * Execute function synchronously on available compute CPU
 */
int amp_execute_function(void *func, void *args, size_t args_size, 
                        void *result, size_t result_size) {
    
    extern void serial_puts(const char *msg);
    
    if (!amp_initialized) {
        serial_puts("SERIAL: AMP execute - not initialized\n");
        return AMP_ERROR_NOT_INIT;
    }
    
    if (!func || args_size > SHARED_BUFFER_SIZE || result_size > SHARED_BUFFER_SIZE) {
        serial_puts("SERIAL: AMP execute - invalid arguments\n");
        return AMP_ERROR_INVALID_ARGS;
    }
    
    // Find available compute CPU using round-robin
    uint8_t target_cpu = select_compute_cpu();
    if (!amp_workspace->cpu_info[target_cpu].online) {
        serial_puts("SERIAL: AMP execute - target CPU not online\n");
        return AMP_ERROR_INVALID_CPU;
    }
    
    // Find free slot in function queue
    uint32_t slot = find_free_function_slot();
    if (slot == (uint32_t)-1) {
        return AMP_ERROR_QUEUE_FULL;
    }
    
    // Prepare function call
    volatile cpu_function_call_t *call = &amp_workspace->function_queue[slot];
    call->function_ptr = func;
    call->args_size = args_size;
    call->result_size = result_size;
    call->task_id = amp_workspace->next_task_id++;
    call->start_time = amp_get_timestamp();
    call->cpu_assigned = target_cpu;
    
    // Copy arguments to shared buffer
    if (args && args_size > 0) {
        memcpy((void*)amp_workspace->shared_args_buffer[slot], args, args_size);
    }
    call->args = (void*)amp_workspace->shared_args_buffer[slot];
    call->result = (void*)amp_workspace->shared_result_buffer[slot];
    
    serial_puts("SERIAL: AMP execute - sending IPI to target CPU\n");
    
    // Mark as pending and wake target CPU
    call->status = FUNCTION_PENDING;
    apic_send_ipi(target_cpu, IPI_FUNCTION_READY);
    
    serial_puts("SERIAL: AMP execute - waiting for completion\n");
    
    // Wait for completion (synchronous) - but also handle syscall requests
    uint32_t timeout = amp_get_timestamp() + FUNCTION_TIMEOUT_MS;
    uint32_t last_status = call->status;
    while (call->status != 3 && // FUNCTION_COMPLETE
           call->status != 4 && // FUNCTION_ERROR
           amp_get_timestamp() < timeout) {
        
        // Handle syscall requests from secondary CPUs while waiting
        amp_handle_syscall_requests();
        
        // Debug status changes
        if (call->status != last_status) {
            char debug_msg[128];
            sprintf(debug_msg, "SERIAL: AMP execute - status changed from %d to %d\n", last_status, call->status);
            serial_puts(debug_msg);
            last_status = call->status;
        }
        
        pause_cpu();
    }
    
    // Add debug to see what status we have after waiting (use numeric values)
    char debug_msg[128];
    sprintf(debug_msg, "SERIAL: AMP execute - wait loop finished, status=%d (3=COMPLETE, 4=ERROR)\n", call->status);
    serial_puts(debug_msg);
    
    // Check result
    if (call->status == 3) { // FUNCTION_COMPLETE
        serial_puts("SERIAL: AMP execute - function completed successfully\n");
        // Copy result back
        if (result && result_size > 0) {
            // Add debug output to check pointers before memcpy
            char ptr_debug[128];
            sprintf(ptr_debug, "SERIAL: AMP execute - copying result, result=0x%x, call->result=0x%x, size=%d\n", 
                    (uint32_t)result, (uint32_t)call->result, result_size);
            serial_puts(ptr_debug);
            memcpy(result, (void*)call->result, result_size);
        }
        call->status = FUNCTION_FREE;  // Mark slot as free
        return AMP_SUCCESS;
    } else if (call->status == 4) { // FUNCTION_ERROR
        serial_puts("SERIAL: AMP execute - function completed with error\n");
        call->status = FUNCTION_FREE;
        return AMP_ERROR_INVALID_ARGS;
    } else {
        serial_puts("SERIAL: AMP execute - function timed out\n");
        call->status = FUNCTION_FREE;
        return AMP_ERROR_TIMEOUT;
    }
}

/**
    
    // Find free slot in function queue
    uint32_t slot = find_free_function_slot();
    if (slot == (uint32_t)-1) {
        return AMP_ERROR_QUEUE_FULL;
    }
    
    // Prepare function call
    volatile cpu_function_call_t *call = &amp_workspace->function_queue[slot];
    call->function_ptr = func;
    call->args_size = args_size;
    call->result_size = result_size;
    call->task_id = amp_workspace->next_task_id++;
    call->start_time = amp_get_timestamp();
    call->cpu_assigned = target_cpu;
    
    // Copy arguments to shared buffer
    if (args && args_size > 0) {
        memcpy((void*)amp_workspace->shared_args_buffer[slot], args, args_size);
    }
    call->args = (void*)amp_workspace->shared_args_buffer[slot];
    call->result = (void*)amp_workspace->shared_result_buffer[slot];
    
    serial_puts("SERIAL: AMP execute - sending IPI to target CPU\n");
    
    // Mark as pending and wake target CPU
    call->status = FUNCTION_PENDING;
    apic_send_ipi(target_cpu, IPI_FUNCTION_READY);
    
    serial_puts("SERIAL: AMP execute - waiting for completion\n");
    
    // Wait for completion (synchronous) - but also handle syscall requests
    uint32_t timeout = amp_get_timestamp() + FUNCTION_TIMEOUT_MS;
    uint32_t last_status = call->status;
    while (call->status != 3 && // FUNCTION_COMPLETE
           call->status != 4 && // FUNCTION_ERROR
           amp_get_timestamp() < timeout) {
        
        // Handle syscall requests from secondary CPUs while waiting
        amp_handle_syscall_requests();
        
        // Debug status changes
        if (call->status != last_status) {
            char debug_msg[128];
            sprintf(debug_msg, "SERIAL: AMP execute - status changed from %d to %d\n", last_status, call->status);
            serial_puts(debug_msg);
            last_status = call->status;
        }
        
        pause_cpu();
    }
    
    // Add debug to see what status we have after waiting (use numeric values)
    char debug_msg[128];
    sprintf(debug_msg, "SERIAL: AMP execute - wait loop finished, status=%d (3=COMPLETE, 4=ERROR)\n", call->status);
    serial_puts(debug_msg);
    
    // Check result
    if (call->status == 3) { // FUNCTION_COMPLETE
        serial_puts("SERIAL: AMP execute - function completed successfully\n");
        // Copy result back
        if (result && result_size > 0) {
            // Add debug output to check pointers before memcpy
            char debug_msg[128];
            sprintf(debug_msg, "SERIAL: AMP execute - about to copy result: src=0x%08x, dst=0x%08x, size=%d\n", 
                    (uint32_t)call->result, (uint32_t)result, result_size);
            serial_puts(debug_msg);
            
            memcpy(result, (void*)call->result, result_size);
            serial_puts("SERIAL: AMP execute - result copied back\n");
        }
        call->status = FUNCTION_FREE;  // Mark slot as free
        serial_puts("SERIAL: AMP execute - returning AMP_SUCCESS\n");
        return AMP_SUCCESS;
    } else if (amp_get_timestamp() >= timeout) {
        serial_puts("SERIAL: AMP execute - function timed out\n");
        call->status = FUNCTION_FREE;
        amp_workspace->total_timeouts++;
        return AMP_ERROR_TIMEOUT;
    } else {
        serial_puts("SERIAL: AMP execute - function error\n");
        call->status = FUNCTION_FREE;
        amp_workspace->total_errors++;
        return AMP_ERROR_INVALID_ARGS;
    }
}

/**
 * Execute function on a specific CPU
 */
int amp_execute_function_on_cpu(uint8_t cpu_id, void *func, void *args, 
                               size_t args_size, void *result, size_t result_size) {
    
    if (!amp_initialized) {
        return AMP_ERROR_NOT_INIT;
    }
    
    if (!func || args_size > SHARED_BUFFER_SIZE || result_size > SHARED_BUFFER_SIZE) {
        return AMP_ERROR_INVALID_ARGS;
    }
    
    if (cpu_id >= MAX_CPUS) {
        return AMP_ERROR_INVALID_CPU;
    }
    
    // Special handling for CPU0 - execute locally but don't use AMP workspace
    if (cpu_id == 0) {
        // Cast and execute function locally
        amp_function_t local_func = (amp_function_t)func;
        void *local_result = local_func(args);
        
        // Copy result back if needed
        if (result && result_size > 0 && local_result) {
            memcpy(result, local_result, result_size);
        }
        
        // CPU0 doesn't participate in AMP system - no counters to increment
        // If you need CPU0 execution tracking, implement separate counters
        return AMP_SUCCESS;
    }
    
    // For other CPUs, check if they're online
    if (!amp_workspace->cpu_info[cpu_id].online) {
        return AMP_ERROR_INVALID_CPU;
    }
    
    // Find free slot in function queue
    uint32_t slot = find_free_function_slot();
    if (slot == (uint32_t)-1) {
        return AMP_ERROR_QUEUE_FULL;
    }
    
    // Prepare function call for secondary CPU
    volatile cpu_function_call_t *call = &amp_workspace->function_queue[slot];
    call->function_ptr = func;
    call->args_size = args_size;
    call->result_size = result_size;
    call->task_id = amp_workspace->next_task_id++;
    call->start_time = amp_get_timestamp();
    call->cpu_assigned = cpu_id;
    
    // Copy arguments to shared buffer
    if (args && args_size > 0) {
        memcpy((void*)amp_workspace->shared_args_buffer[slot], args, args_size);
    }
    call->args = (void*)amp_workspace->shared_args_buffer[slot];
    call->result = (void*)amp_workspace->shared_result_buffer[slot];
    
    // Mark as pending and wake target CPU
    call->status = FUNCTION_PENDING;
    
    // Debug: Log that we're sending IPI
    serial_puts("SERIAL: AMP - Sending IPI to wake target CPU\n");
    
    apic_send_ipi(cpu_id, IPI_FUNCTION_READY);
    
    // Debug: Log that IPI was sent
    char debug_msg[128];
    sprintf(debug_msg, "SERIAL: AMP - IPI sent to CPU %d, waiting for response\n", cpu_id);
    serial_puts(debug_msg);
    
    // Wait for completion
    uint32_t timeout = amp_get_timestamp() + FUNCTION_TIMEOUT_MS;
    uint32_t last_status_check = 0;
    while (call->status != FUNCTION_COMPLETE && 
           call->status != FUNCTION_ERROR &&
           amp_get_timestamp() < timeout) {
        
        // Debug: Periodically log the status
        uint32_t current_time = amp_get_timestamp();
        if (current_time - last_status_check > 1000) { // Every 1 second
            sprintf(debug_msg, "SERIAL: AMP - Waiting... status=%d, time=%d\n", call->status, current_time);
            serial_puts(debug_msg);
            last_status_check = current_time;
        }
        
        // Handle syscall requests from secondary CPUs while waiting
        amp_handle_syscall_requests();
        pause_cpu();
    }
    
    // Check result
    if (call->status == FUNCTION_COMPLETE) {
        // Copy result back
        if (result && result_size > 0) {
            memcpy(result, (void*)call->result, result_size);
        }
        call->status = FUNCTION_FREE;
        serial_puts("SERIAL: AMP - Function completed successfully\n");
        return AMP_SUCCESS;
    } else if (call->status == FUNCTION_ERROR) {
        call->status = FUNCTION_FREE;
        serial_puts("SERIAL: AMP - Function completed with error\n");
        return AMP_ERROR_INVALID_ARGS;
    } else {
        // Timeout
        sprintf(debug_msg, "SERIAL: AMP - TIMEOUT! Final status: %d\n", call->status);
        serial_puts(debug_msg);
        call->status = FUNCTION_FREE;
        return AMP_ERROR_TIMEOUT;
    }
}

/**
 * Execute function asynchronously - returns task ID for later checking
 */
int amp_execute_function_async(void *func, void *args, size_t args_size) {
    if (!amp_initialized) {
        return AMP_ERROR_NOT_INIT;
    }
    
    if (!func || args_size > SHARED_BUFFER_SIZE) {
        return AMP_ERROR_INVALID_ARGS;
    }
    
    // Find available compute CPU using round-robin
    uint8_t target_cpu = select_compute_cpu();
    if (!amp_workspace->cpu_info[target_cpu].online) {
        return AMP_ERROR_INVALID_CPU;
    }
    
    // Find free slot
    uint32_t slot = find_free_function_slot();
    if (slot == (uint32_t)-1) {
        return AMP_ERROR_QUEUE_FULL;
    }
    
    // Prepare function call
    volatile cpu_function_call_t *call = &amp_workspace->function_queue[slot];
    call->function_ptr = func;
    call->args_size = args_size;
    call->result_size = 0;  // No result expected for async
    call->task_id = amp_workspace->next_task_id++;
    call->start_time = amp_get_timestamp();
    call->cpu_assigned = target_cpu;
    
    // Copy arguments
    if (args && args_size > 0) {
        memcpy((void*)amp_workspace->shared_args_buffer[slot], args, args_size);
    }
    call->args = (void*)amp_workspace->shared_args_buffer[slot];
    call->result = NULL;
    
    // Mark as pending and wake target CPU
    call->status = FUNCTION_PENDING;
    apic_send_ipi(target_cpu, IPI_FUNCTION_READY);
    
    return call->task_id;
}

/**
 * Check if async task is complete
 */
bool amp_is_task_complete(uint32_t task_id) {
    if (!amp_initialized) {
        return false;
    }
    
    for (int i = 0; i < MAX_CONCURRENT_FUNCTIONS; i++) {
        if (amp_workspace->function_queue[i].task_id == task_id) {
            return (amp_workspace->function_queue[i].status == FUNCTION_COMPLETE ||
                    amp_workspace->function_queue[i].status == FUNCTION_ERROR);
        }
    }
    return true;  // Task not found, assume complete
}

/**
 * Check if AMP system is initialized
 */
bool amp_is_initialized(void) {
    return amp_initialized;
}

/**
 * Get current CPU count
 */
uint8_t amp_get_cpu_count(void) {
    if (!amp_initialized) {
        return 1;
    }
    
    uint8_t count = 0;
    // Only check up to MAX_BOOT_CPUS since we won't start more than that
    int max_check = (MAX_BOOT_CPUS < MAX_CPUS) ? MAX_BOOT_CPUS : MAX_CPUS;
    for (int i = 0; i < max_check; i++) {
        if (amp_workspace->cpu_info[i].online) {
            count++;
        }
    }
    return count;
}

/**
 * Print AMP status
 */
void amp_print_status(void) {
    if (!amp_initialized) {
        printf("AMP: Not initialized\n");
        return;
    }
    
    if (!amp_workspace) {
        printf("AMP: Workspace not available\n");
        return;
    }
    
    printf("AMP Status:\n");
    printf("  CPUs online: %u/%u\n", amp_get_cpu_count(), MAX_CPUS);
    
    for (int i = 0; i < MAX_CPUS && i < 16; i++) {  // Limit to 16 to avoid potential overflow
        if (amp_workspace->cpu_info[i].online) {
            const char* role_name = "COMPUTE";
            if (i == 0) role_name = "MASTER";
            
            printf("  CPU %d: %s, %u functions executed\n", 
                   i, 
                   role_name,
                   amp_workspace->cpu_info[i].functions_executed);
        }
    }
    
    printf("  Total functions: %u, Errors: %u, Timeouts: %u\n",
           amp_workspace->total_functions_executed,
           amp_workspace->total_errors,
           amp_workspace->total_timeouts);
}

/**
 * Get current timestamp (using system timer)
 */
uint32_t amp_get_timestamp(void) {
    return get_tick_count() * 10;  // Convert to milliseconds
}

/**
 * CPU pause instruction for power saving
 */
void amp_cpu_pause(void) {
    pause_cpu();
}

// Internal helper functions

/**
 * Find a free slot in the function queue
 */
static uint32_t find_free_function_slot(void) {
    for (int i = 0; i < MAX_CONCURRENT_FUNCTIONS; i++) {
        if (amp_workspace->function_queue[i].status == FUNCTION_FREE) {
            return i;
        }
    }
    
    // Try to clean up completed functions
    cleanup_completed_functions();
    
    // Try again
    for (int i = 0; i < MAX_CONCURRENT_FUNCTIONS; i++) {
        if (amp_workspace->function_queue[i].status == FUNCTION_FREE) {
            return i;
        }
    }
    
    return (uint32_t)-1;  // No free slots
}

/**
 * Clean up completed function calls
 */
static void cleanup_completed_functions(void) {
    for (int i = 0; i < MAX_CONCURRENT_FUNCTIONS; i++) {
        volatile cpu_function_call_t *call = &amp_workspace->function_queue[i];
        
        if (call->status == FUNCTION_COMPLETE || call->status == FUNCTION_ERROR) {
            call->status = FUNCTION_FREE;
        }
        
        // Clean up timed out functions
        if (call->status == FUNCTION_PENDING || call->status == FUNCTION_RUNNING) {
            if (amp_get_timestamp() - call->start_time > FUNCTION_TIMEOUT_MS) {
                call->status = FUNCTION_FREE;
                amp_workspace->total_timeouts++;
            }
        }
    }
}

/**
 * Set up kernel stacks for secondary CPUs
 */
static void setup_cpu_stacks(void) {
    printf("AMP: Setting up CPU stacks...\n");
    
    // Use a safe area after the kernel heap but before shared memory
    // Kernel heap: 0xC0000000 - 0xC4000000 (64MB)
    // Shared memory: 0xF0000000
    // Safe area: 0xE0000000 - 0xEF000000 (240MB gap)
    uint32_t stack_area_base = 0xE0000000; 
    
    for (int i = 1; i < MAX_CPUS; i++) {
        // Allocate 64KB per CPU stack
        uint32_t stack_base = stack_area_base + (i * CPU_STACK_SIZE);
        
        // We need to manually map these pages since they're not in the normal heap
        extern page_directory_t *kernel_directory;
        extern page_directory_t *current_directory;
        extern void alloc_frame(page_t *page, int is_kernel, int is_writeable);
        extern page_t *get_page(uint32_t address, int make, page_directory_t *dir);
        
        // Map the stack pages in both kernel directory and current directory
        page_directory_t *directories[] = {kernel_directory, current_directory};
        for (int dir_idx = 0; dir_idx < 2; dir_idx++) {
            for (uint32_t addr = stack_base; addr < stack_base + CPU_STACK_SIZE; addr += 0x1000) {
                page_t *page = get_page(addr, 1, directories[dir_idx]);
                if (page && !page->present) {
                    alloc_frame(page, 1, 1); // kernel=1, writeable=1
                }
            }
        }
        
        // Clear the stack memory
        memset((void*)stack_base, 0, CPU_STACK_SIZE);
        
        // Point to top of stack (stack grows downward) 
        cpu_stacks[i] = stack_base + CPU_STACK_SIZE - 16;  // Leave some padding
        printf("AMP: CPU %d stack allocated - base: 0x%08X, top: 0x%08X\n", 
               i, stack_base, cpu_stacks[i]);
    }
    
    printf("AMP: CPU stack setup complete\n");
}

/**
 * Detect APIC presence
 */
static bool detect_apic(void) {
    uint32_t eax, ebx, ecx, edx;
    
    // Check CPUID for APIC support
    asm volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(1));
    
    if (!(edx & (1 << 9))) {
        printf("AMP: CPU does not support APIC\n");
        return false;
    }
    
    printf("AMP: APIC detected and supported\n");
    return true;
}

/**
 * Start a secondary CPU
 */
static bool start_secondary_cpu(uint8_t cpu_id) {
    char debug_msg[128];
    extern void serial_puts(const char *msg);
    extern page_directory_t *current_directory;
    
    if (cpu_id == 0 || cpu_id >= MAX_CPUS) {
        return false;
    }
    
    printf("AMP: Starting CPU %d...\n", cpu_id);
    
    // Copy AP startup code to 0x8000 (where STARTUP IPI will jump)
    uint32_t code_size = (uint32_t)(ap_startup_end - ap_startup_begin);
    printf("AMP: Copying %d bytes of AP startup code to 0x8000\n", code_size);
    
    if (code_size > 4096) {
        printf("AMP: ERROR - AP startup code too large (%d bytes)\n", code_size);
        return false;
    }
    
    // Copy the startup code to physical address 0x8000
    memcpy((void*)0x8000, ap_startup_begin, code_size);
    
    // Clear debug markers
    *((uint8_t*)0x8FFC) = 0x00;  // Debug marker 1
    *((uint8_t*)0x8FFD) = 0x00;  // Debug marker 2
    *((uint8_t*)0x8FFE) = 0x00;  // Debug marker 3
    *((uint8_t*)0x8FFF) = 0x00;  // Debug marker 4
    
    // Set up data that the AP startup code needs
    // We need to find the data locations within the copied startup code
    // The data is at the end of the startup code
    uint32_t *kernel_page_dir_ptr = (uint32_t*)(0x8000 + code_size - 16);  // Last 16 bytes
    uint32_t *secondary_main_ptr = (uint32_t*)(0x8000 + code_size - 12);   // Last 12 bytes  
    uint32_t *cpu_stack_ptr = (uint32_t*)(0x8000 + code_size - 8);         // Last 8 bytes
    uint32_t *cpu_id_ptr = (uint32_t*)(0x8000 + code_size - 4);            // Last 4 bytes
    
    // Physical address of kernel page directory
    extern page_directory_t *kernel_directory;
    *kernel_page_dir_ptr = (uint32_t)kernel_directory->physicalAddr;
    
    // Virtual address of secondary_cpu_main function
    extern void secondary_cpu_main(uint8_t cpu_id);
    *secondary_main_ptr = (uint32_t)secondary_cpu_main;
    
    // Stack pointer for this CPU
    *cpu_stack_ptr = cpu_stacks[cpu_id];
    
    // CPU ID for this CPU
    *cpu_id_ptr = cpu_id;
    
    printf("AMP: Page directory at 0x%08X, secondary_cpu_main at 0x%08X, stack at 0x%08X, CPU ID %d\n", 
           *kernel_page_dir_ptr, *secondary_main_ptr, *cpu_stack_ptr, *cpu_id_ptr);
    
    // Send INIT IPI
    apic_send_init_ipi(cpu_id);
    
    // Wait 10ms
    uint32_t start_time = get_tick_count();
    while (get_tick_count() - start_time < 10) {
        pause_cpu();
    }
    
    // Send STARTUP IPI (twice as per Intel spec)
    uint8_t startup_vector = 0x08;  // Start execution at 0x8000
    apic_send_startup_ipi(cpu_id, startup_vector);
    
    // Wait 200us
    start_time = get_tick_count();
    while (get_tick_count() - start_time < 1) {
        pause_cpu();
    }
    
    // Send second STARTUP IPI
    apic_send_startup_ipi(cpu_id, startup_vector);
    
    // Wait for CPU to come online (up to 1 second)
    start_time = get_tick_count();
    while (get_tick_count() - start_time < 100) {  // 1 second timeout
        if (amp_workspace && amp_workspace->cpu_info[cpu_id].online) {
            printf("AMP: CPU %d successfully started\n", cpu_id);
            return true;
        }
        pause_cpu();
    }
    
    printf("AMP: ERROR - CPU %d did not come online within timeout\n", cpu_id);
    
    // Check debug markers to see how far the AP startup code got
    uint8_t debug1 = *((uint8_t*)0x8FFC);
    uint8_t debug2 = *((uint8_t*)0x8FFD);
    uint8_t debug3 = *((uint8_t*)0x8FFE);
    uint8_t debug4 = *((uint8_t*)0x8FFF);
    
    printf("AMP: DEBUG - AP startup progress: 0x%02X 0x%02X 0x%02X 0x%02X\n", 
           debug1, debug2, debug3, debug4);
    
    if (debug1 == 0xAA) {
        if (debug2 == 0xBB) {
            if (debug3 == 0xCC) {
                if (debug4 == 0xDD) {
                    serial_puts("SERIAL: AMP - ERROR: AP reached 32-bit mode but failed to call C function\n");
                } else {
                    serial_puts("SERIAL: AMP - ERROR: AP failed to enter 32-bit mode\n");
                }
            } else {
                serial_puts("SERIAL: AMP - ERROR: AP failed to load GDT\n");
            }
        } else {
            serial_puts("SERIAL: AMP - ERROR: AP failed to set up segments\n");
        }
    } else {
        serial_puts("SERIAL: AMP - ERROR: AP startup code was never executed (IPI issue?)\n");
    }
    
    // Add additional debugging - check if APIC is working
    uint32_t apic_id = apic_read(LAPIC_ID_REG);
    printf("AMP: DEBUG - LAPIC ID register: 0x%08X\n", apic_id);
    
    return false;
}

// APIC implementation (basic version)

bool apic_init(void) {
    printf("AMP: Initializing APIC at 0x%08X\n", lapic_base_address);
    
    // Map LAPIC registers (assume they're already mapped)
    // In a full implementation, you'd map the LAPIC page here
    
    // Enable APIC in software
    uint32_t svr = apic_read(LAPIC_SVR_REG);
    svr |= 0x100;  // Enable APIC
    svr |= 0xFF;   // Set spurious vector to 0xFF
    apic_write(LAPIC_SVR_REG, svr);
    
    return true;
}

uint32_t apic_read(uint32_t reg) {
    return *((volatile uint32_t*)(lapic_base_address + reg));
}

void apic_write(uint32_t reg, uint32_t value) {
    *((volatile uint32_t*)(lapic_base_address + reg)) = value;
}

void apic_send_ipi(uint8_t target_cpu, uint8_t vector) {
    // Debug: Log IPI send attempt
    char debug_msg[128];
    sprintf(debug_msg, "SERIAL: APIC - Sending IPI vector 0x%02X to CPU %d\n", vector, target_cpu);
    serial_puts(debug_msg);
    
    // Wait for any previous IPI to complete
    while (apic_read(LAPIC_ICR_LOW_REG) & (1 << 12)) {
        pause_cpu();
    }
    
    // Set destination CPU
    apic_write(LAPIC_ICR_HIGH_REG, ((uint32_t)target_cpu) << 24);
    
    // Send IPI
    apic_write(LAPIC_ICR_LOW_REG, vector | (1 << 14));  // Physical destination mode
    
    // Wait for delivery to complete
    while (apic_read(LAPIC_ICR_LOW_REG) & (1 << 12)) {
        pause_cpu();
    }
    
    // Debug: Log IPI send completion
    sprintf(debug_msg, "SERIAL: APIC - IPI delivery completed to CPU %d\n", target_cpu);
    serial_puts(debug_msg);
}

void apic_send_init_ipi(uint8_t target_cpu) {
    // Wait for delivery
    while (apic_read(LAPIC_ICR_LOW_REG) & (1 << 12)) {
        pause_cpu();
    }
    
    apic_write(LAPIC_ICR_HIGH_REG, ((uint32_t)target_cpu) << 24);
    apic_write(LAPIC_ICR_LOW_REG, 0x4500);  // INIT IPI
}

void apic_send_startup_ipi(uint8_t target_cpu, uint8_t vector) {
    // Wait for delivery
    while (apic_read(LAPIC_ICR_LOW_REG) & (1 << 12)) {
        pause_cpu();
    }
    
    apic_write(LAPIC_ICR_HIGH_REG, ((uint32_t)target_cpu) << 24);
    apic_write(LAPIC_ICR_LOW_REG, 0x4600 | vector);  // STARTUP IPI
}

void apic_eoi(void) {
    apic_write(LAPIC_EOI_REG, 0);
}

/**
 * Select next available compute CPU using round-robin scheduling
 */
static uint8_t select_compute_cpu(void) {
    uint8_t start_cpu = next_cpu_id;
    
    // Try to find an online CPU starting from next_cpu_id
    do {
        if (next_cpu_id >= MAX_CPUS) {
            next_cpu_id = 1;  // Skip CPU 0 (master)
        }
        
        // All CPUs can now handle general compute work
        
        if (amp_workspace->cpu_info[next_cpu_id].online) {
            uint8_t selected = next_cpu_id;
            next_cpu_id++;  // Advance for next call
            return selected;
        }
        
        next_cpu_id++;
    } while (next_cpu_id != start_cpu);
    
    // No CPUs available, fallback to CPU 1 (never use CPU 2 for compute)
    return 1;
}

/**
 * Check if we're running on a secondary CPU
 */
bool amp_is_secondary_cpu(void) {
    if (!amp_initialized || !amp_workspace) {
        return false;
    }
    
    // Get current CPU ID from LAPIC
    uint32_t cpu_id = (apic_read(LAPIC_ID_REG) >> 24) & 0xFF;
    return (cpu_id != MASTER_CPU);
}

/**
 * Forward a syscall from secondary CPU to master CPU
 */
int amp_forward_syscall(uint32_t syscall_num, uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi) {
    extern void serial_puts(const char *msg);
    
    if (!amp_initialized || !amp_workspace) {
        serial_puts("SERIAL: SYSCALL - AMP not initialized, cannot forward\n");
        return -1;
    }
    
    // Get current CPU ID
    uint32_t cpu_id = (apic_read(LAPIC_ID_REG) >> 24) & 0xFF;
    
    char debug_msg[128];
    sprintf(debug_msg, "SERIAL: SYSCALL - CPU %d forwarding syscall %d\n", cpu_id, syscall_num);
    serial_puts(debug_msg);
    
    // Find a free request slot
    uint32_t slot = amp_workspace->syscall_state.request_queue_tail;
    uint32_t next_slot = (slot + 1) % 16; // MAX_CONCURRENT_FUNCTIONS = 16

    // Add debug to show which slot we're using
    char slot_debug_msg[128];
    sprintf(slot_debug_msg, "SERIAL: SYSCALL - Secondary CPU using slot %d\n", slot);
    serial_puts(slot_debug_msg);
    
    if (next_slot == amp_workspace->syscall_state.request_queue_head) {
        serial_puts("SERIAL: SYSCALL - Request queue full\n");
        return -1;
    }
    
    // Fill in the syscall request
    volatile amp_syscall_request_t *req = &amp_workspace->syscall_state.requests[slot];
    req->syscall_number = syscall_num;
    req->ebx = ebx;
    req->ecx = ecx;
    req->edx = edx;
    req->esi = esi;
    req->edi = edi;
    req->calling_cpu = cpu_id;
    req->process_id = 0; // TODO: Get current process ID
    
    // Mark response as not ready
    amp_workspace->syscall_state.response_ready[slot] = 0;
    
    // Update queue tail
    amp_workspace->syscall_state.request_queue_tail = next_slot;
    
    // Force memory sync by reading back the value
    volatile uint32_t sync_check = amp_workspace->syscall_state.request_queue_tail;
    (void)sync_check; // Prevent compiler warning
    
    sprintf(debug_msg, "SERIAL: SYSCALL - Request queued at slot %d, head=%d, tail=%d\n", 
            slot, amp_workspace->syscall_state.request_queue_head, next_slot);
    // Temporarily disable debug output to avoid serial deadlock
    // serial_puts(debug_msg);
    
    // Send IPI to master CPU to handle the syscall
    // serial_puts("SERIAL: SYSCALL - Sending IPI to master CPU\n");
    apic_send_ipi(0, 0x40); // Use literal values to avoid undefined constants
    // serial_puts("SERIAL: SYSCALL - IPI sent, waiting for response\n");
    
    // serial_puts("SERIAL: SYSCALL - Waiting for response from master CPU\n");
    
    // Wait for response (with longer timeout since master CPU might be busy)
    uint32_t timeout = 50000;  // Increased timeout for master CPU processing
    while (timeout > 0) {
        // Memory fence to invalidate cache and force fresh read from memory
        asm volatile("lfence" ::: "memory");
        
        // Now read the response_ready flag from main memory
        volatile uint32_t response_ready = amp_workspace->syscall_state.response_ready[slot];
        if (response_ready) {
            break; // Response is ready
        }
        
        timeout--;
        // Small delay
        for (volatile int i = 0; i < 1000; i++);
    }
    
    if (timeout == 0) {
        serial_puts("SERIAL: SYSCALL - Timeout waiting for response\n");
        return -1;
    }

    // Get the result by accessing structure members directly
    int result = amp_workspace->syscall_state.responses[slot].return_value;
    
    // Add minimal debug to see if secondary CPU gets response
    serial_puts("SERIAL: SYSCALL - Secondary CPU got response value: ");
    if (result == 456) {
        serial_puts("456\n");
    } else if (result == -1) {
        serial_puts("-1\n");
    } else {
        serial_puts("OTHER\n");
    }
    
    return result;
}

/**
 * Handle syscall requests on master CPU (called periodically)
 */
// Helper function to check if a syscall is interrupt-unsafe
static int is_syscall_interrupt_unsafe(uint32_t syscall_num) {
    switch (syscall_num) {
        case 0: // os_puts - accesses VGA/graphics in interrupt context
            return 1;
        // Add other interrupt-unsafe syscalls here as needed
        default:
            return 0; // Safe to execute in interrupt context
    }
}

// Global debug counter for syscall handling (safe to access from interrupt)
static volatile uint32_t syscall_handle_count = 0;
static volatile uint32_t syscall_process_count = 0;
static volatile uint32_t syscall_response_set_count = 0;


void amp_handle_syscall_requests(void) {
    if (!amp_initialized || !amp_workspace) {
        return;
    }

    // Use memory barriers to ensure cache coherency
    asm volatile("mfence" ::: "memory");
    asm volatile("lfence" ::: "memory");

    // Check if there are pending syscall requests
    volatile uint32_t head = amp_workspace->syscall_state.request_queue_head;
    volatile uint32_t tail = amp_workspace->syscall_state.request_queue_tail;

    if (head != tail) {
        printf("SYSCALL HANDLER: Processing syscall, head=%d, tail=%d\n", head, tail);
        
        // Process the request
        if (head >= 16) { // MAX_CONCURRENT_FUNCTIONS - safety check
            return; // Invalid head index
        }
        
        uint32_t slot = head;
    
        // Get syscall parameters
        uint32_t syscall_num = amp_workspace->syscall_state.requests[slot].syscall_number;
        uint32_t calling_cpu = amp_workspace->syscall_state.requests[slot].calling_cpu;
        uint32_t param_ebx = amp_workspace->syscall_state.requests[slot].ebx;
        uint32_t param_ecx = amp_workspace->syscall_state.requests[slot].ecx;
        uint32_t param_edx = amp_workspace->syscall_state.requests[slot].edx;
        uint32_t param_esi = amp_workspace->syscall_state.requests[slot].esi;
        uint32_t param_edi = amp_workspace->syscall_state.requests[slot].edi;
        
        // Execute the syscall
        int result = 0;
        
        if (syscall_num == 0) {
            // os_puts syscall
            extern void os_puts(const char *text);
            os_puts("Hello from AMP syscall forwarding!");
            result = 42;
        } else if (syscall_num == 13) {
            // os_exec_process syscall
            int pid = param_ebx;
            extern int os_exec_process(int pid);
            result = os_exec_process(pid);
        } else {
            result = 42; // Default for other syscalls
        }
        
        // Store the response
        amp_workspace->syscall_state.responses[slot].return_value = result;
        amp_workspace->syscall_state.responses[slot].error_code = 0;
        amp_workspace->syscall_state.responses[slot].request_id = slot;
        
        // Mark response as ready
        amp_workspace->syscall_state.response_ready[slot] = 1;
        
        // Update queue head
        amp_workspace->syscall_state.request_queue_head = (slot + 1) % 16; // MAX_CONCURRENT_FUNCTIONS
    }
}

/**
 * IPI handler for syscall requests from secondary CPUs
 * This runs on CPU 0 when secondary CPUs send syscall IPIs
 */
void amp_syscall_ipi_handler(registers_t *regs) {
    // Immediately process any pending syscall requests
    amp_handle_syscall_requests();
}

/**
 * Map shared memory in a specific page directory
 */
int map_shared_memory_to_directory(page_directory_t *dir) {
    extern page_t *get_page(uint32_t address, int make, page_directory_t *dir);
    extern void alloc_frame(page_t *page, int is_kernel, int is_writeable);
    extern void serial_puts(const char *msg);
    
    if (!dir) {
        serial_puts("SERIAL: AMP - ERROR: NULL directory passed to map_shared_memory_to_directory\n");
        return -1;
    }
    
    // Add basic sanity check for directory pointer
    if ((uint32_t)dir < 0x100000 || (uint32_t)dir > 0xFFFFFF00) {
        serial_puts("SERIAL: AMP - ERROR: Invalid directory pointer in map_shared_memory_to_directory\n");
        return -1;
    }
    
    // Map 4 pages to cover all CPU sections (4 CPUs * 1KB = 4KB total, rounded up to 4 pages)
    uint32_t start_addr = SHARED_MEMORY_BASE;
    uint32_t end_addr = SHARED_MEMORY_BASE + SHARED_MEMORY_SIZE;
    
    for (uint32_t addr = start_addr; addr < end_addr; addr += 0x1000) {
        // Get page in target directory
        page_t *page = get_page(addr, 1, dir);
        if (!page) {
            serial_puts("SERIAL: AMP - ERROR: get_page failed for shared memory\n");
            return -1;
        }
        
        extern page_directory_t *kernel_directory;
        if (dir == kernel_directory) {
            // For kernel directory, allocate new frames normally
            if (!page->frame) {
                alloc_frame(page, 0, 1); // is_kernel=0 (accessible from all), is_writeable=1
                if (!page->frame) {
                    serial_puts("SERIAL: AMP - ERROR: alloc_frame failed for shared memory\n");
                    return -1;
                }
            }
        } else {
            // For other directories, SHARE the same physical frames as kernel directory
            page_t *kernel_page = get_page(addr, 0, kernel_directory); // Don't create if doesn't exist
            if (kernel_page && kernel_page->frame) {
                // Copy the kernel page's frame and settings
                page->present = 1;
                page->rw = 1;
                page->user = 1; // Allow user access
                page->frame = kernel_page->frame; // SHARE the same physical frame
            } else {
                serial_puts("SERIAL: AMP - ERROR: kernel directory doesn't have shared memory mapped\n");
                return -1;
            }
        }
    }
    
    return 0;
}

/**
 * Initialize shared memory region for inter-CPU communication
 */
int amp_init_shared_memory(void) {
    extern void serial_puts(const char *msg);
    extern page_directory_t *kernel_directory;
    extern page_directory_t *current_directory;
    
    if (shared_memory_initialized) {
        return 0; // Already initialized
    }
    
    // Debug output
    char debug_msg[100];
    sprintf(debug_msg, "SERIAL: AMP - kernel_directory = 0x%08X, current_directory = 0x%08X\n", 
            (uint32_t)kernel_directory, (uint32_t)current_directory);
    serial_puts(debug_msg);
    
    // Map shared memory in kernel directory
    serial_puts("SERIAL: AMP - Mapping shared memory in kernel directory\n");
    if (map_shared_memory_to_directory(kernel_directory) != 0) {
        serial_puts("SERIAL: AMP - ERROR: Failed to map shared memory in kernel directory\n");
        return -1;
    }
    
    // Also map in current directory if different
    if (current_directory && current_directory != kernel_directory) {
        serial_puts("SERIAL: AMP - Mapping shared memory in current directory\n");
        if (map_shared_memory_to_directory(current_directory) != 0) {
            serial_puts("SERIAL: AMP - ERROR: Failed to map shared memory in current directory\n");
            return -1;
        }
    }
    
    // Clear the shared memory
    memset((void*)SHARED_MEMORY_BASE, 0, SHARED_MEMORY_SIZE);
    
    // Initialize each CPU's shared data structure explicitly
    for (int cpu_id = 0; cpu_id < 4; cpu_id++) {
        shared_cpu_data_t* cpu_data = amp_get_cpu_shared_data(cpu_id);
        if (cpu_data) {
            cpu_data->counter = 0;
            cpu_data->ready_flag = 0;
            cpu_data->message_length = 0;
            cpu_data->reserved = 0;
            memset(cpu_data->message, 0, MAX_MESSAGE_SIZE);
            
            char debug_msg[64];
            sprintf(debug_msg, "SERIAL: AMP - Initialized CPU %d shared data at 0x%08X\n", 
                    cpu_id, (uint32_t)cpu_data);
            serial_puts(debug_msg);
        }
    }
    
    shared_memory_initialized = true;
    
    char msg[80];
    sprintf(msg, "SERIAL: AMP - Shared memory initialized at virtual 0x%08X (%d KB)\n", 
            SHARED_MEMORY_BASE, SHARED_MEMORY_SIZE / 1024);
    serial_puts(msg);
    
    return 0;
}

/**
 * Shutdown shared memory region
 */
void amp_shutdown_shared_memory(void) {
    extern void free_frame(page_t *page);
    extern page_t *get_page(uint32_t address, int make, page_directory_t *dir);
    extern page_directory_t *kernel_directory;
    
    if (!shared_memory_initialized) {
        return;
    }
    
    // Free the allocated pages
    uint32_t start_addr = SHARED_MEMORY_BASE;
    uint32_t end_addr = SHARED_MEMORY_BASE + SHARED_MEMORY_SIZE;
    
    for (uint32_t addr = start_addr; addr < end_addr; addr += 0x1000) {
        page_t *page = get_page(addr, 0, kernel_directory);
        if (page && page->frame) {
            free_frame(page);
        }
    }
    
    shared_memory_base = NULL;
    shared_memory_initialized = false;
}

/**
 * Get pointer to shared data section for a specific CPU
 */
shared_cpu_data_t* amp_get_cpu_shared_data(uint8_t cpu_id) {
    if (!shared_memory_initialized || cpu_id >= MAX_CPUS) {
        return NULL;
    }
    
    return (shared_cpu_data_t*)(SHARED_MEMORY_BASE + cpu_id * SHARED_MEMORY_PER_CPU);
}

/**
 * Ensure shared memory is mapped in the current page directory
 * Called by secondary CPUs to make sure they can access shared memory
 */
int amp_ensure_shared_memory_mapped(void) {
    extern page_directory_t *current_directory;
    extern page_directory_t *kernel_directory;
    
    if (!shared_memory_initialized) {
        return -1; // Shared memory not initialized yet
    }
    
    // Get the currently active page directory from CR3
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r" (cr3));
    page_directory_t *active_dir = (page_directory_t *)(cr3 & 0xFFFFF000);
    
    // Map to both current_directory and the active directory to be safe
    int result1 = map_shared_memory_to_directory(current_directory);
    int result2 = map_shared_memory_to_directory(active_dir);
    
    // Also ensure it's mapped in kernel directory
    int result3 = map_shared_memory_to_directory(kernel_directory);
    
    // Force TLB flush to ensure mappings are active
    asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax", "memory");
    
    return (result1 == 0 && result2 == 0 && result3 == 0) ? 0 : -1;
}

/**
 * Check shared memory for data from secondary CPUs (called by Core 0)
 */
void amp_check_shared_memory(void) {
    extern void serial_puts(const char *msg);
    char msg[256];  // Buffer for debug messages
    
    if (!shared_memory_initialized) {
        serial_puts("SERIAL: AMP - Shared memory not initialized\n");
        return;
    }
    
    // Check each secondary CPU's shared data section for activity
    for (int cpu = 1; cpu < MAX_CPUS; cpu++) {
        shared_cpu_data_t* cpu_data = amp_get_cpu_shared_data(cpu);
        if (!cpu_data) {
            sprintf(msg, "SERIAL: AMP - ERROR: CPU %d data pointer is NULL\n", cpu);
            serial_puts(msg);
            continue;
        }
        
        // Only report if this CPU has new activity (ready flag set)
        if (cpu_data->ready_flag) {
            // Log message if present and valid
            if (cpu_data->message_length > 0 && cpu_data->message_length < MAX_MESSAGE_SIZE) {
                // Ensure null termination
                cpu_data->message[cpu_data->message_length] = 0;
                sprintf(msg, "SERIAL: AMP - CPU %d message: %s\n", 
                       cpu, cpu_data->message);
                serial_puts(msg);
            }
            
            // Clear the ready flag (acknowledge the data)
            cpu_data->ready_flag = 0;
        }
    }
}

/**
 * IPI handler for function ready notifications
 * This is called when a secondary CPU receives an IPI_FUNCTION_READY interrupt
 */
void ipi_function_ready_handler(registers_t *regs) {
    // This handler doesn't need to do anything special
    // The IPI just wakes up the CPU from HLT
    // The main loop in amp_cpu.c will check for pending functions
    
    // Add debug output to confirm IPI was received
    extern void serial_puts(const char *msg);
    extern uint8_t amp_get_current_cpu_id(void);
    uint8_t cpu_id = amp_get_current_cpu_id();
    
    // Make this VERY visible
    serial_puts("!!! IPI INTERRUPT RECEIVED !!!\n");
    char debug_msg[128];
    sprintf(debug_msg, "!!! CPU %d received IPI_FUNCTION_READY interrupt vector 0x%02X !!!\n", cpu_id, IPI_FUNCTION_READY);
    serial_puts(debug_msg);
    serial_puts("!!! IPI INTERRUPT RECEIVED !!!\n");
    
    // Send EOI to LAPIC
    extern void apic_eoi(void);
    apic_eoi();
}

// Graphics functions removed - all graphics now handled directly on CPU0
  
