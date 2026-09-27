
#include "graphics.h"
#include "common.h"
#include "ff.h"
#include "diskio.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

// Serial debug function
extern void serial_puts(const char *msg);

// Convert RGB to RGBA format used by framebuffer
uint32_t rgb_to_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Create a new bitmap in memory
bmp_image_t* bmp_create(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return NULL;
    }
    
    bmp_image_t* image = malloc(sizeof(bmp_image_t));
    if (!image) {
        return NULL;
    }
    
    image->width = width;
    image->height = height;
    image->bits_per_pixel = 32;
    image->top_down = true;
    image->data_size = width * height * 4;
    
    image->pixel_data = malloc(image->data_size);
    if (!image->pixel_data) {
        free(image);
        return NULL;
    }
    
    // Initialize to transparent black
    for (uint32_t i = 0; i < width * height; i++) {
        image->pixel_data[i] = 0;
    }
    
    return image;
}

// Load 24-bit BMP data
static bool load_24bit_bmp(FIL* file, bmp_image_t* image, uint32_t data_offset) {
    char debug_buf[256];
    // Seek to pixel data
    f_lseek(file, data_offset);
    
    uint32_t row_size = ((image->width * 3 + 3) / 4) * 4; // Row must be 4-byte aligned
    uint32_t padding = row_size - (image->width * 3);
    
    uint8_t* row_buffer = malloc(row_size);
    if (!row_buffer) {
        sprintf(debug_buf, "SERIAL: BMP Load Error: Could not allocate row buffer (%d bytes)\n", row_size);
        serial_puts(debug_buf);
        return false;
    }
    
    // Read pixel data row by row
    for (int y = 0; y < image->height; y++) {
        UINT bytes_read;
        f_read(file, row_buffer, row_size, &bytes_read);
        
        if (bytes_read != row_size) {
            sprintf(debug_buf, "SERIAL: BMP Load Error: Row %d read failed (got %d, expected %d)\n", y, bytes_read, row_size);
            serial_puts(debug_buf);
            free(row_buffer);
            return false;
        }
        
        // Determine actual row index (BMPs are bottom-up by default)
        int actual_y = image->top_down ? y : (image->height - 1 - y);
        uint32_t* pixel_row = &image->pixel_data[actual_y * image->width];
        
        // Convert BGR to RGBA
        for (int x = 0; x < image->width; x++) {
            uint8_t b = row_buffer[x * 3 + 0];
            uint8_t g = row_buffer[x * 3 + 1];
            uint8_t r = row_buffer[x * 3 + 2];
            pixel_row[x] = rgb_to_rgba(r, g, b, 255); // Full alpha
        }
    }
    
    free(row_buffer);
    return true;
}

// Load 32-bit BMP data
static bool load_32bit_bmp(FIL* file, bmp_image_t* image, uint32_t data_offset) {
    char debug_buf[256];
    // Seek to pixel data
    f_lseek(file, data_offset);
    
    uint32_t row_size = image->width * 4; // 32-bit = 4 bytes per pixel
    
    uint8_t* row_buffer = malloc(row_size);
    if (!row_buffer) {
        sprintf(debug_buf, "SERIAL: BMP Load Error: Could not allocate row buffer (%d bytes)\n", row_size);
        serial_puts(debug_buf);
        return false;
    }
    
    // Read pixel data row by row
    for (int y = 0; y < image->height; y++) {
        UINT bytes_read;
        f_read(file, row_buffer, row_size, &bytes_read);
        
        if (bytes_read != row_size) {
            sprintf(debug_buf, "SERIAL: BMP Load Error: Row %d read failed (got %d, expected %d)\n", y, bytes_read, row_size);
            serial_puts(debug_buf);
            free(row_buffer);
            return false;
        }
        
        // Determine actual row index (BMPs are bottom-up by default)
        int actual_y = image->top_down ? y : (image->height - 1 - y);
        uint32_t* pixel_row = &image->pixel_data[actual_y * image->width];
        
        // Convert BGRA to RGBA
        for (int x = 0; x < image->width; x++) {
            uint8_t b = row_buffer[x * 4 + 0];
            uint8_t g = row_buffer[x * 4 + 1];
            uint8_t r = row_buffer[x * 4 + 2];
            uint8_t a = row_buffer[x * 4 + 3];
            pixel_row[x] = rgb_to_rgba(r, g, b, a);
        }
    }
    
    free(row_buffer);
    return true;
}

