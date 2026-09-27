//
// isr.c -- High level interrupt service routines and interrupt request handlers.
//          Part of this code is modified from Bran's kernel development tutorials.
//          Rewritten for JamesM's kernel development tutorials.
//

#include "common.h"
#include "isr.h"
#include "graphics.h"
#include "syscall.h"
isr_t interrupt_handlers[256];
volatile int ide_irq_ready = 0;
void register_interrupt_handler(uint8_t n, isr_t handler)
{
    interrupt_handlers[n] = handler;
}
unsigned char *exception_messages[] =
    {
        "Division By Zero",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Into Detected Overflow",
        "Out of Bounds",
        "Invalid Opcode",
        "No Coprocessor",

        "Double Fault",
        "Coprocessor Segment Overrun",
        "Bad TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection Fault",
        "Page Fault",
        "Unknown Interrupt",

        "Coprocessor Fault",
        "Alignment Check",
        "Machine Check",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",

        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved"};
// This gets called from our ASM interrupt handler stub.
void isr_handler(registers_t regs)
{
    // This line is important. When the processor extends the 8-bit interrupt number
    // to a 32bit value, it sign-extends, not zero extends. So if the most significant
    // bit (0x80) is set, regs.int_no will be very large (about 0xffffff80).
    uint8_t int_no = regs.int_no & 0xFF;

    //
    if (interrupt_handlers[int_no] != 0)
    {

        isr_t handler = interrupt_handlers[int_no];
        handler(&regs);
    }
    else
    {
        if (int_no < 32)
        {
            puts(exception_messages[int_no]);
            
            printf("Interrupt Number %02X (%u)\n", regs.int_no, regs.int_no);
            printf("EIP %08X\n", regs.eip);
            printf("ESP %08X\n", regs.esp);
            printf("EBP %08X\n", regs.ebp);
            printf("EAX %08X\n", regs.eax);
            printf("EBX %08X\n", regs.ebx);
            printf("ECX %08X\n", regs.ecx);
            printf("EDX %08X\n", regs.edx);
            printf("ESI %08X\n", regs.esi);
            printf("EDI %08X\n", regs.edi);
            puts("System halted\n");
        }
        else
        {
            printf("unhandled interrupt %u (%02x):\n", int_no, int_no);
        }
        disable_interrupts();
        halt_cpu();
        for (;;)
            ;
    }
}

// This gets called from our ASM interrupt handler stub.
void irq_handler(registers_t regs)
{
    int32_t actual_int = regs.int_no; //- 32;
                                      // monitor_write_dec(actual_int);
                                      // os_puts("\n");
    // Send an EOI (end of interrupt) signal to the PICs.
    // If this interrupt involved the slave.

    int actual_int_no = actual_int - 32;
    // should be handled by ide driver?
    if (actual_int_no == 14 || actual_int_no == 15)
    {
        ide_irq_ready = 1;
    }
    if (interrupt_handlers[actual_int] != 0)
    {

        isr_t handler = interrupt_handlers[actual_int];
        handler(&regs);
    }
    else
    {

        //  syscall_os_puts("no handler for");
        //  monitor_write_dec(actual_int );
        //   syscall_os_puts("\n");
    }
    if (regs.int_no >= 40)
    {
        // Send reset signal to slave.
        outb(0xA0, 0x20);
    }
    // Send reset signal to master. (As well as slave, if necessary).
    outb(0x20, 0x20);
}
