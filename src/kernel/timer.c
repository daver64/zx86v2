// timer.c -- Initialises the PIT, and handles clock updates.
//            Written for JamesM's kernel development tutorials.

#include "timer.h"
#include "isr.h"
#include "graphics.h"
#include "amp.h"
void switch_task();
uint32_t tick = 0;

static void timer_callback(registers_t *regs)
{
    tick++;
    
    // Check shared memory for inter-CPU communication
    extern uint8_t amp_get_current_cpu_id(void);
    extern void amp_check_shared_memory(void);
    extern void serial_puts(const char *msg);
    extern int sprintf(char *str, const char *format, ...);
    extern int amp_ensure_shared_memory_mapped(void);
    
    // CPU0 no longer participates in AMP shared memory operations
    // This prevents page faults since CPU0 is not part of AMP workspace
    
    // Process deferred page table cleanup every 10 ticks (safe timer context)
    if (tick % 10 == 0) {
        extern void timer_process_deferred_cleanup(void);
        timer_process_deferred_cleanup();
        
        // Also process any deferred executions periodically
        extern void process_deferred_executions(void);
        process_deferred_executions();
    }
    
    // Check for pending process execution request (every tick for immediate response)  
    extern volatile int pending_process_pid;
    static int last_announced_pid = 0;
    if (pending_process_pid > 0 && pending_process_pid != last_announced_pid) {
        // Signal that a process is ready - only announce once per process
        last_announced_pid = pending_process_pid;
        
        // Schedule deferred execution - safer than executing in interrupt context
        extern void schedule_deferred_process_execution(int pid);
        schedule_deferred_process_execution(pending_process_pid);
        pending_process_pid = 0; // Clear flag immediately
    } else if (pending_process_pid == 0) {
        // Reset tracking when flag is cleared
        last_announced_pid = 0;
    }
    
    // No deferred output processing - os_puts is called directly from syscall handler
    
    switch_task();
}
int32_t get_tick_count()
{
    return tick;
}
void initialise_timer(uint32_t frequency)
{
    // Firstly, register our timer callback.
    register_interrupt_handler(IRQ0, &timer_callback);

    // The value we send to the PIT is the value to divide it's input clock
    // (1193180 Hz) by, to get our required frequency. Important to note is
    // that the divisor must be small enough to fit into 16-bits.
    uint32_t divisor = 1193180 / frequency;

    // Send the command byte.
    outb(0x43, 0x36);

    // Divisor has to be sent byte-wise, so split here into upper/lower bytes.
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );

    // Send the frequency divisor.
    outb(0x40, l);
    outb(0x40, h);
}
