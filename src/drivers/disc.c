#include "common.h"
#include "graphics.h"
#include <ata.h>
#include "isr.h"

void enable_interrupts();
void disable_interrupts();
void outportsw(int, void *, int);
void inportsw(int, void *, int);

// Disk I/O error codes - standardized across all functions
#define DISK_SUCCESS 0
#define DISK_ERROR_INVALID -1
#define DISK_ERROR_TIMEOUT -2
#define DISK_ERROR_HARDWARE -3
#define DISK_ERROR_BOUNDS -4

// ATA interrupt-driven I/O state
typedef struct {
    volatile uint8_t operation_pending;    // 1 if operation in progress
    volatile uint8_t operation_complete;   // 1 when operation finished
    volatile uint8_t operation_error;      // Error code if operation failed
    volatile uint32_t sectors_remaining;   // Sectors left to transfer
    volatile void* current_buffer;         // Current buffer pointer
    volatile uint64_t current_lba;         // Current LBA being processed
    volatile uint8_t is_write_operation;   // 1 for write, 0 for read
} ata_async_state_t;

static ata_async_state_t primary_ata_state = {0};
static ata_async_state_t secondary_ata_state = {0};

uint32_t primary_num_sectors = 0;

// Function forward declarations
uint32_t hdc_get_primary_sector_count(void);
int validate_sector_range(uint64_t lba, uint8_t count, uint32_t max_sectors);
int ide_wait_for_drive_ready(void);

// Async disk operation functions
int hdc_read_sectors_async(uint64_t LBA, uint8_t sectorcount, void *buffer)
{
    // Validate inputs first
    uint32_t max_sectors = hdc_get_primary_sector_count();
    int validation_result = validate_sector_range(LBA, sectorcount, max_sectors);
    if (validation_result != DISK_SUCCESS) {
        return validation_result;
    }

    // Check if another operation is in progress
    if (primary_ata_state.operation_pending) {
        return DISK_ERROR_HARDWARE; // Controller busy
    }

    // Initialize async state
    primary_ata_state.operation_pending = 1;
    primary_ata_state.operation_complete = 0;
    primary_ata_state.operation_error = DISK_SUCCESS;
    primary_ata_state.sectors_remaining = sectorcount;
    primary_ata_state.current_buffer = buffer;
    primary_ata_state.current_lba = LBA;
    primary_ata_state.is_write_operation = 0;

    // Wait for drive ready with timeout
    int ready_result = ide_wait_for_drive_ready();
    if (ready_result != DISK_SUCCESS) {
        primary_ata_state.operation_pending = 0;
        return ready_result;
    }

    // Set up LBA addressing
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((LBA >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, sectorcount);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)LBA);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(LBA >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(LBA >> 16));

    // Issue read command - this will trigger interrupt when ready
    outb(ATA_PRIMARY_COMM_REGSTAT, ATA_CMD_READ_PIO);
    
    return DISK_SUCCESS; // Operation started, will complete asynchronously
}

int hdc_write_sectors_async(uint64_t LBA, uint8_t sectorcount, void *buffer)
{
    // Validate inputs first
    uint32_t max_sectors = hdc_get_primary_sector_count();
    int validation_result = validate_sector_range(LBA, sectorcount, max_sectors);
    if (validation_result != DISK_SUCCESS) {
        return validation_result;
    }

    // Check if another operation is in progress
    if (primary_ata_state.operation_pending) {
        return DISK_ERROR_HARDWARE; // Controller busy
    }

    // Initialize async state
    primary_ata_state.operation_pending = 1;
    primary_ata_state.operation_complete = 0;
    primary_ata_state.operation_error = DISK_SUCCESS;
    primary_ata_state.sectors_remaining = sectorcount;
    primary_ata_state.current_buffer = buffer;
    primary_ata_state.current_lba = LBA;
    primary_ata_state.is_write_operation = 1;

    // Wait for drive ready with timeout
    int ready_result = ide_wait_for_drive_ready();
    if (ready_result != DISK_SUCCESS) {
        primary_ata_state.operation_pending = 0;
        return ready_result;
    }

    // Set up LBA addressing
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((LBA >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, sectorcount);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)LBA);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(LBA >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(LBA >> 16));

    // Issue write command - this will trigger interrupt when ready for data
    outb(ATA_PRIMARY_COMM_REGSTAT, ATA_CMD_WRITE_PIO);

    return DISK_SUCCESS; // Operation started, will complete asynchronously
}

