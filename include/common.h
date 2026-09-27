// common.h -- Defines typedefs and some global functions.
//             From JamesM's kernel development tutorials.

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdatomic.h>
#define KEYBUF_SIZE 128
#define VK_NONE 0xFF
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_SPACE 0x20

#define VK_ESCAPE 0x1B
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 128
#define VK_UP 130
#define VK_RIGHT 129
#define VK_DOWN 140
#define VK_PRINT 0x2A
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E

#define VK_KEY_0 0x30
#define VK_KEY_1 0x31
#define VK_KEY_2 0x32
#define VK_KEY_3 0x33
#define VK_KEY_4 0x34
#define VK_KEY_5 0x35
#define VK_KEY_6 0x36
#define VK_KEY_7 0x37
#define VK_KEY_8 0x38
#define VK_KEY_9 0x39

#define VK_KEY_A 0x41
#define VK_KEY_B 0x42
#define VK_KEY_C 0x43
#define VK_KEY_D 0x44
#define VK_KEY_E 0x45
#define VK_KEY_F 0x46
#define VK_KEY_G 0x47
#define VK_KEY_H 0x48
#define VK_KEY_I 0x49
#define VK_KEY_J 0x4A
#define VK_KEY_K 0x4B
#define VK_KEY_L 0x4C
#define VK_KEY_M 0x4D
#define VK_KEY_N 0x4E
#define VK_KEY_O 0x4F
#define VK_KEY_P 0x50
#define VK_KEY_Q 0x51
#define VK_KEY_R 0x52
#define VK_KEY_S 0x53
#define VK_KEY_T 0x54
#define VK_KEY_U 0x55
#define VK_KEY_V 0x56
#define VK_KEY_W 0x57
#define VK_KEY_X 0x58
#define VK_KEY_Y 0x59
#define VK_KEY_Z 0x5A

#define VK_SLEEP 0x5F

#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69

#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B

#define VK_F1 141
#define VK_F2 142
#define VK_F3 143
#define VK_F4 144
#define VK_F5 145
#define VK_F6 146
#define VK_F7 147
#define VK_F8 148
#define VK_F9 149
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
void outb(uint16_t port, uint8_t value);
void outw(uint16_t port, uint16_t value);
void outl(uint16_t port, uint32_t value);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

#define PANIC(msg) panic(msg, __FILE__, __LINE__);
#define ASSERT(b) ((b) ? (void)0 : panic_assert(__FILE__, __LINE__, #b))

extern void panic(const char *message, const char *file, uint32_t line);
extern void panic_assert(const char *file, uint32_t line, const char *desc);
void os_puts(const char *c);
void monitor_write_hex(uint32_t n);
void monitor_write_dec(uint32_t n);
void malloc_init();
int32_t get_tick_count();
const char *os_asctime();
const char *asciidate();
const char *asciidatetime();
int32_t string_split(const char *txt, char delim, char ***tokens);
uint8_t hdc_identify_primary();
uint32_t hdc_read_sectors(uint64_t LBA, uint8_t sectorcount, void *t);
uint32_t hdc_write_sectors(uint64_t LBA, uint8_t sectorcount, void *t);
uint32_t rdc_read_sectors(uint64_t LBA, uint8_t sectorcount, void *t);
uint32_t rdc_write_sectors(uint64_t LBA, uint8_t sectorcount, void *t);
void ungetc(int k,FILE* fp);
int kbhit();
/* DFS.C */
unsigned int os_mount();
unsigned int os_unmount();

// Serial port functions
void serial_puts(const char *msg);

uint32_t kmalloc(uint32_t);
uint32_t kmalloc_a(uint32_t);
void set_kernel_stack(uint32_t);
void switch_to_user_mode();
void initialise_keyboard();
void enable_interrupts();
void disable_interrupts();
void halt_cpu();
void pause_cpu();
void invlpg(uint32_t addr);
void restore_irqs(uint32_t flags);
uint32_t save_irqdisable();
bool are_interrupts_enabled();
void io_wait();
uint64_t rdmsr(uint32_t msr_id);
void wrmsr(uint32_t msr_id, uint64_t msr_value);
void enable_paging();
void disable_paging();
void disasm(unsigned char *code, uint32_t codelength);
typedef unsigned long jmp_buf[6];
int setjmp(jmp_buf var);
void longjmp(jmp_buf var,int m);
void os_report_space();
uint32_t next_pow2(uint32_t x);
void *virtual_alloc(uint32_t address,size_t numpages);
void virtual_free(void *ptr);
void acquire_mutex(atomic_flag *lock);
void release_mutex(atomic_flag *lock);
#define KEYBUF_SIZE 128
#define VK_NONE 0xFF
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_SPACE 0x20

#define VK_ESCAPE 0x1B
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 128
#define VK_UP 130
#define VK_RIGHT 129
#define VK_DOWN 140
#define VK_PRINT 0x2A
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E

#define VK_KEY_0 0x30
#define VK_KEY_1 0x31
#define VK_KEY_2 0x32
#define VK_KEY_3 0x33
#define VK_KEY_4 0x34
#define VK_KEY_5 0x35
#define VK_KEY_6 0x36
#define VK_KEY_7 0x37
#define VK_KEY_8 0x38
#define VK_KEY_9 0x39

#define VK_KEY_A 0x41
#define VK_KEY_B 0x42
#define VK_KEY_C 0x43
#define VK_KEY_D 0x44
#define VK_KEY_E 0x45
#define VK_KEY_F 0x46
#define VK_KEY_G 0x47
#define VK_KEY_H 0x48
#define VK_KEY_I 0x49
#define VK_KEY_J 0x4A
#define VK_KEY_K 0x4B
#define VK_KEY_L 0x4C
#define VK_KEY_M 0x4D
#define VK_KEY_N 0x4E
#define VK_KEY_O 0x4F
#define VK_KEY_P 0x50
#define VK_KEY_Q 0x51
#define VK_KEY_R 0x52
#define VK_KEY_S 0x53
#define VK_KEY_T 0x54
#define VK_KEY_U 0x55
#define VK_KEY_V 0x56
#define VK_KEY_W 0x57
#define VK_KEY_X 0x58
#define VK_KEY_Y 0x59
#define VK_KEY_Z 0x5A

#define VK_SLEEP 0x5F
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69

#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B

#define VK_F1 141
#define VK_F2 142
#define VK_F3 143
#define VK_F4 144
#define VK_F5 145
#define VK_F6 146
#define VK_F7 147
#define VK_F8 148
#define VK_F9 149
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B

#define VGA_BLACK 0x00
#define VGA_BLUE 0x01
#define VGA_GREEN 0x02
#define VGA_CYAN 0x03
#define VGA_RED 0x04
#define VGA_MAGENTA 0x05
#define VGA_BROWN 0x06
#define VGA_LIGHTGREY 0x07
#define VGA_DARKGREY 0x08
#define VGA_LIGHTBLUE 0x09
#define VGA_LIGHTGREEN 0x0A
#define VGA_LIGHTCYAN 0x0B
#define VGA_LIGHTRED 0x0C
#define VGA_LIGHTMAGENTA 0x0D
#define VGA_YELLOW 0x0E
#define VGA_WHITE 0x0F

#endif // COMMON_H
