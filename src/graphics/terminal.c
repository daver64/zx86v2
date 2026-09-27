// terminal.c -- Graphics-based terminal for ZX86v2
// Implements a text terminal that renders to the framebuffer

#include "graphics.h"
#include "common.h"

// Direct font drawing function (CPU0 only)
static void terminal_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color) {
    extern void font_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color);
    font_draw_char(x, y, c, fg_color, bg_color);
}

// Direct framebuffer wrappers (CPU0 only)
static void terminal_fb_clear(uint32_t color) {
    extern void fb_clear(uint32_t color);
    fb_clear(color);
}

static void terminal_fb_fill_rect(int x, int y, int width, int height, uint32_t color) {
    extern void fb_fill_rect(int x, int y, int width, int height, uint32_t color);
    fb_fill_rect(x, y, width, height, color);
}

static void terminal_fb_copy_rect(int src_x, int src_y, int dst_x, int dst_y, int width, int height) {
    extern void fb_copy_rect(int src_x, int src_y, int dst_x, int dst_y, int width, int height);
    fb_copy_rect(src_x, src_y, dst_x, dst_y, width, height);
}

static void terminal_fb_set_render_target_back(void) {
    extern void fb_set_render_target_back(void);
    fb_set_render_target_back();
}

static void terminal_fb_set_render_target_front(void) {
    extern void fb_set_render_target_front(void);
    fb_set_render_target_front();
}

static void terminal_fb_swap_buffers(void) {
    extern void fb_swap_buffers(void);
    fb_swap_buffers();
}

// Dynamic terminal dimensions based on actual screen resolution
int terminal_get_cols()
{
    if (!graphics_enabled()) {
        return 80; // Default text mode width
    }
    
    extern graphics_mode_t *graphics_get_mode();
    graphics_mode_t *mode = graphics_get_mode();
    if (!mode) {
        return 80; // Fallback
    }
    
    return mode->width / FONT_WIDTH;
}

int terminal_get_rows()
{
    if (!graphics_enabled()) {
        return 25; // Default text mode height
    }
    
    extern graphics_mode_t *graphics_get_mode();
    graphics_mode_t *mode = graphics_get_mode();
    if (!mode) {
        return 25; // Fallback
    }
    
    return mode->height / FONT_HEIGHT;
}

// Terminal state
typedef struct
{
    int cursor_x;                  // Current cursor column (0 to terminal_get_cols()-1)
    int cursor_y;                  // Current cursor row (0 to terminal_get_rows()-1)
    int last_cursor_x;             // Previous cursor column for erasing
    int last_cursor_y;             // Previous cursor row for erasing
    uint32_t fg_color;             // Foreground color
    uint32_t bg_color;             // Background color
    bool initialized;              // Whether terminal is initialized
    bool cursor_visible;           // Whether cursor should be drawn
    bool cursor_blink_state;       // Current blink state (on/off)
    uint32_t cursor_blink_counter; // Counter for blink timing
    bool text_output_mode;         // Whether we're currently outputting text (suppress cursor)
} terminal_state_t;

static terminal_state_t terminal = {0};

void terminal_init()
{
    // Debug: Add serial debugging to trace exactly where recursion happens
    extern void serial_puts(const char *msg);

    if (!graphics_enabled())
    {
        // Don't use printf here as it might cause recursion
        return;
    }

    // Try to preserve current cursor position from VGA text mode
    extern int get_cursor_x();
    extern int get_cursor_y();
    int initial_x = get_cursor_x();
    int initial_y = get_cursor_y();
    
    // Don't use printf during initialization to avoid recursion
    terminal.cursor_x = initial_x;
    terminal.cursor_y = initial_y;
    terminal.last_cursor_x = initial_x;
    terminal.last_cursor_y = initial_y;
    terminal.fg_color = COLOUR_WHITE;
    terminal.bg_color = COLOUR_BLACK;
    terminal.cursor_visible = true;
    terminal.cursor_blink_state = true;
    terminal.cursor_blink_counter = 0;
    terminal.initialized = true;

    // For now, don't clear the screen during init to preserve boot messages
    // terminal_clear();
    
    // Instead, just make sure we're set up for rendering
    terminal_fb_set_render_target_back();
    terminal_fb_swap_buffers();

    // Ensure cursor is visible after initialization
    terminal_show_cursor();
}

