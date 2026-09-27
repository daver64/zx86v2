// main.c -- Defines the C-code kernel entry point, calls initialisation routines.
//           Made for JamesM's tutorials <www.jamesmolloy.co.uk>

#include "graphics.h"
#include "descriptor_tables.h"
#include "timer.h"
#include "paging.h"
#include "multiboot.h"
#include "task.h"
#include "syscall.h"
#include "pci.h"
#include "ramdisk.h"
#include "fat32_format.h"
#include "blockdev.h"
#include "vfs.h"
#include "graphics.h"
#include "amp.h"
#include "sound.h"
#include "amp.h"
#include "vfs.h"

struct multiboot;
extern uint32_t placement_address;
uint32_t initial_esp = 0x8000;
uint32_t mem_lower = 0;
uint32_t mem_upper = 0;
void shell_main();
void initialise_ata(); 

extern uint32_t kstart;
extern uint32_t code;
extern uint32_t bss;
extern uint32_t data;
void boot_kmesg()
{
    cls();
    uint8_t ofgc = get_foreground_colour();
    set_foreground_colour(VGA_LIGHTRED);
    printf("\n _______  __      ___   __   \n");
    printf("|__  /\\ \\/ /     ( _ ) / /_  \n");
    printf("  / /  \\  /_____ / _ \\| \'_ \\ \n");
    printf(" / /_  /  \\_____| (_) | (_) |\n");
    printf("/____|/_/\\_\\     \\___/ \\___/ \n");
    set_foreground_colour(VGA_LIGHTBLUE);
    printf("\nKernel Version 2\n");
    set_foreground_colour(VGA_GREEN);
    printf("[Volatile Storage]\n");
    set_foreground_colour(ofgc);
    printf("Physical Memory    : Lower %u Kb , Upper %u Kb\n", mem_lower, mem_upper);
    printf("Initial Stack      : 0x%08X\n", initial_esp);
    uint32_t kheap = get_kernel_heap();
    uint32_t kheap_end = get_kernel_heap_end();
    uint32_t kheap_size = kheap_end - kheap;
    printf("Kernel Heap        : 0x%08X - 0x%08X (%4u MB )\n", kheap, kheap_end, kheap_size / (1024 * 1024));
    printf("LibC Heap          : 0x%08X - 0x%08X (%4u MB )\n", get_libc_heap_start(), get_libc_heap_end(), get_libc_heap_size() / (1024 * 1024));
    printf("Virtual Memory     : %u 4096 Byte Pages  (%4u MB )\n", get_page_frame_count(), (get_page_frame_count() * 4096) / (1024 * 1024));
    
    // AMP and Graphics Memory Information
    set_foreground_colour(VGA_LIGHTCYAN);
    printf("[Multi-Processing]\n");
    set_foreground_colour(ofgc);
    printf("AMP Shared Memory  : 0x%08X - 0x%08X (%4u MB )\n", SHARED_MEMORY_BASE, SHARED_MEMORY_BASE + SHARED_MEMORY_SIZE, SHARED_MEMORY_SIZE / (1024 * 1024));
    
    // AMP Status Information
    if (amp_is_initialized()) {
        uint8_t cpu_count = amp_get_cpu_count();
        uint8_t online_cpus = 0;
        for (int i = 1; i < MAX_CPUS; i++) {  // Skip CPU0 as it's not in AMP workspace
            if (amp_is_cpu_online(i)) {
                online_cpus++;
            }
        }
        printf("AMP System Status  : %u CPUs detected, %u secondary CPUs online\n", cpu_count, online_cpus);
        printf("CPU0 Role          : Graphics, syscalls, kernel (excluded from AMP)\n");
        if (online_cpus > 0) {
            printf("Secondary CPUs     : CPU1-%u available for compute tasks\n", online_cpus);
            printf("Power Management   : HLT/IPI power saving enabled\n");
        }
    } else {
        printf("AMP System Status  : Not initialized\n");
    }
    
    // Get graphics mode information
    extern graphics_mode_t *graphics_get_mode();
    graphics_mode_t *gfx_mode = graphics_get_mode();
    if (gfx_mode && gfx_mode->enabled) {
        uint32_t fb_size = gfx_mode->width * gfx_mode->height * 4; // 32-bit pixels
        printf("Graphics Memory    : 0x%08X - 0x%08X (%4u MB ) %ux%u\n", 
               (uint32_t)gfx_mode->framebuffer, 
               (uint32_t)gfx_mode->framebuffer + fb_size,
               fb_size / (1024 * 1024),
               gfx_mode->width, gfx_mode->height);
    } else {
        printf("Graphics Memory    : Not initialized\n");
    }
}




