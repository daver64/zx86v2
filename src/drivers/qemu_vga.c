// qemu_vga.c -- QEMU Standard VGA driver
// Provides 1920x1080x32 graphics mode for ZX86v2

#include "graphics.h"
#include "pci.h"
#include "common.h"
#include "paging.h"
#include "multiboot.h"

// Serial debugging functions
void serial_install();
void serial_puts(const char *msg);

static graphics_mode_t graphics_mode = {0};
static bool graphics_init_in_progress = false;

bool qemu_vga_detect() {
    printf("Searching for QEMU Standard VGA (1234:1111)...\n");
    
    // Look for QEMU Standard VGA device
    pci_device_t *vga_dev = pci_find_device(QEMU_VGA_VENDOR_ID, QEMU_VGA_DEVICE_ID);
    if (vga_dev) {
        printf("Found QEMU Standard VGA at %02x:%02x.%x\n", 
               vga_dev->bus, vga_dev->device, vga_dev->function);
        return true;
    } else {
        printf("QEMU Standard VGA not found\n");
    }
    
    // Fallback: look for any VGA-compatible device
    printf("Searching for any VGA-compatible device (class 030000)...\n");
    vga_dev = pci_find_class(PCI_CLASS_VGA);
    if (vga_dev) {
        printf("Found VGA-compatible device %04x:%04x at %02x:%02x.%x\n",
               vga_dev->vendor_id, vga_dev->device_id,
               vga_dev->bus, vga_dev->device, vga_dev->function);
        
        // Provide helpful information about VGA device types
        if (vga_dev->vendor_id == 0x1013) {
            printf("This is Cirrus Logic VGA - try 'qemu -vga cirrus'\n");
        } else if (vga_dev->vendor_id == 0x15ad) {
            printf("This is VMware SVGA - try 'qemu -vga vmware'\n");
        } else if (vga_dev->vendor_id == 0x1b36) {
            printf("This is QXL VGA - try 'qemu -vga qxl'\n");
        } else if (vga_dev->vendor_id == 0x1af4) {
            printf("This is VirtIO GPU - try 'qemu -vga virtio'\n");
        } else {
            printf("Unknown VGA device - for QEMU Standard VGA use 'qemu -vga std'\n");
        }
        
        return true;
    } else {
        printf("No VGA-compatible devices found\n");
    }
    
    return false;
}