bmp_image_t* bmp_load_file(const char* filename) {
    char debug_buf[256];
    FIL file;
    FRESULT result = f_open(&file, filename, FA_READ);
    if (result != FR_OK) {
        sprintf(debug_buf, "SERIAL: BMP Load Error: Could not open file '%s' (FRESULT=%d)\n", filename, result);
        serial_puts(debug_buf);
        return NULL;
    }
    
    // Read file header
    bmp_file_header_t file_header;
    UINT bytes_read;
    result = f_read(&file, &file_header, sizeof(file_header), &bytes_read);
    if (result != FR_OK || bytes_read != sizeof(file_header)) {
        sprintf(debug_buf, "SERIAL: BMP Load Error: Could not read file header (FRESULT=%d, bytes=%d)\n", result, bytes_read);
        serial_puts(debug_buf);
        f_close(&file);
        return NULL;
    }
    
    // Validate BMP signature
    if (file_header.signature != 0x4D42) { // "BM" in little-endian
        sprintf(debug_buf, "SERIAL: BMP Load Error: Invalid signature 0x%04X (expected 0x4D42)\n", file_header.signature);
        serial_puts(debug_buf);
        f_close(&file);
        return NULL;
    }
    
    // Read info header
    bmp_info_header_t info_header;
    result = f_read(&file, &info_header, sizeof(info_header), &bytes_read);
    if (result != FR_OK || bytes_read != sizeof(info_header)) {
        sprintf(debug_buf, "SERIAL: BMP Load Error: Could not read info header (FRESULT=%d, bytes=%d)\n", result, bytes_read);
        serial_puts(debug_buf);
        f_close(&file);
        return NULL;
    }
    
    // Validate format
    if (info_header.header_size != 40) {
        sprintf(debug_buf, "SERIAL: BMP Load Error: Unsupported header size %d (expected 40)\n", info_header.header_size);
        serial_puts(debug_buf);
        f_close(&file);
        return NULL;
    }
    
    if (info_header.planes != 1) {
        sprintf(debug_buf, "SERIAL: BMP Load Error: Invalid planes %d (expected 1)\n", info_header.planes);
        serial_puts(debug_buf);
        f_close(&file);
        return NULL;
    }
    
    if (info_header.compression != 0) {
        sprintf(debug_buf, "SERIAL: BMP Load Error: Compression not supported (compression=%d)\n", info_header.compression);
        serial_puts(debug_buf);
        f_close(&file);
        return NULL;
    }
    
    if (info_header.bits_per_pixel != 24 && info_header.bits_per_pixel != 32) {
        sprintf(debug_buf, "SERIAL: BMP Load Error: Unsupported bit depth %d (only 24 and 32 supported)\n", info_header.bits_per_pixel);
        serial_puts(debug_buf);
        f_close(&file);
        return NULL;
    }
    
    // Create image structure
    bmp_image_t* image = malloc(sizeof(bmp_image_t));
    if (!image) {
        serial_puts("SERIAL: BMP Load Error: Could not allocate image structure\n");
        f_close(&file);
        return NULL;
    }
    
    image->width = info_header.width;
    image->height = info_header.height < 0 ? -info_header.height : info_header.height;
    image->top_down = info_header.height < 0;
    image->bits_per_pixel = info_header.bits_per_pixel;
    image->data_size = image->width * image->height * 4; // Always RGBA
    
    // Allocate pixel data
    image->pixel_data = malloc(image->data_size);
    if (!image->pixel_data) {
        sprintf(debug_buf, "SERIAL: BMP Load Error: Could not allocate pixel data (%d bytes)\n", image->data_size);
        serial_puts(debug_buf);
        free(image);
        f_close(&file);
        return NULL;
    }
    
    // Load pixel data based on bit depth
    bool success = false;
    if (info_header.bits_per_pixel == 24) {
        success = load_24bit_bmp(&file, image, file_header.data_offset);
    } else if (info_header.bits_per_pixel == 32) {
        success = load_32bit_bmp(&file, image, file_header.data_offset);
    }
    
    f_close(&file);
    
    if (!success) {
        serial_puts("SERIAL: BMP Load Error: Failed to load pixel data\n");
        free(image->pixel_data);
        free(image);
        return NULL;
    }
    
    return image;
}

void bmp_free(bmp_image_t* image) {
    if (image) {
        if (image->pixel_data) {
            free(image->pixel_data);
        }
        free(image);
    }
}

