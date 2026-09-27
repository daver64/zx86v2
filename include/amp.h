// amp.h -- Asymmetric Multiprocessing support for zx86v2
// Implements function offloading to secondary CPUs

#ifndef AMP_H
#define AMP_H

#include "common.h"

// Forward declarations
struct page_directory;
typedef struct page_directory page_directory_t;

// Maximum number of CPUs supported
#define MAX_CPUS 16
#define MAX_BOOT_CPUS 8  // Actual number of CPUs to try to boot

// Bitmap processing configuration - use only the last two highest cores
#define BITMAP_CPU_COUNT 2
#define BITMAP_CPU_START (MAX_BOOT_CPUS - BITMAP_CPU_COUNT)  // Start at CPU 6 for 8-core system

// CPU roles
#define MASTER_CPU    0  // Kernel, drivers, scheduling, I/O, graphics (no longer in AMP workspace)
#define COMPUTE_CPU_1 1 // General computation tasks
#define COMPUTE_CPU_2 2 // General computation tasks  
#define COMPUTE_CPU_3 3  // Additional compute (if available)
#define COMPUTE_CPU_4 4  // Additional compute (if available)
#define COMPUTE_CPU_5 5  // Additional compute (if available)
#define COMPUTE_CPU_6 6  // Additional compute (if available)
#define COMPUTE_CPU_7 7  // Additional compute (if available)
#define COMPUTE_CPU_8 8  // Additional compute (if available)
#define COMPUTE_CPU_9 9  // Additional compute (if available)
#define COMPUTE_CPU_10 10 // Additional compute (if available)
#define COMPUTE_CPU_11 11 // Additional compute (if available)
#define COMPUTE_CPU_12 12 // Additional compute (if available)
#define COMPUTE_CPU_13 13 // Additional compute (if available)
#define COMPUTE_CPU_14 14 // Additional compute (if available)
#define COMPUTE_CPU_15 15 // Additional compute (if available)

// Function call status
#define FUNCTION_FREE     0
#define FUNCTION_PENDING  1
#define FUNCTION_RUNNING  2
#define FUNCTION_COMPLETE 3
#define FUNCTION_ERROR    4

// IPI (Inter-Processor Interrupt) commands
#define IPI_FUNCTION_READY    0xF0
#define IPI_FUNCTION_COMPLETE 0xF1
#define IPI_SHUTDOWN          0xF2
#define IPI_WAKEUP           0xF3

// Shared memory configuration
#define MAX_CONCURRENT_FUNCTIONS 16
#define SHARED_BUFFER_SIZE (64 * 1024)  // 64KB shared space per function
#define FUNCTION_TIMEOUT_MS 5000        // 5 second timeout

// Inter-CPU shared memory configuration
#define SHARED_MEMORY_BASE    0xF0000000  // Virtual address for shared memory region (high kernel space)
#define SHARED_MEMORY_SIZE    0x800000    // 8MB for expanded workspace and inter-CPU communication
#define SHARED_MEMORY_PER_CPU 0x400       // 1KB per CPU section (4 CPUs * 1KB = 4KB total)
#define MAX_MESSAGE_SIZE      4080        // Message size within each CPU section

// APIC register offsets
#define LAPIC_ID_REG         0x020
#define LAPIC_VERSION_REG    0x030
#define LAPIC_TPR_REG        0x080
#define LAPIC_EOI_REG        0x0B0
#define LAPIC_SVR_REG        0x0F0
#define LAPIC_ICR_LOW_REG    0x300
#define LAPIC_ICR_HIGH_REG   0x310
#define LAPIC_TIMER_REG      0x320
#define LAPIC_LINT0_REG      0x350
#define LAPIC_LINT1_REG      0x360

// Function call descriptor
typedef struct {
    void *function_ptr;          // Function to execute
    void *args;                  // Function arguments (pointer to shared buffer)
    size_t args_size;           // Size of arguments
    void *result;               // Return value location (pointer to shared buffer)
    size_t result_size;         // Expected result size
    volatile uint32_t status;   // Current status
    uint32_t task_id;           // Unique identifier
    uint32_t start_time;        // When function started (for timeout)
    uint32_t cpu_assigned;      // Which CPU is handling this
} cpu_function_call_t;

// Per-CPU information
typedef struct {
    uint8_t cpu_id;
    uint8_t online;             // 1 if CPU is active
    uint8_t role;               // MASTER_CPU or COMPUTE_CPU_x
    volatile uint32_t status;   // IDLE, BUSY, ERROR
    uint32_t kernel_stack;      // Kernel stack for this CPU
    uint32_t functions_executed; // Statistics
    uint32_t last_heartbeat;    // For health monitoring
} cpu_info_t;

// Syscall forwarding structures (defined before workspace to avoid forward declaration issues)
typedef struct {
    uint32_t syscall_number;     // Syscall number (from EAX)
    uint32_t ebx, ecx, edx, esi, edi; // Syscall parameters
    uint32_t calling_cpu;        // Which CPU made the syscall
    uint32_t process_id;         // Process making the syscall
} amp_syscall_request_t;