bool qemu_vga_init(uint16_t width, uint16_t height) {
    printf("Looking for VGA device...\n");
    
    if (!qemu_vga_detect()) {
        printf("NET: ERROR - No VGA device found\n");
        return false;
    }
    
    // Find the VGA device
    pci_device_t *vga_dev = pci_find_device(QEMU_VGA_VENDOR_ID, QEMU_VGA_DEVICE_ID);
    if (!vga_dev) {
        printf("Trying fallback VGA detection...\n");
        vga_dev = pci_find_class(PCI_CLASS_VGA);
    }
    
    if (!vga_dev) {
        printf("VGA: ERROR - VGA device detection failed\n");
        return false;
    }
    
    printf("Found VGA device %04x:%04x\n", vga_dev->vendor_id, vga_dev->device_id);
    
    // Check if we found the QEMU Standard VGA device specifically
    bool is_qemu_std_vga = (vga_dev->vendor_id == QEMU_VGA_VENDOR_ID && 
                           vga_dev->device_id == QEMU_VGA_DEVICE_ID);
    printf("Is QEMU Standard VGA: %s\n", is_qemu_std_vga ? "yes" : "no");
    
    // Enable memory access for the device
    printf("Enabling VGA memory access...\n");
    pci_enable_memory(vga_dev);
    
    // Get framebuffer address from BAR0
    uint32_t framebuffer_raw = pci_get_bar(vga_dev, 0);
    uint32_t framebuffer_base = framebuffer_raw & 0xFFFFFFF0; // Clear low 4 bits for proper alignment
    
    printf("BAR0 raw: 0x%08x, framebuffer: 0x%08x\n", framebuffer_raw, framebuffer_base);
    
    // Check other BARs too for debugging
    for (int bar = 1; bar < 6; bar++) {
        uint32_t bar_val = pci_get_bar(vga_dev, bar);
        if (bar_val != 0) {
            printf("BAR%d: 0x%08x\n", bar, bar_val);
        }
    }
    
    if (framebuffer_base == 0) {
        printf("VGA: ERROR - Failed to get VGA framebuffer address from BAR0\n");
        return false;
    }
    
    // Validate framebuffer address - make range more permissive
    if (framebuffer_base < 0x100000 || framebuffer_base > 0xFE000000) {
        printf("VGA: ERROR - Invalid framebuffer address: 0x%08x (outside 0x100000-0xFE000000)\n", framebuffer_base);
        return false;
    }
    
    printf("VGA framebuffer at 0x%08x\n", framebuffer_base);
    
    // Program QEMU VGA to switch to graphics mode (only for QEMU Standard VGA)
    if (is_qemu_std_vga) {
        printf("Programming QEMU VGA registers for %dx%dx32...\n", width, height);
        qemu_vga_set_mode(width, height, 32);
        
        // Give the hardware much more time to switch modes
        printf("Waiting for mode switch to complete...\n");
        for (volatile int i = 0; i < 5000000; i++);  // Increased from 1M to 5M
        
        // Add additional delay to ensure framebuffer is ready
        for (volatile int i = 0; i < 5000000; i++);
    } else {
        printf("Non-QEMU VGA device detected - skipping mode programming\n");
        printf("Device may already be in compatible mode or need different setup\n");
    }
    
    // Set up graphics mode structure FIRST
    graphics_mode.framebuffer = framebuffer_base;
    graphics_mode.width = width;
    graphics_mode.height = height;
    graphics_mode.bpp = 32;
    graphics_mode.pitch = graphics_mode.width * (graphics_mode.bpp / 8);
    graphics_mode.enabled = true;
    // Initialize double buffering fields
    graphics_mode.back_buffer = 0;
    graphics_mode.double_buffering_enabled = false;
    graphics_mode.render_to_back_buffer = false;
    
    printf("Graphics mode: %dx%dx%d (pitch=%d)\n", 
           graphics_mode.width, graphics_mode.height, 
           graphics_mode.bpp, graphics_mode.pitch);
    
    // Map the framebuffer in the page tables with maximum possible size
    // This ensures we can switch to any resolution without remapping
    uint32_t max_framebuffer_size = 1920 * 1080 * 4; // Max resolution at 32 bpp
    uint32_t current_framebuffer_size = graphics_mode.pitch * graphics_mode.height;
    printf("Mapping framebuffer in page tables (current: 0x%08x, max: 0x%08x)...\n", 
           current_framebuffer_size, max_framebuffer_size);
    map_framebuffer(framebuffer_base, max_framebuffer_size);
    
    // Initialize double buffering
    if (fb_init_double_buffer()) {
        printf("VGA: Double buffering initialized\n");
    } else {
        printf("VGA: WARNING - Double buffering initialization failed\n");
    }
    
    // Initialize the graphics terminal
    terminal_init();
    
    printf("VGA: Graphics initialization complete\n");
    return true;
}

bool graphics_init(uint16_t width, uint16_t height) {
    // Initialize serial first to ensure we can debug
    serial_install();
    
    // Prevent recursive calls
    if (graphics_init_in_progress) {
        serial_puts("SERIAL: ERROR - Graphics init recursion detected - blocking\n");
        return false;
    }
    
    graphics_init_in_progress = true;
    
    // Try QEMU VGA first
    bool result = false;
    if (qemu_vga_init(width, height)) {
        result = true;
    } else {
        printf("VGA: Graphics init failed, trying fallback\n");
        
        // Try fallback - but prevent infinite loop
        if (graphics_init_fallback(width, height)) {
            result = true;
        } else {
            printf("VGA: ERROR - Graphics initialization failed completely\n");
            result = false;
        }
    }
    
    graphics_init_in_progress = false;
    return result;
}

