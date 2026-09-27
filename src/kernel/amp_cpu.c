// amp_cpu.c -- Secondary CPU execution code for AMP
// Main execution loop for compute CPUs

#include "amp.h"
#include "common.h"
#include "timer.h"
#include "isr.h"
#include "descriptor_tables.h"
#include "string.h"
#include <stdint.h>

// External workspace reference
extern volatile amp_workspace_t *amp_workspace;
extern bool amp_initialized;

// Forward declarations
static void execute_function_call(volatile cpu_function_call_t *call);
static void handle_ipi_interrupt(uint8_t cpu_id, uint8_t vector);
static volatile cpu_function_call_t* find_pending_function(uint8_t cpu_id);

/**
 * Main entry point for secondary CPUs
 * This function is called by the assembly bootstrap code
 */
void secondary_cpu_main(uint8_t cpu_id) {
    extern void serial_puts(const char *msg);
    extern void idt_flush(uint32_t);
    extern idt_ptr_t idt_ptr;
    extern void gdt_flush(uint32_t);
    extern gdt_ptr_t gdt_ptr;
    
    // Disable interrupts initially
    disable_interrupts();
    
    // Ensure secondary CPU uses kernel page directory for all operations
    extern uint32_t get_kernel_physical(void);
    uint32_t kernel_phys = get_kernel_physical();
    if (kernel_phys) {
        // Force switch to kernel directory
        asm volatile("mov %0, %%cr3" :: "r" (kernel_phys) : "memory");
        asm volatile("jmp 1f; 1:" ::: "memory"); // Serialize execution
    } else {
        return; // Exit if no kernel directory
    }
    
    // Set up GDT and IDT for this CPU - critical for interrupt handling
    gdt_flush((uint32_t)&gdt_ptr);
    idt_flush((uint32_t)&idt_ptr);
    
    // Register IPI handler on this CPU
    extern void register_interrupt_handler(uint8_t n, isr_t handler);
    extern void ipi_function_ready_handler(registers_t *regs);
    extern void idt_set_gate_public(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
    
    // First register the software handler
    register_interrupt_handler(IPI_FUNCTION_READY, &ipi_function_ready_handler);
    
    // Then set up the IDT entry to actually call our handler
    // We'll use a generic ISR wrapper - let's try using the same mechanism as other interrupts
    extern void isr128();  // Generic ISR stub
    idt_set_gate_public(IPI_FUNCTION_READY, (uint32_t)isr128, 0x08, 0x8E);
    
    // Verify the handler was registered by checking the IDT
    extern idt_entry_t idt_entries[256];
    idt_entry_t *entry = &idt_entries[IPI_FUNCTION_READY];
    uint32_t handler_addr = entry->base_lo | (entry->base_hi << 16);
    
    char verify_msg[256];
    sprintf(verify_msg, "SERIAL: CPU %d - IDT entry 0x%02X: base=0x%08X, flags=0x%02X\n", 
            cpu_id, IPI_FUNCTION_READY, handler_addr, entry->flags);
    serial_puts(verify_msg);
    
    // Initialize LAPIC on this CPU for IPI reception
    extern bool apic_init(void);
    extern uint32_t apic_read(uint32_t reg);
    extern void apic_write(uint32_t reg, uint32_t value);
    
    // Make sure LAPIC is enabled on this CPU
    uint32_t spurious_reg = apic_read(LAPIC_SVR_REG);
    apic_write(LAPIC_SVR_REG, spurious_reg | 0x100); // Enable LAPIC
    
    // Read and log the LAPIC ID for this CPU
    uint32_t lapic_id_reg = apic_read(LAPIC_ID_REG);
    uint8_t lapic_id = (lapic_id_reg >> 24) & 0xFF;
    
    char ipi_msg[256];
    sprintf(ipi_msg, "SERIAL: CPU %d registered IPI handler for vector 0x%02X, enabled LAPIC, LAPIC_ID=0x%02X\n", 
            cpu_id, IPI_FUNCTION_READY, lapic_id);
    serial_puts(ipi_msg);
    
    // Wait for AMP to be initialized by CPU 0
    while (!amp_initialized || !amp_workspace) {
        pause_cpu();
    }
    
    // Wait for shared memory to be initialized - no need to map it ourselves
    // since CPU 0 already mapped it in the kernel directory that we're using
    extern bool shared_memory_initialized;
    
    // Wait up to 5 seconds for shared memory to be initialized
    int retry_count = 0;
    while (!shared_memory_initialized && retry_count < 500) {
        pause_cpu();
        retry_count++;
    }
    
    if (shared_memory_initialized) {
        // Shared memory is ready
    } else {
        // Don't proceed if shared memory isn't ready
        while (1) {
            halt_cpu();
        }
    }
    
    // Test workspace access safety before proceeding
    // Try to read a safe field first to verify accessibility
    if (amp_workspace) {
        // Try a simple read test to see if workspace is accessible
        volatile uint32_t test_read = amp_workspace->next_task_id;
        if (test_read > 0) {
            // Now it's safe to mark this CPU as online
            amp_workspace->cpu_info[cpu_id].online = 1;
            amp_workspace->cpu_info[cpu_id].status = 0;  // IDLE
            amp_workspace->cpu_info[cpu_id].last_heartbeat = amp_get_timestamp();
        } else {
            while (1) {
                halt_cpu();
            }
        }
    } else {
        serial_puts("SERIAL: Secondary CPU - Workspace pointer is NULL\n");
        while (1) {
            halt_cpu();
        }
    }
    
    // Force kernel directory one more time before main loop
    if (kernel_phys) {
        asm volatile("mov %0, %%cr3" :: "r" (kernel_phys) : "memory");
    }
    
    // Enable interrupts for IPI handling
    enable_interrupts();
    
    // Debug: Announce that we're starting the main loop
    char startup_msg[128];
    sprintf(startup_msg, "SERIAL: CPU %d starting main execution loop\n", cpu_id);
    serial_puts(startup_msg);
    
    // Main execution loop - power-efficient with HLT and IPI wake-up
    int loop_count = 0;
    while (1) {
        loop_count++;
        
        // Debug: Show that we're in the loop (first few iterations only)
        if (loop_count <= 3) {
            char loop_msg[128];
            sprintf(loop_msg, "SERIAL: CPU %d main loop iteration %d\n", cpu_id, loop_count);
            serial_puts(loop_msg);
        }
        
        // Check for pending work
        volatile cpu_function_call_t* pending_call = find_pending_function(cpu_id);
        
        if (pending_call) {
            // Execute the function call
            execute_function_call(pending_call);
            
            // Update heartbeat after work completion
            if (amp_workspace) {
                amp_workspace->cpu_info[cpu_id].last_heartbeat = amp_get_timestamp();
            }
        } else {
            // No work available - enter power saving mode
            // Update status to IDLE
            if (amp_workspace) {
                amp_workspace->cpu_info[cpu_id].status = 0; // IDLE
            }
            
            // Debug: Check interrupt flag before HLT
            uint32_t eflags;
            asm volatile("pushf; pop %0" : "=r"(eflags));
            if (loop_count <= 3) {
                char int_msg[128];
                sprintf(int_msg, "SERIAL: CPU %d going to HLT, interrupts %s (eflags=0x%X)\n", 
                       cpu_id, (eflags & 0x200) ? "ENABLED" : "DISABLED", eflags);
                serial_puts(int_msg);
            }
            
            // Enter HLT state to save power - will wake on any interrupt (including IPI)
            // The 'sti' ensures interrupts are enabled before HLT
            // This saves significant power compared to busy waiting
            asm volatile("sti; hlt" ::: "memory");
            
            // CPU wakes up here - let's see if this actually happens
            char wake_msg[128];
            sprintf(wake_msg, "SERIAL: CPU %d woke up from HLT, checking for work immediately\n", cpu_id);
            serial_puts(wake_msg);
            
            // Force an immediate check for work after waking up
            volatile cpu_function_call_t* immediate_check = find_pending_function(cpu_id);
            if (immediate_check) {
                sprintf(wake_msg, "SERIAL: CPU %d found work immediately after wake-up!\n", cpu_id);
                serial_puts(wake_msg);
            }
            
            // Reduce debug spam - only show wake-up occasionally
            static int wake_count = 0;
            wake_count++;
            
            // CPU wakes up here on interrupt:
            // - IPI from CPU0 with new work (IPI_FUNCTION_READY)
            // - Timer interrupt (periodic check)
            // - Any other system interrupt
            // The interrupt handler will have run, now check for work again
        }
    }
}

/**
 * Execute a function call on this CPU
 */
static void execute_function_call(volatile cpu_function_call_t *call) {
    extern void serial_puts(const char *msg);
    
    if (!call || call->status != FUNCTION_PENDING) {
        serial_puts("SERIAL: Execute function - ERROR: invalid call\n");
        return;
    }
    
    uint8_t cpu_id = call->cpu_assigned;  // Get CPU ID from the call
    
    // Mark as running
    call->status = FUNCTION_RUNNING;
    
    // Cast function pointer to appropriate type
    amp_function_t func = (amp_function_t)call->function_ptr;
    
    if (!func) {
        serial_puts("SERIAL: Execute function - ERROR: null function pointer\n");
        call->status = FUNCTION_ERROR;
        return;
    }
    
    char debug_msg[128];
    sprintf(debug_msg, "SERIAL: Execute function - About to call function 0x%X with args 0x%X\n", 
            (uint32_t)func, (uint32_t)call->args);
    serial_puts(debug_msg);
    
    // Execute the function with arguments
    void *result = func(call->args);
    
    sprintf(debug_msg, "SERIAL: Execute function - Function returned, result: 0x%X\n", (uint32_t)result);
    serial_puts(debug_msg);
    
    // Copy result to shared buffer if expected
    if (call->result_size > 0 && call->result) {
        // Handle the case where result is a value cast as pointer, not a memory address
        if (call->result_size == sizeof(int)) {
            // For integer results, the pointer value IS the result
            int int_result = (int)((unsigned long)result);
            memcpy((void*)call->result, &int_result, sizeof(int));
            
            char result_msg[128];
            sprintf(result_msg, "SERIAL: Execute function - Copied integer result %d to shared buffer\n", int_result);
            serial_puts(result_msg);
        } else {
            // For other types, assume result is a pointer to memory
            if (result) {
                memcpy((void*)call->result, result, call->result_size);
            }
        }
    }
    
    call->status = FUNCTION_COMPLETE;
    
    // No need to send IPI - CPU 0 will poll for completion
}

/**
 * Find a pending function call assigned to this CPU
 */
static volatile cpu_function_call_t* find_pending_function(uint8_t cpu_id) {
    // Remove debug spam - only log errors
    if (!amp_workspace) {
        serial_puts("SERIAL: find_pending_function - ERROR: amp_workspace is null\n");
        return NULL;
    }
    
    // Add debug to see if we're looking for work
    static int check_count = 0;
    check_count++;
    if (check_count % 10000 == 0) { // Every 10000 checks, log status
        char debug_msg[128];
        sprintf(debug_msg, "SERIAL: CPU %d checking for work (check #%d)\n", cpu_id, check_count);
        serial_puts(debug_msg);
    }
    
    for (int i = 0; i < MAX_CONCURRENT_FUNCTIONS; i++) {
        volatile cpu_function_call_t *call = &amp_workspace->function_queue[i];
        
        if (call->status == FUNCTION_PENDING && call->cpu_assigned == cpu_id) {
            char debug_msg[128];
            sprintf(debug_msg, "SERIAL: CPU %d found pending work in slot %d\n", cpu_id, i);
            serial_puts(debug_msg);
            return call;
        }
    }
    return NULL;
}

/**
 * IPI interrupt handler for secondary CPUs
 * This gets called when CPU 0 sends an IPI
 */
void amp_ipi_handler(registers_t *regs) {
    uint8_t cpu_id = amp_get_current_cpu_id();  // We'll implement this
    
    // Check what type of IPI this is based on vector
    uint8_t vector = regs->int_no;
    
    switch (vector) {
        case IPI_FUNCTION_READY:
            // CPU 0 has queued a function for us - main loop will pick it up
            break;
            
        case IPI_SHUTDOWN:
            printf("CPU %d: Received shutdown IPI\n", cpu_id);
            amp_workspace->cpu_info[cpu_id].online = 0;
            // Halt this CPU
            while (1) {
                halt_cpu();
            }
            break;
            
        case IPI_WAKEUP:
            // Just a wakeup call - continue execution
            break;
            
        default:
            printf("CPU %d: Unknown IPI vector 0x%02X\n", cpu_id, vector);
            break;
    }
    
    // Send EOI
    apic_eoi();
}

/**
 * Get current CPU ID (simplified version)
 * In a full implementation, this would read the LAPIC ID
 * NOTE: CPU0 is not part of AMP system - it handles syscalls directly
 */
uint8_t amp_get_current_cpu_id(void) {
    // Read the LAPIC ID register to get the actual CPU ID
    extern uint32_t apic_read(uint32_t reg);
    
    if (!amp_initialized) {
        return 0;  // Before AMP init, assume CPU 0
    }
    
    // Read the LAPIC ID register
    uint32_t lapic_id_reg = apic_read(LAPIC_ID_REG);
    uint8_t lapic_id = (lapic_id_reg >> 24) & 0xFF;
    
    // In most systems, LAPIC ID matches logical CPU ID
    return lapic_id;
}

/**
 * Check if specified CPU is online
 * NOTE: CPU0 is always considered "online" but not part of AMP system
 */
bool amp_is_cpu_online(uint8_t cpu_id) {
    if (cpu_id >= 16) {  // Use 16 instead of MAX_CPUS
        return false;
    }
    
    // CPU0 is always online but not part of AMP
    if (cpu_id == 0) {
        return true;  // CPU0 is always online for syscalls
    }
    
    if (!amp_initialized || !amp_workspace) {
        return false;
    }
    
    return amp_workspace->cpu_info[cpu_id].online;
}