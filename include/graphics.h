// graphics.h -- Graphics subsystem for ZX86v2
// Includes framebuffer, fonts, and terminal support

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "common.h"

// Forward declaration
struct multiboot;

// Graphics mode information
typedef struct
{
    uint32_t framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
    bool enabled;
    // Double buffering support
    uint32_t back_buffer;
    bool double_buffering_enabled;
    bool render_to_back_buffer;
} graphics_mode_t;

// Colour definitions (32-bit ARGB)
#define COLOUR_BLACK 0xFF000000
#define COLOUR_WHITE 0xFFFFFFFF
#define COLOUR_RED 0xFFFF0000
#define COLOUR_GREEN 0xFF00FF00
#define COLOUR_BLUE 0xFF0000FF

// Allegro4-style color composition and extraction macros
// These work with 32-bit RGBA pixels (0xRRGGBBAA format)
#define rgba(r, g, b, a) (((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(a))
#define rgb(r, g, b) rgba(r, g, b, 255)

// Color component extraction macros
#define getr(color) (((color) >> 24) & 0xFF)
#define getg(color) (((color) >> 16) & 0xFF)
#define getb(color) (((color) >> 8) & 0xFF)
#define geta(color) ((color) & 0xFF)
#define COLOUR_YELLOW 0xFFFFFF00
#define COLOUR_CYAN 0xFF00FFFF
#define COLOUR_MAGENTA 0xFFFF00FF
#define COLOUR_GRAY 0xFF808080
#define COLOUR_DARK_GRAY 0xFF404040

// Font information
#define FONT_WIDTH 8
#define FONT_HEIGHT 16



// Dynamic terminal dimension functions
int terminal_get_cols();
int terminal_get_rows();

// Graphics functions
bool graphics_init(uint16_t width, uint16_t height);
bool graphics_init_multiboot(struct multiboot *mboot_ptr);
bool graphics_init_fallback(uint16_t width, uint16_t height);
bool graphics_enabled();
graphics_mode_t *graphics_get_mode();
void qemu_vga_set_mode(uint16_t width, uint16_t height, uint8_t bpp);
bool graphics_set_mode(uint16_t width, uint16_t height, uint8_t bpp);

// Framebuffer functions
void fb_put_pixel(int x, int y, uint32_t colour);
uint32_t fb_get_pixel(int x, int y);
void fb_fill_rect(int x, int y, int width, int height, uint32_t colour);
void fb_copy_rect(int src_x, int src_y, int dst_x, int dst_y, int width, int height);
void fb_clear(uint32_t colour);

// Double buffering functions
bool fb_init_double_buffer();
void fb_swap_buffers();
void fb_enable_double_buffering();
void fb_disable_double_buffering();
void fb_set_render_target_back();
void fb_set_render_target_front();

// Font and text rendering
void font_draw_char(int x, int y, char c, uint32_t fg_colour, uint32_t bg_colour);
void font_draw_string(int x, int y, const char *str, uint32_t fg_colour, uint32_t bg_colour);

// Graphics terminal
void terminal_init();
void terminal_putc(char c);
void terminal_putc_immediate(char c);
void terminal_puts(const char *str);
void terminal_clear();
void terminal_scroll();
void terminal_flush();
void terminal_set_cursor(int x, int y);
void terminal_get_cursor(int *x, int *y);
void terminal_set_colors(uint32_t fg, uint32_t bg);
void terminal_set_fg_colour(uint32_t colour);
void terminal_set_bg_colour(uint32_t colour);

// Cursor functions
void terminal_draw_cursor();
void terminal_draw_cursor_solid();
void terminal_erase_cursor();
void terminal_update_cursor();
void terminal_show_cursor();
void terminal_hide_cursor();
void terminal_set_text_output_mode(bool mode);


// BMP file header (14 bytes)
typedef struct __attribute__((packed)) {
    uint16_t signature;     // "BM" (0x424D)
    uint32_t file_size;     // Total file size
    uint16_t reserved1;     // Reserved (0)
    uint16_t reserved2;     // Reserved (0)
    uint32_t data_offset;   // Offset to pixel data
} bmp_file_header_t;

// BMP info header (40 bytes for BITMAPINFOHEADER)
typedef struct __attribute__((packed)) {
    uint32_t header_size;   // Size of this header (40)
    int32_t width;          // Image width (signed)
    int32_t height;         // Image height (signed, negative = top-down)
    uint16_t planes;        // Number of color planes (must be 1)
    uint16_t bits_per_pixel; // Bits per pixel (24 or 32)
    uint32_t compression;   // Compression method (0 = none)
    uint32_t image_size;    // Size of image data (can be 0 for uncompressed)
    int32_t x_pixels_per_m; // Horizontal resolution
    int32_t y_pixels_per_m; // Vertical resolution
    uint32_t colors_used;   // Number of colors in palette
    uint32_t colors_important; // Important colors
} bmp_info_header_t;

// BMP image structure
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    bool top_down;          // true if negative height (top-down)
    uint32_t* pixel_data;   // RGBA pixel data (always converted to 32-bit)
    uint32_t data_size;     // Size of pixel_data in bytes
} bmp_image_t;

// Function declarations
bmp_image_t* bmp_create(uint32_t width, uint32_t height);
bmp_image_t* bmp_load_file(const char* filename);
void bmp_free(bmp_image_t* image);
bool bmp_display(bmp_image_t* image, int x, int y);
bool bmp_display_scaled(bmp_image_t* image, int x, int y, int scale_x, int scale_y);
bool bmp_save_file(const char* filename, bmp_image_t* image);
void bmp_putpixel(bmp_image_t* image, int x, int y, uint32_t colour);
void bmp_line(bmp_image_t* image, int x0, int y0, int x1, int y1, uint32_t colour);
void bmp_rect(bmp_image_t* image, int x, int y, int width, int height, uint32_t colour);
void bmp_fillrect(bmp_image_t* image, int x, int y, int width, int height, uint32_t colour);   
void bmp_circle(bmp_image_t* image, int center_x, int center_y, int radius, uint32_t colour);
void bmp_fillcircle(bmp_image_t* image, int center_x, int center_y, int radius, uint32_t colour);
bool bmp_blit(bmp_image_t* dest, int x, int y, int src_x, int src_y, int w, int h, bmp_image_t* src);
void bmp_clear(bmp_image_t* image, uint32_t colour);

// VGA compatibility functions (implemented in terminal.c)
void set_foreground_colour(unsigned char colour);
void set_background_colour(unsigned char colour);
uint8_t get_foreground_colour(void);
uint8_t get_background_colour(void);
void cls(void);
void gotoxy(int x, int y);
int get_cursor_x(void);
int get_cursor_y(void);
int os_putc(int c);
uint32_t vga_colour_to_rgb(unsigned char vga_colour);

// Additional compatibility functions
int putchar(int c);
int puts(const char* str);
void os_puts(const char* str);
int get_num_rows(void);
int get_num_columns(void);
unsigned short* video_mem_ptr(void);
void disable_cursor(void);
void enable_cursor(void);
void set_text_colour(unsigned char forecolour, unsigned char backcolour);

// VGA color constants for compatibility
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

// Utility functions
uint32_t rgb_to_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
#endif