// Wait for async operation to complete
int hdc_wait_for_completion(uint32_t timeout_ms)
{
    uint32_t timeout_counter = timeout_ms * 1000; // Back to original timing
    
    while (timeout_counter > 0) {
        // Force fresh read of the volatile state
        volatile uint8_t pending = primary_ata_state.operation_pending;
        
        if (!pending) {
            break;
        }
        
        timeout_counter--;
        
        // Force memory access to prevent compiler optimization
        volatile uint8_t temp = primary_ata_state.operation_pending;
    }
    
    // Check final state
    if (primary_ata_state.operation_pending) {
        // Timeout occurred
        primary_ata_state.operation_pending = 0;
        primary_ata_state.operation_complete = 0;
        return DISK_ERROR_TIMEOUT;
    }
    
    // Return the result of the operation
    return primary_ata_state.operation_error;
}

// Check if async operation is complete (non-blocking)
int hdc_check_completion()
{
    if (primary_ata_state.operation_pending) {
        return -1; // Still in progress
    }
    return primary_ata_state.operation_error; // Completed, return result
}

uint32_t hdc_write_sectors(uint64_t LBA, uint8_t sectorcount, void *t);

// ATA interrupt handlers
void ata_primary_interrupt_handler(registers_t* regs)
{
    // Read status to acknowledge interrupt
    uint8_t status = inb(ATA_PRIMARY_COMM_REGSTAT);
    
    // Check for errors
    if (status & ATA_SR_ERR) {
        primary_ata_state.operation_error = DISK_ERROR_HARDWARE;
        primary_ata_state.operation_complete = 1;
        primary_ata_state.operation_pending = 0;
        return;
    }
    
    // Check if drive is ready for data transfer
    if (!(status & ATA_SR_DRQ) && primary_ata_state.sectors_remaining > 0) {
        // Still waiting for data ready
        return;
    }
    
    // Handle data transfer
    if (primary_ata_state.sectors_remaining > 0) {
        if (primary_ata_state.is_write_operation) {
            // Write one sector
            outportsw(ATA_PRIMARY_DATA, (void*)primary_ata_state.current_buffer, 256);
        } else {
            // Read one sector
            inportsw(ATA_PRIMARY_DATA, (void*)primary_ata_state.current_buffer, 256);
        }
        
        // Update state
        primary_ata_state.current_buffer = (void*)((uint8_t*)primary_ata_state.current_buffer + 512);
        primary_ata_state.current_lba++;
        primary_ata_state.sectors_remaining--;
        
        // If more sectors needed, continue operation
        if (primary_ata_state.sectors_remaining > 0) {
            return; // Wait for next interrupt
        }
    }
    
    // Operation complete
    primary_ata_state.operation_complete = 1;
    primary_ata_state.operation_pending = 0;
    primary_ata_state.operation_error = DISK_SUCCESS;
}

