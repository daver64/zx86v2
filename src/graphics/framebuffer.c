// framebuffer.c -- Framebuffer management for ZX86v2 graphics
// Provides pixel-level operations on the graphics framebuffer

#include "graphics.h"
#include "common.h"

void fb_put_pixel(int x, int y, uint32_t colour)
{
    graphics_mode_t *mode = graphics_get_mode();
    if (!mode || !mode->enabled)
    {
        return; // No graphics mode or invalid framebuffer
    }

    if (x < 0 || y < 0 || x >= (int)mode->width || y >= (int)mode->height)
    {
        return; // Out of bounds
    }

    // Choose target framebuffer (back buffer if double buffering enabled, front buffer otherwise)
    uint32_t target_buffer;
    if (mode->double_buffering_enabled && mode->render_to_back_buffer && mode->back_buffer)
    {
        target_buffer = mode->back_buffer;
    }
    else
    {
        target_buffer = mode->framebuffer;
    }

    // Additional safety check for framebuffer address
    if (target_buffer < 0x1000000)
    {           // Framebuffer should be above 16MB
        return; // Invalid framebuffer address
    }

    uint32_t *framebuffer = (uint32_t *)target_buffer;
    uint32_t offset = y * (mode->pitch / 4) + x;
    framebuffer[offset] = colour;
}

uint32_t fb_get_pixel(int x, int y)
{
    graphics_mode_t *mode = graphics_get_mode();
    if (!mode || !mode->framebuffer)
    {
        return 0; // No graphics mode or invalid framebuffer
    }

    if (x < 0 || y < 0 || x >= (int)mode->width || y >= (int)mode->height)
    {
        return 0; // Out of bounds
    }

    // Use the same render target logic as fb_put_pixel
    uint32_t target_buffer = mode->framebuffer;
    if (mode->double_buffering_enabled && mode->render_to_back_buffer && mode->back_buffer)
    {
        target_buffer = mode->back_buffer;
    }

    // Additional safety check for buffer address
    if (target_buffer < 0x1000000)
    {             // Buffer should be above 16MB
        return 0; // Invalid buffer address
    }

    uint32_t *framebuffer = (uint32_t *)target_buffer;
    uint32_t offset = y * (mode->pitch / 4) + x;
    return framebuffer[offset];
}

void fb_fill_rect(int x, int y, int width, int height, uint32_t colour)
{
    graphics_mode_t *mode = graphics_get_mode();
    if (!mode)
        return;

    // Clip rectangle to screen bounds
    if (x < 0)
    {
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        height += y;
        y = 0;
    }
    if (x + width > (int)mode->width)
        width = (int)mode->width - x;
    if (y + height > (int)mode->height)
        height = (int)mode->height - y;

    if (width <= 0 || height <= 0)
        return;

    // Use the same render target logic as fb_put_pixel
    uint32_t target_buffer = mode->framebuffer;
    if (mode->double_buffering_enabled && mode->render_to_back_buffer && mode->back_buffer)
    {
        target_buffer = mode->back_buffer;
    }

    uint32_t *framebuffer = (uint32_t *)target_buffer;
    uint32_t pitch_pixels = mode->pitch / 4;

    for (int row = 0; row < height; row++)
    {
        uint32_t *line = framebuffer + (y + row) * pitch_pixels + x;
        for (int col = 0; col < width; col++)
        {
            line[col] = colour;
        }
    }
}

void fb_copy_rect(int src_x, int src_y, int dst_x, int dst_y, int width, int height)
{
    graphics_mode_t *mode = graphics_get_mode();
    if (!mode)
        return;

    // Bounds checking
    if (src_x < 0 || src_y < 0 || dst_x < 0 || dst_y < 0)
        return;
    if (src_x + width > (int)mode->width || src_y + height > (int)mode->height)
        return;
    if (dst_x + width > (int)mode->width || dst_y + height > (int)mode->height)
        return;

    // Use the same render target logic as fb_put_pixel
    uint32_t target_buffer = mode->framebuffer;
    if (mode->double_buffering_enabled && mode->render_to_back_buffer && mode->back_buffer)
    {
        target_buffer = mode->back_buffer;
    }

    uint32_t *framebuffer = (uint32_t *)target_buffer;
    uint32_t pitch_pixels = mode->pitch / 4;

    // Handle overlapping regions by copying in the right direction
    if (src_y < dst_y || (src_y == dst_y && src_x < dst_x))
    {
        // Copy from bottom-right to top-left
        for (int row = height - 1; row >= 0; row--)
        {
            uint32_t *src_line = framebuffer + (src_y + row) * pitch_pixels + src_x;
            uint32_t *dst_line = framebuffer + (dst_y + row) * pitch_pixels + dst_x;
            for (int col = width - 1; col >= 0; col--)
            {
                dst_line[col] = src_line[col];
            }
        }
    }
    else
    {
        // Copy from top-left to bottom-right
        for (int row = 0; row < height; row++)
        {
            uint32_t *src_line = framebuffer + (src_y + row) * pitch_pixels + src_x;
            uint32_t *dst_line = framebuffer + (dst_y + row) * pitch_pixels + dst_x;
            for (int col = 0; col < width; col++)
            {
                dst_line[col] = src_line[col];
            }
        }
    }
}