void terminal_clear()
{
    if (!terminal.initialized)
    {
        return;
    }

    // Use CPU2-aware wrapper
    terminal_fb_set_render_target_back();
    terminal_fb_clear(terminal.bg_color);

    // Reset cursor position
    terminal.cursor_x = 0;
    terminal.cursor_y = 0;

    // Do NOT swap buffers here - let terminal_flush handle it
}

void terminal_scroll()
{
    if (!terminal.initialized)
        return;

    // Use CPU0 graphics operations for all terminal operations
    // This ensures synchronous operation and avoids AMP timing issues
    terminal_fb_set_render_target_back();

    // Always use direct graphics operations on CPU0
    // Copy screen content up by one line
    terminal_fb_copy_rect(0, FONT_HEIGHT, 0, 0,
                 terminal_get_cols() * FONT_WIDTH,
                 (terminal_get_rows() - 1) * FONT_HEIGHT);

    // Clear the bottom line
    terminal_fb_fill_rect(0, (terminal_get_rows() - 1) * FONT_HEIGHT,
                 terminal_get_cols() * FONT_WIDTH, FONT_HEIGHT,
                 terminal.bg_color);

    // Do NOT swap buffers here - let terminal_flush handle it
}

void terminal_newline()
{
    terminal.cursor_x = 0;
    terminal.cursor_y++;

    if (terminal.cursor_y >= terminal_get_rows())
    {
        terminal_scroll();
        terminal.cursor_y = terminal_get_rows() - 1;
    }
}

void terminal_putc(char c)
{
    // Use original graphics-based terminal system
    if (!terminal.initialized)
    {
        // Fallback to VGA text mode if graphics not available
        return;
    }

    // Make sure we're rendering to back buffer
    terminal_fb_set_render_target_back();

    switch (c)
    {
    case '\n':
        terminal_newline();
        break;

    case '\r':
        terminal.cursor_x = 0;
        break;

    case '\t':
        // Tab to next 8-character boundary
        terminal.cursor_x = (terminal.cursor_x + 8) & ~7;
        if (terminal.cursor_x >= terminal_get_cols())
        {
            terminal_newline();
        }
        break;

    case '\b':
        // Backspace
        if (terminal.cursor_x > 0)
        {
            terminal.cursor_x--;
            // Erase the character at cursor position (no need to clear cursor area)
            terminal_draw_char(terminal.cursor_x * FONT_WIDTH,
                           terminal.cursor_y * FONT_HEIGHT,
                           ' ', terminal.fg_color, terminal.bg_color);
        }
        break;

    default:
        // Printable character
        if (c >= 32 && c <= 126)
        {
            // Draw the character (no need to clear cursor since it's only on front buffer)
            terminal_draw_char(terminal.cursor_x * FONT_WIDTH,
                           terminal.cursor_y * FONT_HEIGHT,
                           c, terminal.fg_color, terminal.bg_color);

            terminal.cursor_x++;
            
            if (terminal.cursor_x >= terminal_get_cols())
            {
                terminal_newline();
            }
        }
        break;
    }
}

void terminal_puts(const char *str)
{
    if (!str)
        return;

    // Make sure we're rendering to back buffer
    terminal_fb_set_render_target_back();

    while (*str)
    {
        terminal_putc(*str);
        str++;
    }

    // Use terminal_flush to properly manage cursor before swapping buffers
    terminal_flush();
}

// Terminal output function that immediately displays (for interactive use)
void terminal_putc_immediate(char c)
{
    terminal_putc(c);
    // Only swap buffers for scrolling-triggering events or when explicitly needed
    // This maintains performance while ensuring visibility for interactive use
}