void ata_secondary_interrupt_handler(registers_t* regs)
{
    // Similar to primary handler but for secondary ATA controller
    uint8_t status = inb(ATA_SECONDARY_COMM_REGSTAT);
    
    if (status & ATA_SR_ERR) {
        secondary_ata_state.operation_error = DISK_ERROR_HARDWARE;
        secondary_ata_state.operation_complete = 1;
        secondary_ata_state.operation_pending = 0;
        return;
    }
    
    if (!(status & ATA_SR_DRQ) && secondary_ata_state.sectors_remaining > 0) {
        return;
    }
    
    if (secondary_ata_state.sectors_remaining > 0) {
        if (secondary_ata_state.is_write_operation) {
            outportsw(ATA_SECONDARY_DATA, (void*)secondary_ata_state.current_buffer, 256);
        } else {
            inportsw(ATA_SECONDARY_DATA, (void*)secondary_ata_state.current_buffer, 256);
        }
        
        secondary_ata_state.current_buffer = (void*)((uint8_t*)secondary_ata_state.current_buffer + 512);
        secondary_ata_state.current_lba++;
        secondary_ata_state.sectors_remaining--;
        
        if (secondary_ata_state.sectors_remaining > 0) {
            return;
        }
    }
    
    secondary_ata_state.operation_complete = 1;
    secondary_ata_state.operation_pending = 0;
    secondary_ata_state.operation_error = DISK_SUCCESS;
}

// Initialize interrupt-driven ATA
void initialise_ata()
{
    // Register interrupt handlers
    register_interrupt_handler(IRQ14, &ata_primary_interrupt_handler);
    register_interrupt_handler(IRQ15, &ata_secondary_interrupt_handler);
    
    // Clear any pending operations
    primary_ata_state.operation_pending = 0;
    primary_ata_state.operation_complete = 0;
    secondary_ata_state.operation_pending = 0;
    secondary_ata_state.operation_complete = 0;
    
    printf("[ATA] Interrupt-driven I/O initialized\n");
}

uint32_t hdc_get_primary_sector_count()
{
	return primary_num_sectors;
}

// Sector validation function
int validate_sector_range(uint64_t lba, uint8_t count, uint32_t max_sectors)
{
	if (lba >= max_sectors) {
		return DISK_ERROR_INVALID;
	}
	if (lba + count > max_sectors) {
		return DISK_ERROR_BOUNDS;
	}
	if (count == 0) {
		return DISK_ERROR_INVALID;
	}
	return DISK_SUCCESS;
}

int ide_wait_for_drive_ready()
{
	int count = 0;
	const int MAX_RETRIES = 10000;  // Increased timeout for better reliability
	
	while (count < MAX_RETRIES)
	{
		uint8_t status = inb(ATA_PRIMARY_COMM_REGSTAT);
		
		// Check for error conditions first
		if (status & ATA_SR_DF) {
			// Drive fault - serious hardware error
			return DISK_ERROR_HARDWARE;
		}
		
		if (status & ATA_SR_ERR) {
			// Error bit set - read error register for details
			return DISK_ERROR_HARDWARE;
		}
		
		// Check if drive is busy
		if (status & ATA_SR_BSY) {
			count++;
			// Small delay to prevent overwhelming the bus
			for (volatile int i = 0; i < 100; i++);
			continue;
		}
		
		// Check if drive is ready
		if (status & ATA_SR_DRDY) {
			return DISK_SUCCESS;
		}
		
		count++;
		// Small delay between checks
		for (volatile int i = 0; i < 100; i++);
	}
	
	// Timeout occurred
	return DISK_ERROR_TIMEOUT;
}