bool bmp_display(bmp_image_t* image, int x, int y) {
    if (!image || !image->pixel_data) {
        return false;
    }
    
    // Get graphics mode info
    graphics_mode_t* mode = graphics_get_mode();
    if (!mode || !mode->enabled) {
        return false;
    }
    
    // Clip to screen bounds
    int start_x = x < 0 ? -x : 0;
    int start_y = y < 0 ? -y : 0;
    int end_x = (x + image->width > mode->width) ? mode->width - x : image->width;
    int end_y = (y + image->height > mode->height) ? mode->height - y : image->height;
    
    if (start_x >= end_x || start_y >= end_y) {
        return false; // Completely off-screen
    }
    
    // Copy pixels to framebuffer using standard framebuffer functions
    extern void fb_put_pixel(int x, int y, uint32_t colour);
    extern uint32_t fb_get_pixel(int x, int y);
    
    for (int img_y = start_y; img_y < end_y; img_y++) {
        uint32_t* src_row = &image->pixel_data[img_y * image->width];
        
        for (int img_x = start_x; img_x < end_x; img_x++) {
            uint32_t pixel = src_row[img_x];
            
            // Handle alpha blending if needed (for 32-bit images)
            uint8_t alpha = (pixel >> 24) & 0xFF;
            if (alpha == 255) {
                // Fully opaque - direct copy
                fb_put_pixel(x + img_x, y + img_y, pixel);
            } else if (alpha > 0) {
                // Alpha blending
                uint32_t bg = fb_get_pixel(x + img_x, y + img_y);
                uint8_t bg_r = (bg >> 16) & 0xFF;
                uint8_t bg_g = (bg >> 8) & 0xFF;
                uint8_t bg_b = bg & 0xFF;
                
                uint8_t fg_r = (pixel >> 16) & 0xFF;
                uint8_t fg_g = (pixel >> 8) & 0xFF;
                uint8_t fg_b = pixel & 0xFF;
                
                uint8_t out_r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
                uint8_t out_g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
                uint8_t out_b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;
                
                uint32_t blended = rgb_to_rgba(out_r, out_g, out_b, 255);
                fb_put_pixel(x + img_x, y + img_y, blended);
            }
            // If alpha == 0, skip (transparent)
        }
    }
    
    return true;
}

bool bmp_display_scaled(bmp_image_t* image, int x, int y, int scale_x, int scale_y) {
    if (!image || !image->pixel_data || scale_x <= 0 || scale_y <= 0) {
        return false;
    }
    
    // Get graphics mode info
    graphics_mode_t* mode = graphics_get_mode();
    if (!mode || !mode->enabled) {
        return false;
    }
    
    int scaled_width = image->width * scale_x;
    int scaled_height = image->height * scale_y;
    
    // Clip to screen bounds
    int start_x = x < 0 ? -x : 0;
    int start_y = y < 0 ? -y : 0;
    int end_x = (x + scaled_width > mode->width) ? mode->width - x : scaled_width;
    int end_y = (y + scaled_height > mode->height) ? mode->height - y : scaled_height;
    
    if (start_x >= end_x || start_y >= end_y) {
        return false;
    }
    
    // Simple nearest-neighbor scaling with standard framebuffer calls
    extern void fb_put_pixel(int x, int y, uint32_t colour);
    
    for (int screen_y = start_y; screen_y < end_y; screen_y++) {
        int img_y = screen_y / scale_y;
        uint32_t* src_row = &image->pixel_data[img_y * image->width];
        
        for (int screen_x = start_x; screen_x < end_x; screen_x++) {
            int img_x = screen_x / scale_x;
            uint32_t pixel = src_row[img_x];
            
            // Handle transparency
            uint8_t alpha = (pixel >> 24) & 0xFF;
            if (alpha > 0) {
                fb_put_pixel(x + screen_x, y + screen_y, pixel);
            }
        }
    }
    
    return true;
}