void fb_clear(uint32_t colour)
{
    extern void serial_puts(const char *msg);

    graphics_mode_t *mode = graphics_get_mode();
    if (!mode || !mode->framebuffer)
    {
        return;
    }

    // Determine which buffer to clear based on double buffering settings
    uint32_t buffer_to_clear;
    if (mode->double_buffering_enabled && mode->render_to_back_buffer && mode->back_buffer)
    {
        buffer_to_clear = mode->back_buffer;
    }
    else
    {
        buffer_to_clear = mode->framebuffer;
    }

    // Additional safety check
    if (buffer_to_clear < 0x1000000)
    {
        serial_puts("SERIAL: fb_clear() - ERROR: invalid buffer address\n");
        return;
    }

    // Clear framebuffer without verbose testing
    volatile uint32_t *framebuffer = (volatile uint32_t *)buffer_to_clear;
    uint32_t total_pixels = mode->width * mode->height;

    // Clear in chunks with occasional checks for stability
    for (uint32_t i = 0; i < total_pixels; i++)
    {
        framebuffer[i] = colour;

        // Add occasional delays to prevent overwhelming the system
        if ((i & 0x3FF) == 0)  // Every 1024 pixels
        {
            for (volatile int delay = 0; delay < 1000; delay++)
                ;
        }
    }
}

// Double buffering implementation
bool fb_init_double_buffer()
{
    graphics_mode_t *mode = graphics_get_mode();
    if (!mode || !mode->enabled)
    {
        return false;
    }

    // Calculate buffer size
    uint32_t buffer_size = mode->pitch * mode->height;

    // Allocate back buffer
    mode->back_buffer = (uint32_t)malloc(buffer_size);
    if (!mode->back_buffer)
    {
        return false;
    }

    // Initialize back buffer to black
    uint32_t *back_buffer = (uint32_t *)mode->back_buffer;
    uint32_t total_pixels = buffer_size / 4;
    for (uint32_t i = 0; i < total_pixels; i++)
    {
        back_buffer[i] = COLOUR_BLACK;
    }

    mode->double_buffering_enabled = true;
    mode->render_to_back_buffer = true;

    return true;
}

void fb_swap_buffers()
{
    graphics_mode_t *mode = graphics_get_mode();
    if (!mode || !mode->enabled || !mode->double_buffering_enabled || !mode->back_buffer)
    {
        return;
    }

    // Copy back buffer to front buffer
    uint32_t buffer_size = mode->pitch * mode->height;
    uint32_t *front_buffer = (uint32_t *)mode->framebuffer;
    uint32_t *back_buffer = (uint32_t *)mode->back_buffer;

    // Use fast memory copy
    memcpy(front_buffer, back_buffer, buffer_size);
}

void fb_enable_double_buffering()
{
    graphics_mode_t *mode = graphics_get_mode();
    if (mode && mode->back_buffer)
    {
        mode->double_buffering_enabled = true;
        mode->render_to_back_buffer = true;
    }
}

void fb_disable_double_buffering()
{
    graphics_mode_t *mode = graphics_get_mode();
    if (mode)
    {
        mode->double_buffering_enabled = false;
        mode->render_to_back_buffer = false;
    }
}

void fb_set_render_target_back()
{
    graphics_mode_t *mode = graphics_get_mode();
    if (mode && mode->double_buffering_enabled)
    {
        mode->render_to_back_buffer = true;
    }
}

void fb_set_render_target_front()
{
    graphics_mode_t *mode = graphics_get_mode();
    if (mode)
    {
        mode->render_to_back_buffer = false;
    }
}