// Force a buffer swap to display any pending characters
void terminal_flush()
{
    // Use CPU2-aware wrapper
    terminal_fb_swap_buffers();
    
    // Handle cursor drawing on the front buffer
    if (terminal.cursor_visible) {
        terminal_fb_set_render_target_front();
        
        // Clear old cursor position if it has moved
        if (terminal.cursor_x != terminal.last_cursor_x || terminal.cursor_y != terminal.last_cursor_y) {
            int old_x = terminal.last_cursor_x * FONT_WIDTH;
            int old_y = terminal.last_cursor_y * FONT_HEIGHT;
            // Clear old cursor from front buffer
            terminal_fb_fill_rect(old_x, old_y + FONT_HEIGHT - 2, FONT_WIDTH, 2, terminal.bg_color);
            
            // Update tracking
            terminal.last_cursor_x = terminal.cursor_x;
            terminal.last_cursor_y = terminal.cursor_y;
        }
        
        // Draw new cursor position
        int x = terminal.cursor_x * FONT_WIDTH;
        int y = terminal.cursor_y * FONT_HEIGHT;
        terminal_fb_fill_rect(x, y + FONT_HEIGHT - 2, FONT_WIDTH, 2, terminal.fg_color);
        
        terminal_fb_set_render_target_back(); // Restore back buffer for next rendering
    }
}

void terminal_set_cursor(int x, int y)
{
    if (x >= 0 && x < terminal_get_cols() && y >= 0 && y < terminal_get_rows())
    {
        terminal.cursor_x = x;
        terminal.cursor_y = y;
    }
}

void terminal_get_cursor(int *x, int *y)
{
    if (x)
        *x = terminal.cursor_x;
    if (y)
        *y = terminal.cursor_y;
}

void terminal_set_colors(uint32_t fg, uint32_t bg)
{
    terminal.fg_color = fg;
    terminal.bg_color = bg;
}

void terminal_set_fg_color(uint32_t color)
{
    if (terminal.initialized)
    {
        terminal.fg_color = color;
    }
}

void terminal_set_bg_color(uint32_t color)
{
    if (terminal.initialized)
    {
        terminal.bg_color = color;
    }
}

// Cursor rendering functions
void terminal_draw_cursor()
{
    if (!terminal.initialized || !terminal.cursor_visible || !terminal.cursor_blink_state)
    {
        return;
    }

    // Make sure we're rendering to back buffer
    terminal_fb_set_render_target_back();

    int x = terminal.cursor_x * FONT_WIDTH;
    int y = terminal.cursor_y * FONT_HEIGHT;

    // Draw cursor as an underscore at the bottom of the character cell
    terminal_fb_fill_rect(x, y + FONT_HEIGHT - 2, FONT_WIDTH, 2, terminal.fg_color);
}

// Force draw cursor regardless of blink state (for editors)
void terminal_draw_cursor_solid()
{
    if (!terminal.initialized || !terminal.cursor_visible)
    {
        return;
    }

    // Draw cursor directly to front buffer
    terminal_fb_set_render_target_front();

    int x = terminal.cursor_x * FONT_WIDTH;
    int y = terminal.cursor_y * FONT_HEIGHT;

    // Draw cursor as an underline at the bottom of the character cell (2 pixels high)
    terminal_fb_fill_rect(x, y + FONT_HEIGHT - 2, FONT_WIDTH, 2, terminal.fg_color);
    
    terminal_fb_set_render_target_back(); // Restore back buffer
}

void terminal_erase_cursor()
{
    if (!terminal.initialized)
    {
        return;
    }

    // Erase cursor from front buffer
    terminal_fb_set_render_target_front();

    int x = terminal.cursor_x * FONT_WIDTH;
    int y = terminal.cursor_y * FONT_HEIGHT;

    // Erase underline cursor (2 pixels high at bottom of character cell)
    terminal_fb_fill_rect(x, y + FONT_HEIGHT - 2, FONT_WIDTH, 2, terminal.bg_color);
    
    terminal_fb_set_render_target_back(); // Restore back buffer
}