typedef struct {
    uint32_t return_value;       // Syscall return value (for EAX)
    int32_t error_code;          // Error code if syscall failed
    uint32_t request_id;         // To match with request
} amp_syscall_response_t;

// Shared memory communication structure (per CPU)
typedef struct {
    volatile uint32_t counter;        // General purpose counter
    volatile uint32_t ready_flag;     // Set when data is ready to read
    volatile uint32_t message_length; // Length of message if any
    uint32_t reserved;                // For future use
    char message[MAX_MESSAGE_SIZE];   // Message buffer (rest of 4KB)
} shared_cpu_data_t;

// Syscall forwarding state
typedef struct {
    volatile amp_syscall_request_t requests[MAX_CONCURRENT_FUNCTIONS];
    volatile amp_syscall_response_t responses[MAX_CONCURRENT_FUNCTIONS];
    volatile uint32_t request_queue_head;
    volatile uint32_t request_queue_tail;
    volatile uint32_t response_ready[MAX_CONCURRENT_FUNCTIONS];
} amp_syscall_state_t;

// Shared workspace for function execution
typedef struct {
    // Function queue
    volatile cpu_function_call_t function_queue[MAX_CONCURRENT_FUNCTIONS];
    volatile uint32_t queue_head;
    volatile uint32_t queue_tail;
    volatile uint32_t next_task_id;
    
    // Shared buffers for arguments and results
    uint8_t shared_args_buffer[MAX_CONCURRENT_FUNCTIONS][SHARED_BUFFER_SIZE];
    uint8_t shared_result_buffer[MAX_CONCURRENT_FUNCTIONS][SHARED_BUFFER_SIZE];
    
    // CPU status information
    cpu_info_t cpu_info[MAX_CPUS];
    
    // Syscall forwarding state
    amp_syscall_state_t syscall_state;
    
    // Statistics
    uint32_t total_functions_executed;
    uint32_t total_errors;
    uint32_t total_timeouts;
    
} amp_workspace_t;

// Function pointer types for offloading
typedef void* (*amp_function_t)(void *args);
typedef void* (*amp_function_noargs_t)(void);

// Core AMP API functions
bool amp_init(void);
bool amp_start_secondary_cpus(void);
void amp_shutdown(void);

// Function offloading API
int amp_execute_function(void *func, void *args, size_t args_size, 
                        void *result, size_t result_size);
int amp_execute_function_async(void *func, void *args, size_t args_size);
int amp_execute_function_on_cpu(uint8_t cpu_id, void *func, void *args, 
                                size_t args_size, void *result, size_t result_size);
int amp_wait_for_completion(uint32_t task_id);
bool amp_is_task_complete(uint32_t task_id);

// Process execution on specific CPUs
int amp_exec_process_on_cpu(int pid, int cpu_id, int wait_for_completion);

// CPU management
bool amp_is_cpu_online(uint8_t cpu_id);
uint8_t amp_get_cpu_count(void);
uint8_t amp_get_current_cpu_id(void);
bool amp_is_initialized(void);
void amp_print_status(void);

// APIC functions (internal)
bool apic_init(void);
void apic_send_ipi(uint8_t target_cpu, uint8_t vector);
void apic_send_init_ipi(uint8_t target_cpu);
void apic_send_startup_ipi(uint8_t target_cpu, uint8_t vector);
void apic_eoi(void);
uint32_t apic_read(uint32_t reg);
void apic_write(uint32_t reg, uint32_t value);

// CPU bootstrap functions (internal)
void cpu1_entry(void);      // Assembly entry point
void cpu1_main(void);       // C main function for CPU 1
void secondary_cpu_main(uint8_t cpu_id);  // Generic secondary CPU main

// Utility functions
void amp_cpu_pause(void);
uint32_t amp_get_timestamp(void);

// Error codes
#define AMP_SUCCESS           0
#define AMP_ERROR_QUEUE_FULL -1
#define AMP_ERROR_INVALID_CPU -2
#define AMP_ERROR_TIMEOUT    -3
#define AMP_ERROR_NOT_INIT   -4
#define AMP_ERROR_NO_MEMORY  -5
#define AMP_ERROR_INVALID_ARGS -6

// Global variables (declared here, defined in amp.c)
extern volatile amp_workspace_t *amp_workspace;
extern bool amp_initialized;
extern uint32_t lapic_base_address;
extern bool shared_memory_initialized;

// Syscall forwarding functions
int amp_forward_syscall(uint32_t syscall_num, uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi);
void amp_handle_syscall_requests(void);
bool amp_is_secondary_cpu(void);

// Shared memory functions
int amp_init_shared_memory(void);
void amp_shutdown_shared_memory(void);
void amp_check_shared_memory(void);
shared_cpu_data_t* amp_get_cpu_shared_data(uint8_t cpu_id);
int amp_ensure_shared_memory_mapped(void);
int map_shared_memory_to_directory(page_directory_t *dir);


// Utility functions
uint8_t amp_get_current_cpu_id(void);
shared_cpu_data_t* amp_get_cpu_shared_data(uint8_t cpu_id);

#endif // AMP_H