bool bmp_save_file(const char* filename, bmp_image_t* image) {
    if (!image || !image->pixel_data) {
        return false;
    }
    
    // Disable async I/O for file operations (like ls, cd, mkdir)
    set_disk_sync_mode(1);
    
    FIL file;
    FRESULT result = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (result != FR_OK) {
        // Re-enable async I/O before returning
        set_disk_sync_mode(0);
        return false;
    }
    
    // Calculate file size
    uint32_t row_size = ((image->width * 4 + 3) / 4) * 4; // 32-bit, 4-byte aligned
    uint32_t image_size = row_size * image->height;
    uint32_t file_size = 54 + image_size; // 14 + 40 + image_size
    
    // Write file header manually (byte by byte to avoid alignment issues)
    uint8_t header_buffer[54]; // 14 + 40 bytes for headers
    uint8_t* ptr = header_buffer;
    
    // BMP file header (14 bytes)
    *ptr++ = 0x42; *ptr++ = 0x4D;                           // "BM" signature
    *(uint32_t*)ptr = file_size; ptr += 4;                  // File size
    *(uint16_t*)ptr = 0; ptr += 2;                         // Reserved1
    *(uint16_t*)ptr = 0; ptr += 2;                         // Reserved2
    *(uint32_t*)ptr = 54; ptr += 4;                        // Data offset (14 + 40)
    
    // BMP info header (40 bytes)
    *(uint32_t*)ptr = 40; ptr += 4;                        // Header size
    *(uint32_t*)ptr = image->width; ptr += 4;              // Width
    *(int32_t*)ptr = -(int32_t)image->height; ptr += 4;    // Height (negative for top-down)
    *(uint16_t*)ptr = 1; ptr += 2;                         // Planes
    *(uint16_t*)ptr = 32; ptr += 2;                        // Bits per pixel
    *(uint32_t*)ptr = 0; ptr += 4;                         // Compression
    *(uint32_t*)ptr = image_size; ptr += 4;                // Image size
    *(uint32_t*)ptr = 2835; ptr += 4;                      // X pixels per meter
    *(uint32_t*)ptr = 2835; ptr += 4;                      // Y pixels per meter
    *(uint32_t*)ptr = 0; ptr += 4;                         // Colors used
    *(uint32_t*)ptr = 0; ptr += 4;                         // Important colors
    
    // Write the combined header
    UINT bytes_written;
    result = f_write(&file, header_buffer, 54, &bytes_written);
    if (result != FR_OK || bytes_written != 54) {
        f_close(&file);
        set_disk_sync_mode(0);
        return false;
    }
    
    // Write pixel data
    uint8_t* row_buffer = malloc(row_size);
    if (!row_buffer) {
        serial_puts("SERIAL: BMP Save Error: Failed to allocate row buffer\n");
        f_close(&file);
        // Re-enable async I/O before returning
        set_disk_sync_mode(0);
        return false;
    }
    
    serial_puts("SERIAL: BMP Save: Starting pixel data write...\n");
    
    for (int y = 0; y < image->height; y++) {
        uint32_t* src_row = &image->pixel_data[y * image->width];
        
        // Convert RGBA to BGRA and write row
        for (int x = 0; x < image->width; x++) {
            uint32_t pixel = src_row[x];
            row_buffer[x * 4 + 0] = (pixel >> 8) & 0xFF;  // B
            row_buffer[x * 4 + 1] = (pixel >> 16) & 0xFF; // G
            row_buffer[x * 4 + 2] = (pixel >> 24) & 0xFF; // R
            row_buffer[x * 4 + 3] = pixel & 0xFF;         // A
        }
        
        // Clear padding bytes
        for (int p = image->width * 4; p < row_size; p++) {
            row_buffer[p] = 0;
        }
        
        result = f_write(&file, row_buffer, row_size, &bytes_written);
        if (result != FR_OK || bytes_written != row_size) {
            free(row_buffer);
            f_close(&file);
            set_disk_sync_mode(0);
            return false;
        }
        
        // Sync every 10 rows to avoid buffer overflow
        if ((y % 10) == 9) {
            result = f_sync(&file);
            if (result != FR_OK) {
                free(row_buffer);
                f_close(&file);
                set_disk_sync_mode(0);
                return false;
            }
        }
    }
    
    // Final sync before closing
    result = f_sync(&file);
    if (result != FR_OK) {
        free(row_buffer);
        f_close(&file);
        set_disk_sync_mode(0);
        return false;
    }
    
    free(row_buffer);
    f_close(&file);
    
    // Re-enable async I/O
    set_disk_sync_mode(0);
    
    return true;
}

void bmp_putpixel(bmp_image_t* image, int x, int y, uint32_t colour) {
    if (!image || !image->pixel_data || x < 0 || y < 0 || x >= image->width || y >= image->height) {
        return;
    }
    
    image->pixel_data[y * image->width + x] = colour;
}

