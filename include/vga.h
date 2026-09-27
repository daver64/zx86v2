// vga.h -- Defines the interface for vga.h
//              From JamesM's kernel development tutorials.

#ifndef MONITOR_H
#define MONITOR_H

#include "common.h"
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

void scroll();
void cls();
void gotoxy(int x,int y);
uint8_t get_foreground_colour();
void set_foreground_colour(unsigned char colour);
void set_background_colour(unsigned char colour);
uint8_t get_foreground_colour();
uint8_t get_background_colour();
void set_text_colour(unsigned char forecolour, unsigned char backcolour);
int get_num_rows();
int get_num_columns();
unsigned short *video_mem_ptr();
// Write a single character out to the screen.
int os_putc(int c);
int puts(const char *text);
void disable_cursor();
void enable_cursor();
void sync_cursor_position();

int get_cursor_x();
int get_cursor_y();
uint32_t vga_colour_to_rgb(unsigned char vga_colour);
#endif // MONITOR_H