uint8_t hdc_identify_primary()
{
	uint8_t ofgc;
	ofgc = get_foreground_colour();
	set_foreground_colour(VGA_GREEN);
	printf("[Non-Volatile Storage]\n");
	set_foreground_colour(ofgc);

	inb(ATA_PRIMARY_COMM_REGSTAT);
	outb(ATA_PRIMARY_DRIVE_HEAD, 0xA0);
	inb(ATA_PRIMARY_COMM_REGSTAT);
	outb(ATA_PRIMARY_SECCOUNT, 0);
	inb(ATA_PRIMARY_COMM_REGSTAT);
	outb(ATA_PRIMARY_LBA_LO, 0);
	inb(ATA_PRIMARY_COMM_REGSTAT);
	outb(ATA_PRIMARY_LBA_MID, 0);
	inb(ATA_PRIMARY_COMM_REGSTAT);
	outb(ATA_PRIMARY_LBA_HI, 0);
	inb(ATA_PRIMARY_COMM_REGSTAT);
	outb(ATA_PRIMARY_COMM_REGSTAT, 0xEC);
	outb(ATA_PRIMARY_COMM_REGSTAT, 0xE7);

	// Read the status port. If it's zero, the drive does not exist.
	uint8_t status = inb(ATA_PRIMARY_COMM_REGSTAT);
	if (status == 0)
	{
		printf("disc not found\n");
		return 0;
	}
	ide_wait_for_drive_ready();

	uint8_t mid = inb(ATA_PRIMARY_LBA_MID);
	uint8_t hi = inb(ATA_PRIMARY_LBA_HI);
	if (mid || hi)
	{
		printf("non ATA drive\n");
		return 0;
	}
	else
	{
		//printf("ATA Disc\n");
	}

	if (status & STAT_ERR)
	{
		printf("drive error\n");
		return 0;
	}
	uint16_t buff[256];
	inportsw(ATA_PRIMARY_DATA, buff, 256);
	uint32_t lb = *((uint32_t *)&buff[100]);

	IDENTIFY_DEVICE_DATA *deviced = (IDENTIFY_DEVICE_DATA *)&buff[0];
	int k;

	printf("Primary ATA        : ");
	for (k = 0; k < 39; k += 2)
	{
		char c1 = deviced->ModelNumber[k];
		char c2 = deviced->ModelNumber[k + 1];
		putchar(c2);
		putchar(c1);
	}
	putchar('\n');
	printf("Disc Size          : %u Mb ( %u Sectors) \n", ((lb *512) / (1000 * 1000)), lb  );
	primary_num_sectors = lb;
	//printf("Num Sectors=%u\n",lb);
	return 1;
}
uint8_t hdc_identify_secondary()
{
	return 1;
}
extern volatile int ide_irq_ready;

uint32_t hdc_write_sectors(uint64_t LBA, uint8_t sectorcount, void *t)
{
	// Validate sector range
	int validation_result = validate_sector_range(LBA, sectorcount, primary_num_sectors);
	if (validation_result != DISK_SUCCESS) {
		return validation_result;
	}
	
	// Validate buffer pointer
	if (!t) {
		return DISK_ERROR_INVALID;
	}
	
	ide_irq_ready = 0;
	unsigned char *target = t;
	
	// Set up LBA48 addressing
	outb(ATA_PRIMARY_DRIVE_HEAD, 0x40);
	outb(ATA_PRIMARY_SECCOUNT, (sectorcount >> 8) & 0xFF);
	outb(ATA_PRIMARY_LBA_LO, (LBA >> 24) & 0xFF);
	outb(ATA_PRIMARY_LBA_MID, (LBA >> 32) & 0xFF);
	outb(ATA_PRIMARY_LBA_HI, (LBA >> 40) & 0xFF);
	outb(ATA_PRIMARY_SECCOUNT, sectorcount & 0xFF);
	outb(ATA_PRIMARY_LBA_LO, LBA & 0xFF);
	outb(ATA_PRIMARY_LBA_MID, (LBA >> 8) & 0xFF);
	outb(ATA_PRIMARY_LBA_HI, (LBA >> 16) & 0xFF);
	outb(ATA_PRIMARY_COMM_REGSTAT, ATA_CMD_WRITE_PIO_EXT);
	
	// Wait for drive to be ready
	int wait_result = ide_wait_for_drive_ready();
	if (wait_result != DISK_SUCCESS) {
		return wait_result;
	}
	
	// Write sectors
	for (uint8_t i = 0; i < sectorcount; i++)
	{
		ide_irq_ready = 0;
		outportsw(ATA_PRIMARY_DATA, (void *)target, 256);
		target += 512;
		
		// Wait for drive ready after each sector
		wait_result = ide_wait_for_drive_ready();
		if (wait_result != DISK_SUCCESS) {
			return wait_result;
		}
	}
	
	return DISK_SUCCESS;
}