void bmp_line(bmp_image_t* image, int x0, int y0, int x1, int y1, uint32_t colour) {
    if (!image || !image->pixel_data) {
        return;
    }
    
    // Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    int x = x0, y = y0;
    
    while (true) {
        bmp_putpixel(image, x, y, colour);
        
        if (x == x1 && y == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void bmp_rect(bmp_image_t* image, int x, int y, int width, int height, uint32_t colour) {
    if (!image || !image->pixel_data || width <= 0 || height <= 0) {
        return;
    }
    
    // Draw four sides of rectangle
    bmp_line(image, x, y, x + width - 1, y, colour);                    // Top
    bmp_line(image, x, y + height - 1, x + width - 1, y + height - 1, colour); // Bottom
    bmp_line(image, x, y, x, y + height - 1, colour);                   // Left
    bmp_line(image, x + width - 1, y, x + width - 1, y + height - 1, colour);  // Right
}

void bmp_fillrect(bmp_image_t* image, int x, int y, int width, int height, uint32_t colour) {
    if (!image || !image->pixel_data || width <= 0 || height <= 0) {
        return;
    }
    
    // Clip to image bounds
    int start_x = x < 0 ? 0 : x;
    int start_y = y < 0 ? 0 : y;
    int end_x = (x + width > image->width) ? image->width : x + width;
    int end_y = (y + height > image->height) ? image->height : y + height;
    
    if (start_x >= end_x || start_y >= end_y) {
        return;
    }
    
    for (int py = start_y; py < end_y; py++) {
        for (int px = start_x; px < end_x; px++) {
            image->pixel_data[py * image->width + px] = colour;
        }
    }
}

void bmp_circle(bmp_image_t* image, int center_x, int center_y, int radius, uint32_t colour) {
    if (!image || !image->pixel_data || radius <= 0) {
        return;
    }
    
    // Midpoint circle algorithm
    int x = radius;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        bmp_putpixel(image, center_x + x, center_y + y, colour);
        bmp_putpixel(image, center_x + y, center_y + x, colour);
        bmp_putpixel(image, center_x - y, center_y + x, colour);
        bmp_putpixel(image, center_x - x, center_y + y, colour);
        bmp_putpixel(image, center_x - x, center_y - y, colour);
        bmp_putpixel(image, center_x - y, center_y - x, colour);
        bmp_putpixel(image, center_x + y, center_y - x, colour);
        bmp_putpixel(image, center_x + x, center_y - y, colour);
        
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

void bmp_fillcircle(bmp_image_t* image, int center_x, int center_y, int radius, uint32_t colour) {
    if (!image || !image->pixel_data || radius <= 0) {
        return;
    }
    
    // Fill circle using horizontal lines
    for (int y = -radius; y <= radius; y++) {
        int x = (int)sqrt(radius * radius - y * y);
        bmp_line(image, center_x - x, center_y + y, center_x + x, center_y + y, colour);
    }
}

bool bmp_blit(bmp_image_t* dest, int x, int y, int src_x, int src_y, int w, int h, bmp_image_t* src) {
    if (!dest || !src || !dest->pixel_data || !src->pixel_data) {
        return false;
    }
    
    if (w <= 0 || h <= 0) {
        return false;
    }
    
    // Clip source rectangle
    if (src_x < 0) { w += src_x; x -= src_x; src_x = 0; }
    if (src_y < 0) { h += src_y; y -= src_y; src_y = 0; }
    if (src_x + w > src->width) { w = src->width - src_x; }
    if (src_y + h > src->height) { h = src->height - src_y; }
    
    // Clip destination rectangle
    if (x < 0) { w += x; src_x -= x; x = 0; }
    if (y < 0) { h += y; src_y -= y; y = 0; }
    if (x + w > dest->width) { w = dest->width - x; }
    if (y + h > dest->height) { h = dest->height - y; }
    
    if (w <= 0 || h <= 0) {
        return false;
    }
    
    // Copy pixels
    for (int py = 0; py < h; py++) {
        uint32_t* src_row = &src->pixel_data[(src_y + py) * src->width + src_x];
        uint32_t* dest_row = &dest->pixel_data[(y + py) * dest->width + x];
        
        for (int px = 0; px < w; px++) {
            uint32_t pixel = src_row[px];
            uint8_t alpha = pixel & 0xFF;
            
            if (alpha == 255) {
                // Fully opaque - direct copy
                dest_row[px] = pixel;
            } else if (alpha > 0) {
                // Alpha blending
                uint32_t bg = dest_row[px];
                uint8_t bg_r = (bg >> 24) & 0xFF;
                uint8_t bg_g = (bg >> 16) & 0xFF;
                uint8_t bg_b = (bg >> 8) & 0xFF;
                
                uint8_t fg_r = (pixel >> 24) & 0xFF;
                uint8_t fg_g = (pixel >> 16) & 0xFF;
                uint8_t fg_b = (pixel >> 8) & 0xFF;
                
                uint8_t out_r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
                uint8_t out_g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
                uint8_t out_b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;
                
                dest_row[px] = rgb_to_rgba(out_r, out_g, out_b, 255);
            }
            // If alpha == 0, skip (transparent)
        }
    }
    
    return true;
}

void bmp_clear(bmp_image_t* image, uint32_t colour) {
    if (!image || !image->pixel_data) {
        return;
    }
    
    for (uint32_t i = 0; i < image->width * image->height; i++) {
        image->pixel_data[i] = colour;
    }
}