void terminal_update_cursor()
{
    // Update blink counter every call
    terminal.cursor_blink_counter++;

    // Blink every ~30 calls (adjust for desired speed)
    if (terminal.cursor_blink_counter >= 30)
    {
        terminal.cursor_blink_counter = 0;
        terminal.cursor_blink_state = !terminal.cursor_blink_state;
    }

    // Always redraw to handle blink state changes
    terminal_erase_cursor();
    terminal_draw_cursor();
}

void terminal_show_cursor()
{
    terminal.cursor_visible = true;
    terminal_draw_cursor_solid();  // Use solid cursor for terminal
}

void terminal_hide_cursor()
{
    terminal.cursor_visible = false;
    terminal_erase_cursor();
}

void terminal_set_text_output_mode(bool mode)
{
    terminal.text_output_mode = mode;
}

// VGA compatibility functions - forward to terminal system
void set_foreground_colour(unsigned char colour) {
    terminal.fg_color = vga_colour_to_rgb(colour);
}

void set_background_colour(unsigned char colour) {
    terminal.bg_color = vga_colour_to_rgb(colour);
}

uint8_t get_foreground_colour(void) {
    // Convert RGB back to VGA color (approximate)
    // This is a simplified conversion - just return white for now
    return VGA_WHITE;
}

uint8_t get_background_colour(void) {
    // Convert RGB back to VGA color (approximate)
    return VGA_BLACK;
}

void cls(void) {
    terminal_clear();
    terminal_flush(); // Force buffer swap to display the cleared screen
}

void gotoxy(int x, int y) {
    terminal_set_cursor(x, y);
}

int get_cursor_x(void) {
    return terminal.cursor_x;
}

int get_cursor_y(void) {
    return terminal.cursor_y;
}

// Character output function used by printf system
int os_putc(int c) {
    terminal_putc((char)c);
    return c;
}

// Additional compatibility functions
int putchar(int c) {
    return os_putc(c);
}

int puts(const char* str) {
    if (!str) return -1;
    
    // Direct terminal output (CPU0 only)
    
    // Use direct terminal output
    
    // Use original character-by-character output
    while (*str) {
        terminal_putc(*str);
        str++;
    }
    terminal_putc('\n');
    return 1;
}

void os_puts(const char* str) {
    puts(str);
}

int get_num_rows(void) {
    return terminal_get_rows();
}

int get_num_columns(void) {
    return terminal_get_cols();
}

unsigned short* video_mem_ptr(void) {
    // Return null since we don't use direct video memory in graphics mode
    return NULL;
}

void disable_cursor(void) {
    terminal_hide_cursor();
}

void enable_cursor(void) {
    terminal_show_cursor();
}

void set_text_colour(unsigned char forecolour, unsigned char backcolour) {
    set_foreground_colour(forecolour);
    set_background_colour(backcolour);
}

// Convert VGA color to RGB
uint32_t vga_colour_to_rgb(unsigned char vga_colour) {
    static const uint32_t vga_to_rgb[] = {
        0xFF000000, // VGA_BLACK
        0xFF0000AA, // VGA_BLUE  
        0xFF00AA00, // VGA_GREEN
        0xFF00AAAA, // VGA_CYAN
        0xFFAA0000, // VGA_RED
        0xFFAA00AA, // VGA_MAGENTA
        0xFFAA5500, // VGA_BROWN
        0xFFAAAAAA, // VGA_LIGHTGREY
        0xFF555555, // VGA_DARKGREY
        0xFF5555FF, // VGA_LIGHTBLUE
        0xFF55FF55, // VGA_LIGHTGREEN
        0xFF55FFFF, // VGA_LIGHTCYAN
        0xFFFF5555, // VGA_LIGHTRED
        0xFFFF55FF, // VGA_LIGHTMAGENTA
        0xFFFFFF55, // VGA_YELLOW
        0xFFFFFFFF  // VGA_WHITE
    };
    
    if (vga_colour < 16) {
        return vga_to_rgb[vga_colour];
    }
    return 0xFFFFFFFF; // Default to white
}