bool graphics_init_multiboot(struct multiboot *mboot_ptr) {
    printf("Initializing graphics from multiboot information...\n");
    
    // Check if VBE information is available
    if (!(mboot_ptr->flags & MULTIBOOT_FLAG_VBE)) {
        printf("No VBE information from bootloader\n");
        return graphics_init_fallback(1920, 1080);
    }
    
    printf("VBE information available from bootloader!\n");
    printf("VBE mode: 0x%x\n", mboot_ptr->vbe_mode);
    printf("VBE control info: 0x%x\n", mboot_ptr->vbe_control_info);
    printf("VBE mode info: 0x%x\n", mboot_ptr->vbe_mode_info);
    
    // The VBE mode info structure contains framebuffer details
    // For now, let's try to detect the framebuffer from the VBE mode
    // In a real implementation, we'd parse the VBE mode info structure
    
    // Try to get framebuffer from PCI as fallback, but with multiboot mode set
    printf("Reading framebuffer address from PCI...\n");
    
    // Try QEMU VGA first
    if (qemu_vga_init(1920, 1080)) {
        printf("Graphics subsystem initialized successfully with QEMU VGA\n");
        return true;
    }
    
    return graphics_init_fallback(1920, 1080);
}

bool graphics_init_fallback(uint16_t width, uint16_t height) {
    printf("Using fallback graphics initialization...\n");
    
    // If that failed, try to detect any framebuffer we can use
    printf("QEMU VGA failed, trying alternative detection...\n");
    
        // Check if we have any VGA device at all
        pci_device_t *any_vga = pci_find_class(PCI_CLASS_VGA);
        if (any_vga) {
            printf("Found alternative VGA device %04x:%04x, attempting to use...\n", 
                   any_vga->vendor_id, any_vga->device_id);
            
            // Enable memory access first
            pci_enable_memory(any_vga);
            
            // Try to use it with standard VGA framebuffer address first
            uint32_t framebuffer_addr = 0xA0000; // Standard VGA framebuffer
            
            // Try reading the BAR to see if we get a better address
            uint32_t bar0 = pci_get_bar(any_vga, 0);
            if (bar0 != 0 && (bar0 & 0xFFFFFFF0) > 0x100000) {
                // Clear the low bits that indicate memory type and prefetchable flags
                uint32_t proposed_addr = bar0 & 0xFFFFFFF0;
                
                // Only use the BAR address if it seems reasonable
                if (proposed_addr < 0xE0000000) { // Don't use addresses too high in memory
                    framebuffer_addr = proposed_addr;
                    printf("Using BAR0 framebuffer address: 0x%08x (raw BAR: 0x%08x)\n", framebuffer_addr, bar0);
                } else {
                    printf("BAR0 address 0x%08x too high, using standard VGA address 0x%08x\n", proposed_addr, framebuffer_addr);
                }
            } else {
                printf("Using standard VGA framebuffer address: 0x%08x\n", framebuffer_addr);
            }
            
            // Set up basic graphics mode
            graphics_mode.framebuffer = framebuffer_addr;
            graphics_mode.width = width;
            graphics_mode.height = height;
            graphics_mode.bpp = 32;
            graphics_mode.pitch = graphics_mode.width * 4;
            graphics_mode.enabled = true;
            // Initialize double buffering fields for fallback
            graphics_mode.back_buffer = 0;
            graphics_mode.double_buffering_enabled = false;
            graphics_mode.render_to_back_buffer = false;
            
            // Map the framebuffer in the page tables with maximum possible size
            uint32_t max_framebuffer_size = 1920 * 1080 * 4; // Max resolution at 32 bpp
            uint32_t current_framebuffer_size = graphics_mode.pitch * graphics_mode.height;
            printf("Mapping alternative framebuffer in page tables (current: 0x%08x, max: 0x%08x)...\n", 
                   current_framebuffer_size, max_framebuffer_size);
            map_framebuffer(framebuffer_addr, max_framebuffer_size);
            
            // Initialize double buffering for fallback path
            if (fb_init_double_buffer()) {
                printf("Fallback double buffering initialized\n");
            }
            
            printf("Alternative graphics setup complete\n");
            return true;
        }    printf("Graphics initialization failed - no VGA devices found\n");
    return false;
}

bool graphics_enabled() {
    return graphics_mode.enabled;
}

graphics_mode_t* graphics_get_mode() {
    if (!graphics_mode.enabled) return NULL;
    return &graphics_mode;
}