int kmain(struct multiboot *mboot_ptr, uint32_t initial_stack)
{
    initial_esp = 0x8000;
    initialise_tables();
    initialise_syscalls();
    initialise_timer(100);
    initialise_paging();
    initialise_tasking();
    initialise_keyboard();
    initialise_ata(); 
    initialise_pci();
    
    // Initialize network subsystem
    extern bool rtl8139_init(void);
    if (rtl8139_init()) {
        // Initialize network protocols
        extern void ip_init(void);
        extern void icmp_init(void);
        extern void arp_init(void);
        extern void tcp_init(void);
        ip_init();
        icmp_init();
        arp_init();
        tcp_init();
    }

    // Initialize AMP (Asymmetric Multiprocessing) after basic systems are ready
    if (amp_init()) {
        // Initialize graphics first, but keep terminal operations on CPU0 for now
        bool graphics_ok = false;
        graphics_ok = graphics_init(800, 600);
        extern void serial_puts(const char *msg);
        if (!graphics_ok) {
            serial_puts("SERIAL: MAIN - ERROR: Graphics initialization failed, using text mode\n");
        }
        
        // DO NOT initialize CPU2 terminal system - keep terminal operations on CPU0
        // This avoids synchronization issues with scrolling and buffer management
        // amp_init_terminal_system();
        
        // Initialize shared memory BEFORE starting secondary CPUs
        if (amp_init_shared_memory() != 0) {
            serial_puts("SERIAL: ERROR: AMP shared memory initialization failed\n");
        } else {
            serial_puts("SERIAL: AMP shared memory initialized successfully\n");
        }
        
        // Now start secondary CPUs - they will be available for other tasks
        if (!amp_start_secondary_cpus()) {
            printf("ERROR: Failed to start secondary CPUs\n");
        }
        
        serial_puts("SERIAL: AMP - Graphics operations will run on CPU0, secondary CPUs available for other tasks\n");
        
        // Test distributed bitmap rendering on high cores (CPU6-7)
        printf("Waiting for secondary CPUs to initialize...\n");
        for (int i = 0; i < 1000000; i++) { pause_cpu(); } // Brief delay
        // test_distributed_bitmap_rendering(); // Disabled for now
        
    } else {
        printf("ERROR: AMP initialization failed - continuing with single CPU\n");
    }

    // Graphics initialization was moved above

    // Initialize Sound Blaster 16
    if (sb16_detect()) {
        if (!sb16_init()) {
            serial_puts("SERIAL: MAIN - ERROR: Sound Blaster 16 initialization failed\n");
        }
    }

    mem_lower = mboot_ptr->mem_lower;
    mem_upper = mboot_ptr->mem_upper;
    putenv("PATH=.;\\bin;\\user\\bin");
    
    // Initialize VFS (Virtual File System)
    if (vfs_init() != 0) {
        printf("ERROR: VFS initialization failed\n");
    }
    
    // Set disk I/O to synchronous mode globally for reliable file operations
    extern void set_disk_sync_mode(int sync_mode);
    set_disk_sync_mode(1);  // 1 = synchronous mode, 0 = asynchronous mode
    
    os_mount();
    
    // Setup ramdisk after main filesystem is mounted - temporarily disabled for debugging
    printf("Ramdisk setup moved to before shell startup\n");
    
    // Banner will be displayed after switching to user mode
    
    serial_puts("SERIAL: MAIN - About to switch to user mode and start shell\n");
    printf("MAIN: Starting shell...\n");
    
    // Setup ramdisk just before starting the shell (after all CPUs are settled)
    printf("Setting up ramdisk...\n");
    ramdisk_init();
    
    // Create an 8MB ramdisk. fat32_format() automatically picks FAT16 (proper
    // fixed-size root directory layout) for volumes this small, since FatFs
    // classifies FAT12/16/32 purely by cluster count (>65525 clusters => FAT32),
    // and reaching that threshold would require a much larger ramdisk than we
    // want to carve out of the kernel heap.
    int ramdisk_id = ramdisk_create(8 * 1024 * 1024);
    if (ramdisk_id >= 0) {
        // Find the ramdisk block device by name
        char ramdisk_name[16];
        sprintf(ramdisk_name, "ram%d", ramdisk_id);
        blockdev_t *ramdisk_bdev = blockdev_find(ramdisk_name);
        
        if (ramdisk_bdev) {
            printf("Found ramdisk: %s\n", ramdisk_bdev->name);
            
            // Format the ramdisk with FAT32
            if (fat32_format(ramdisk_bdev, "RAMDISK") == 0) {
                // Create the mount point directory first
                if (vfs_mkdir("/ramdisc", 0755) != 0) {
                    printf("Note: /ramdisc directory may already exist or creation failed\n");
                }
                
                // Mount the ramdisk at /ramdisc (use device name directly, not /dev/ path)
                if (vfs_mount(ramdisk_bdev->name, "/ramdisc", "fat32", 0, NULL) == 0) {
                    printf("Ramdisk successfully mounted at /ramdisc\n");
                } else {
                    printf("Failed to mount ramdisk at /ramdisc\n");
                }
            } else {
                printf("Failed to format ramdisk\n");
            }
        } else {
            printf("Failed to find ramdisk block device\n");
        }
    } else {
        printf("Failed to create ramdisk\n");
    }
    
    switch_to_user_mode();
    
    for (;;)
    {
        // Process any deferred executions before shell
        extern void process_deferred_executions(void);
        process_deferred_executions();
        
        serial_puts("SERIAL: MAIN - Calling shell_main\n");
        shell_main();
    }
    return 0;
}