uint32_t hdc_read_sectors(uint64_t LBA, uint8_t sectorcount, void *t)
{
	// Validate sector range
	int validation_result = validate_sector_range(LBA, sectorcount, primary_num_sectors);
	if (validation_result != DISK_SUCCESS) {
		return validation_result;
	}
	
	// Validate buffer pointer
	if (!t) {
		return DISK_ERROR_INVALID;
	}
	
	ide_irq_ready = 0;
	unsigned char *target = t;
	
	// Set up LBA48 addressing
	outb(ATA_PRIMARY_DRIVE_HEAD, 0x40);
	outb(ATA_PRIMARY_SECCOUNT, (sectorcount >> 8) & 0xFF);
	outb(ATA_PRIMARY_LBA_LO, (LBA >> 24) & 0xFF);
	outb(ATA_PRIMARY_LBA_MID, (LBA >> 32) & 0xFF);
	outb(ATA_PRIMARY_LBA_HI, (LBA >> 40) & 0xFF);
	outb(ATA_PRIMARY_SECCOUNT, sectorcount & 0xFF);
	outb(ATA_PRIMARY_LBA_LO, LBA & 0xFF);
	outb(ATA_PRIMARY_LBA_MID, (LBA >> 8) & 0xFF);
	outb(ATA_PRIMARY_LBA_HI, (LBA >> 16) & 0xFF);
	outb(ATA_PRIMARY_COMM_REGSTAT, ATA_CMD_READ_PIO_EXT);
	
	// Wait for drive to be ready
	int wait_result = ide_wait_for_drive_ready();
	if (wait_result != DISK_SUCCESS) {
		return wait_result;
	}
	
	// Read sectors
	for (uint32_t i = 0; i < sectorcount; i++)
	{
		ide_irq_ready = 0;
		inportsw(ATA_PRIMARY_DATA, (void *)target, 256);
		target += 512;
		
		// Wait for drive ready after each sector (except last)
		if (i < sectorcount - 1) {
			wait_result = ide_wait_for_drive_ready();
			if (wait_result != DISK_SUCCESS) {
				return wait_result;
			}
		}
	}
	
	return DISK_SUCCESS;
}

uint32_t ramdisc_start = 0;
uint32_t ramdisc_end = 0;
uint32_t ramdisc_sector_count = 0;

uint32_t rdc_read_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer)
{
	// Validate buffer pointer
	if (!buffer) {
		return DISK_ERROR_INVALID;
	}
	
	// Validate sector range for RAM disk
	if (ramdisc_sector_count > 0) {
		int validation_result = validate_sector_range(LBA, sectorcount, ramdisc_sector_count);
		if (validation_result != DISK_SUCCESS) {
			return validation_result;
		}
	}
	
	uint32_t startaddr = ramdisc_start + LBA * 512;
	uint32_t endaddr = startaddr + sectorcount * 512;
	
	// Additional bounds checking for RAM disk
	if (endaddr > ramdisc_end) {
		return DISK_ERROR_BOUNDS;
	}
	if (startaddr < ramdisc_start) {
		return DISK_ERROR_BOUNDS;
	}
	
	uint32_t len = endaddr - startaddr;
	memcpy(buffer, (void *)startaddr, len);
	return DISK_SUCCESS;
}

uint32_t rdc_write_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer)
{
	// Validate buffer pointer
	if (!buffer) {
		return DISK_ERROR_INVALID;
	}
	
	// Validate sector range for RAM disk
	if (ramdisc_sector_count > 0) {
		int validation_result = validate_sector_range(LBA, sectorcount, ramdisc_sector_count);
		if (validation_result != DISK_SUCCESS) {
			return validation_result;
		}
	}
	
	uint32_t startaddr = ramdisc_start + LBA * 512;
	uint32_t endaddr = startaddr + sectorcount * 512;
	
	// Additional bounds checking for RAM disk
	if (endaddr > ramdisc_end) {
		return DISK_ERROR_BOUNDS;
	}
	if (startaddr < ramdisc_start) {
		return DISK_ERROR_BOUNDS;
	}
	
	uint32_t len = endaddr - startaddr;
	memcpy((void *)startaddr, buffer, len);
	return DISK_SUCCESS;
}
