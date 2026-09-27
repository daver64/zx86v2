#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define PORT 0x3f8 /* COM1 */

// Simple spinlock for serial output synchronization
static volatile uint32_t serial_lock = 0;

// Spinlock functions
static inline void spin_lock(volatile uint32_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        __asm__ volatile ("pause" ::: "memory");
    }
}

static inline void spin_unlock(volatile uint32_t *lock) {
    __sync_lock_release(lock);
}

void serial_install();
int serial_received();
char read_serial();
void write_serial(char a);
int is_transmit_empty();

void serial_install()
{
   outb(PORT + 1, 0x00); // Disable all interrupts
   outb(PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
   outb(PORT + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
   outb(PORT + 1, 0x00); //                  (hi byte)
   outb(PORT + 3, 0x03); // 8 bits, no parity, one stop bit
   outb(PORT + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
   outb(PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
                             // printf("Init Com1.\n");
}

int serial_received()
{
   return inb(PORT + 5) & 1;
}

char read_serial()
{
   while (serial_received() == 0)
      ;
   return inb(PORT);
}
int is_transmit_empty()
{
   return inb(PORT + 5) & 0x20;
}

void write_serial(char a)
{
   while (is_transmit_empty() == 0)
      ;
   outb(PORT, a);
}

void serial_puts(const char *msg)
{
   spin_lock(&serial_lock);   // Acquire lock before writing
   
   while (*msg != '\0')
   {
      write_serial(*msg);
      msg++;
   }
   
   spin_unlock(&serial_lock); // Release lock after writing
}