void qemu_vga_set_mode(uint16_t width, uint16_t height, uint8_t bpp) {
    printf("Setting QEMU VGA mode: %dx%dx%d\n", width, height, bpp);
    
    // VBE (VESA BIOS Extensions) registers for QEMU
    #define VBE_DISPI_IOPORT_INDEX    0x01CE
    #define VBE_DISPI_IOPORT_DATA     0x01CF
    
    #define VBE_DISPI_INDEX_ID        0x0
    #define VBE_DISPI_INDEX_XRES      0x1
    #define VBE_DISPI_INDEX_YRES      0x2
    #define VBE_DISPI_INDEX_BPP       0x3
    #define VBE_DISPI_INDEX_ENABLE    0x4
    
    #define VBE_DISPI_DISABLED        0x00
    #define VBE_DISPI_ENABLED         0x01
    #define VBE_DISPI_LFB_ENABLED     0x40
    
    // Check if VBE is available by reading the ID register
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
    uint16_t vbe_id = inw(VBE_DISPI_IOPORT_DATA);
    printf("VBE ID: 0x%04x\n", vbe_id);
    
    if (vbe_id < 0xB0C0) {
        printf("VGA: ERROR - VBE not available or version too old (need >= 0xB0C0)\n");
        return;
    }
    
    printf("VBE %d.%d detected, proceeding with mode set\n", 
           (vbe_id >> 8) & 0xFF, vbe_id & 0xFF);
    
    // Disable VBE first
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
    outw(VBE_DISPI_IOPORT_DATA, VBE_DISPI_DISABLED);
    
    // Set X resolution
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_XRES);
    outw(VBE_DISPI_IOPORT_DATA, width);
    
    // Set Y resolution
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_YRES);
    outw(VBE_DISPI_IOPORT_DATA, height);
    
    // Set bits per pixel
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_BPP);
    outw(VBE_DISPI_IOPORT_DATA, bpp);
    
    // Enable VBE with linear framebuffer
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
    outw(VBE_DISPI_IOPORT_DATA, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    
    // Verify enable status
    outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
    uint16_t enable_status = inw(VBE_DISPI_IOPORT_DATA);
    printf("VBE enable status: 0x%04x\n", enable_status);
    
    // Give hardware time to switch
    for (volatile int i = 0; i < 1000000; i++);
    
    printf("QEMU VGA mode setting complete\n");
}

bool graphics_set_mode(uint16_t width, uint16_t height, uint8_t bpp) {
    if (!graphics_enabled()) {
        printf("VGA: ERROR - Graphics not initialized\n");
        return false;
    }
    
    // Validate parameters
    if (width < 320 || width > 1920 || height < 240 || height > 1080) {
        printf("VGA: ERROR - Invalid resolution %dx%d (supported: 320x240 to 1920x1080)\n", width, height);
        return false;
    }
    
    if (bpp != 16 && bpp != 24 && bpp != 32) {
        printf("VGA: ERROR - Invalid BPP %d (supported: 16, 24, 32)\n", bpp);
        return false;
    }
    
    printf("VGA: Changing mode to %dx%dx%d...\n", width, height, bpp);
    
    // Save current mode info
    graphics_mode_t *current_mode = graphics_get_mode();
    if (!current_mode) {
        printf("VGA: ERROR - Could not get current graphics mode\n");
        return false;
    }
    
    // Disable double buffering during mode change for safety
    bool was_double_buffered = current_mode->double_buffering_enabled;
    if (was_double_buffered) {
        fb_disable_double_buffering();
    }
    
    // Set new VGA mode (hardware level)
    qemu_vga_set_mode(width, height, bpp);
    
    // Update mode structure
    current_mode->width = width;
    current_mode->height = height;
    current_mode->bpp = bpp;
    current_mode->pitch = width * (bpp / 8);
    
    // Calculate new framebuffer size for validation
    uint32_t new_framebuffer_size = current_mode->pitch * current_mode->height;
    uint32_t max_safe_size = 1920 * 1080 * 4; // Max reasonable size
    
    if (new_framebuffer_size > max_safe_size) {
        printf("VGA: ERROR - New framebuffer size too large: 0x%08x\n", new_framebuffer_size);
        return false;
    }
    
    printf("VGA: New framebuffer size: 0x%08x bytes (already mapped)\n", new_framebuffer_size);
    
    // Clear the screen with new dimensions
    fb_clear(COLOUR_BLACK);
    
    // Re-enable double buffering if it was enabled before
    if (was_double_buffered) {
        if (!fb_init_double_buffer()) {
            printf("VGA: WARNING - Could not reinitialize double buffering\n");
        }
    }
    
    printf("VGA: Mode change complete - %dx%dx%d\n", width, height, bpp);
    return true;
}