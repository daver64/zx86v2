/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/
#include "common.h"
#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */

// Disk operation result codes
#define DISK_SUCCESS          0
#define DISK_ERROR_INVALID    -1
#define DISK_ERROR_TIMEOUT    -2
#define DISK_ERROR_HARDWARE   -3
#define DISK_ERROR_BOUNDS     -4

// Function declarations from disc.c
uint32_t hdc_get_primary_sector_count(void);
uint8_t hdc_identify_primary(void);
uint32_t hdc_read_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer);
uint32_t hdc_write_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer);
uint32_t rdc_read_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer);
uint32_t rdc_write_sectors(uint64_t LBA, uint8_t sectorcount, void *buffer);

// Async function declarations from disc.c
int hdc_read_sectors_async(uint64_t LBA, uint8_t sectorcount, void *buffer);
int hdc_write_sectors_async(uint64_t LBA, uint8_t sectorcount, void *buffer);
int hdc_wait_for_completion(uint32_t timeout_ms);
int hdc_check_completion(void);

// Configuration: Enable async I/O (set to 1 for interrupt-driven, 0 for legacy polling)
#define USE_ASYNC_DISK_IO 1
#define DISK_TIMEOUT_MS 5000  // 5 second timeout for disk operations

// Hybrid mode: allow switching between async and sync for different operations
static int use_async_for_operation = 1;  // 1 = async, 0 = sync

// Function to control disk I/O mode
void set_disk_sync_mode(int sync_mode) {
    use_async_for_operation = !sync_mode;  // sync_mode=1 means disable async
}

/* Definitions of physical drive number for each drive */
#define DEV_IDE		0
#define DEV_RAM		1	/* Example: Map Ramdisk to physical drive 0 */


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
		BYTE pdrv		/* Physical drive nmuber to identify the drive */
		)
{
	DSTATUS stat;
	int result;

	switch (pdrv) {

		case DEV_IDE:
			// Check if primary drive has been identified and has sectors
			if (hdc_get_primary_sector_count() == 0) {
				return STA_NOINIT;
			}
			return 0;  // Drive ready
			break;

		case DEV_RAM :
			// RAM disk is always ready if initialized
			return 0;
			break;

	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
		BYTE pdrv				/* Physical drive nmuber to identify the drive */
		)
{
	DSTATUS stat;
	int result;

	switch (pdrv) {
		case DEV_IDE:
		//printf("initialise IDE\n");
			// Check if already initialized
			if (hdc_get_primary_sector_count() > 0) {
				return 0;  // Already initialized
			}
			// Initialize IDE controller and detect drives
			result = hdc_identify_primary();
			if (result != 1) {
				return STA_NOINIT;
			}
			return 0;  // Successfully initialized
			break;

		case DEV_RAM :
			// RAM disk is always available and ready
			return 0;
			break;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read (
		BYTE pdrv,		/* Physical drive nmuber to identify the drive */
		BYTE *buff,		/* Data buffer to store read data */
		LBA_t sector,	/* Start sector in LBA */
		UINT count		/* Number of sectors to read */
		)
{
	DRESULT res;
	int result;
	switch (pdrv) {

		case DEV_IDE:
			//printf("Disc Read\n");
#if USE_ASYNC_DISK_IO
			if (use_async_for_operation) {
				// Use interrupt-driven async I/O
				result = hdc_read_sectors_async(sector, count, buff);
				if (result == DISK_SUCCESS) {
					// Wait for completion with timeout
					result = hdc_wait_for_completion(DISK_TIMEOUT_MS);
				}
			} else {
				// Use legacy blocking I/O for complex operations
				result = hdc_read_sectors(sector, count, buff);
			}
#else
			// Use legacy blocking I/O
			result = hdc_read_sectors(sector, count, buff);
#endif
			if (result != DISK_SUCCESS) {
				return RES_ERROR;
			}
			return RES_OK;
			break;

		case DEV_RAM :
			//printf("Ram Disc Read\n");
			result = rdc_read_sectors(sector,count,buff);
			if (result != DISK_SUCCESS) {
				return RES_ERROR;
			}
			return RES_OK;
			break;
	}

	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0
DRESULT disk_write (
		BYTE pdrv,			/* Physical drive nmuber to identify the drive */
		const BYTE *buff,	/* Data to be written */
		LBA_t sector,		/* Start sector in LBA */
		UINT count			/* Number of sectors to write */
		)
{
	DRESULT res;
	int result;

	switch (pdrv) {

		case DEV_IDE:
			//printf("Disk Write\n");
#if USE_ASYNC_DISK_IO
			if (use_async_for_operation) {
				// Use interrupt-driven async I/O
				result = hdc_write_sectors_async(sector, count, (void*)buff);
				if (result == DISK_SUCCESS) {
					// Wait for completion with timeout
					result = hdc_wait_for_completion(DISK_TIMEOUT_MS);
				}
			} else {
				// Use legacy blocking I/O for complex operations
				result = hdc_write_sectors(sector, count, (void*)buff);
			}
#else
			// Use legacy blocking I/O
			result = hdc_write_sectors(sector, count, (void*)buff);
#endif
			if (result != DISK_SUCCESS) {
				return RES_ERROR;
			}
			return RES_OK;
			break;
		case DEV_RAM :
			//printf("Ram Disk Write\n");
			result = rdc_write_sectors(sector,count,(void*)buff);
			if (result != DISK_SUCCESS) {
				return RES_ERROR;
			}
			return RES_OK;
			break;
	}

	return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl (
		BYTE pdrv,		/* Physical drive nmuber (0..) */
		BYTE cmd,		/* Control code */
		void *buff		/* Buffer to send/receive control data */
		)
{
	DRESULT res;
	int result;

	switch (pdrv) {
		case DEV_IDE:
			{
				if(cmd==GET_SECTOR_COUNT)
				{
					uint32_t sc=hdc_get_primary_sector_count();
					//printf("ioctl GET_SECTOR_COUNT %u\n",sc);
					*((LBA_t*)buff)=sc;
				}
				result=0;
				return RES_OK;
			}break;

		case DEV_RAM :
			result=0;
			return RES_OK;
			break;
	}

	return RES_PARERR;
}

