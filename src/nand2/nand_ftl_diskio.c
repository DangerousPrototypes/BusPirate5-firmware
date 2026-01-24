/**
 * @file		nand_ftl_diskio.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the nand ftl diskio module
 *
 */
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "pirate.h"

#include "../fatfs/ff.h"     // BYTE type
#include "../fatfs/diskio.h" // types from the diskio driver

#include "nand_ftl_diskio.h"
#include "include/spi_nand_flash.h"

//#include "../dhara/map.h"
//#include "../dhara/nand.h"
// #include "shell.h"
//#include "nand/spi_nand.h"

// private variables
static bool initialized = false;
//static struct dhara_map map;
//static uint8_t page_buffer[SPI_NAND_PAGE_SIZE];
//static struct dhara_nand dhara_nand_parameters = {};
static mutex_t diskio_mutex;
static spi_nand_flash_device_t *flash;

#define SPI_NAND_LOG2_PAGE_SIZE              (  11)
#define SPI_NAND_LOG2_PAGES_PER_ERASE_BLOCK  (   6)
#define SPI_NAND_PAGES_PER_ERASE_BLOCK       (1 << SPI_NAND_LOG2_PAGES_PER_ERASE_BLOCK)
#define SPI_NAND_PAGE_SIZE                   (1 << SPI_NAND_LOG2_PAGE_SIZE)

// public function definitions
DSTATUS diskio_initialize(BYTE drv) {
    if (drv) {
        return STA_NOINIT; /* Supports only drive 0 */
    }
    if (!mutex_is_initialized(&diskio_mutex)) {
        mutex_init(&diskio_mutex);
    }
    #define SPI_PORT spi0
    #define PIN_CS   26  // GP17 - Chip Select (directly controlled)
    // init flash management stack
    //int ret = spi_nand_init(&dhara_nand_parameters);
    spi_nand_flash_config_t nand_flash_config = {
        .spi = SPI_PORT,
        .cs_pin = PIN_CS,
        .io_mode = SPI_NAND_IO_MODE_SIO,
        .gc_factor = 4, //same as previous NAND driver
    };

    spi_nand_flash_device_t *nand_flash_device_handle;    
    esp_err_t ret = spi_nand_flash_init_device(&nand_flash_config, &nand_flash_device_handle);
    if (ret != ESP_OK) {
        //LOG_E(TAG, "Failed to initialize NAND flash: %d", ret);
        //*out_handle = NULL;
        return STA_NOINIT;
    }

    flash = nand_flash_device_handle;

    // TODO: Flag statuses from dhara that do not indicate an empty map
    initialized = true;
    return 0;
}

DSTATUS diskio_status(BYTE drv) {
    if (drv) {
        return STA_NOINIT; /* Supports only drive 0 */
    }

    if (!initialized) {
        return STA_NOINIT;
    } else {
        return 0;
    }
}

DRESULT diskio_read(BYTE drv, BYTE* buff, LBA_t sector, UINT count) {
    //dhara_error_t err;

    if (drv) {
        return STA_NOINIT; /* Supports only drive 0 */
    }
    mutex_enter_blocking(&diskio_mutex);
    // read *count* consecutive sectors
    for (int i = 0; i < count; i++) {
        //int ret = dhara_map_read(&map, sector, buff, &err);
        esp_err_t ret = spi_nand_flash_read_sector(flash, buff, sector);
        if (ret != ESP_OK) {
            // printf("dhara read failed: %d, error: %d", ret, err);
            return RES_ERROR;
        }
        buff += SPI_NAND_PAGE_SIZE; // sector size == page size
        sector++;
    }
    mutex_exit(&diskio_mutex);
    return RES_OK;
}

DRESULT diskio_write(BYTE drv, const BYTE* buff, LBA_t sector, UINT count) {
    //dhara_error_t err;

    if (drv) {
        return STA_NOINIT; /* Supports only drive 0 */
    }
    mutex_enter_blocking(&diskio_mutex);
    // write *count* consecutive sectors
    for (int i = 0; i < count; i++) {  
        //int ret = dhara_map_write(&map, sector, buff, &err);
        esp_err_t ret = spi_nand_flash_write_sector(flash, buff, sector);
        if (ret!= ESP_OK) {
            // printf("dhara write failed: %d, error: %d", ret, err);
            return RES_ERROR;
        }
        buff += SPI_NAND_PAGE_SIZE; // sector size == page size
        sector++;
    }
    mutex_exit(&diskio_mutex);
    return RES_OK;
}

DRESULT diskio_ioctl(BYTE drv, BYTE cmd, void* buff) {
    if (drv) {
        return STA_NOINIT; /* Supports only drive 0 */
    }
    switch (cmd) {
        case CTRL_SYNC:;
            ;
            mutex_enter_blocking(&diskio_mutex);
            //int ret = dhara_map_sync(&map, &err);
            esp_err_t ret = spi_nand_flash_sync(flash);
            mutex_exit(&diskio_mutex);
            if (ret!= ESP_OK) {
                // printf("dhara sync failed: %d, error: %d", ret, err);
                return RES_ERROR;
            }
            break;
        case GET_SECTOR_COUNT:;
            ;
            //dhara_sector_t sector_count = dhara_map_capacity(&map);
            LBA_t sector_count = 0;
            spi_nand_flash_get_capacity(flash, &sector_count);
            // printf("dhara capacity: %d", sector_count);
            LBA_t* sector_count_out = (LBA_t*)buff;
            *sector_count_out = sector_count;
            break;
        case GET_SECTOR_SIZE:;
            WORD* sector_size_out = (WORD*)buff;
            *sector_size_out = SPI_NAND_PAGE_SIZE;
            break;
        case GET_BLOCK_SIZE:;
            DWORD* block_size_out = (DWORD*)buff;
            *block_size_out = SPI_NAND_PAGES_PER_ERASE_BLOCK;
            break;
        case CTRL_TRIM:;
            LBA_t* args = (LBA_t*)buff;
            LBA_t start = args[0];
            LBA_t end = args[1];
            mutex_enter_blocking(&diskio_mutex);
            while (start <= end) {
                //int ret = dhara_map_trim(&map, start, &err);
                esp_err_t ret = spi_nand_flash_trim(flash, start);
                if (ret!= ESP_OK) {
                    // printf("dhara trim failed: %d, error: %d", ret, err);
                    return RES_ERROR;
                }
                start++;
            }
            mutex_exit(&diskio_mutex);
            break;
        default:
            return RES_PARERR;
    }
    return RES_OK;
}
