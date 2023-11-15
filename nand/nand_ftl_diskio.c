/**
 * @file		nand_ftl_diskio.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the nand ftl diskio module
 *
 */

#include "nand_ftl_diskio.h"

#include "../dhara/map.h"
#include "../dhara/nand.h"
#include "shell.h"
#include "spi_nand.h"

// private variables
static bool initialized = false;
static struct dhara_map map;
static uint8_t page_buffer[SPI_NAND_PAGE_SIZE];
static struct dhara_nand nand = {
    .log2_page_size = SPI_NAND_LOG2_PAGE_SIZE,
    .log2_ppb = SPI_NAND_LOG2_PAGES_PER_BLOCK,
    .num_blocks = SPI_NAND_BLOCKS_PER_LUN,
};

// public function definitions
DSTATUS nand_ftl_diskio_initialize(void)
{
    // init flash management stack
    int ret = spi_nand_init();
    if (SPI_NAND_RET_OK != ret) {
        shell_printf_line("spi_nand_init failed, status: %d.", ret);
        return STA_NOINIT;
    }
    // init flash translation layer
    dhara_map_init(&map, &nand, page_buffer, 4);
    dhara_error_t err = DHARA_E_NONE;
    ret = dhara_map_resume(&map, &err);
    shell_printf_line("dhara resume return: %d, error: %d", ret, err);
    // map_resume will return a bad status in the case of an empty map, however this just
    // means that the file system is empty

    // TODO: Flag statuses from dhara that do not indicate an empty map
    initialized = true;
    return 0;
}

DSTATUS nand_ftl_diskio_status(void)
{
    if (!initialized) {
        return STA_NOINIT;
    }
    else {
        return 0;
    }
}

DRESULT nand_ftl_diskio_read(BYTE *buff, LBA_t sector, UINT count)
{
    dhara_error_t err;
    // read *count* consecutive sectors
    for (int i = 0; i < count; i++) {
        int ret = dhara_map_read(&map, sector, buff, &err);
        if (ret) {
            shell_printf_line("dhara read failed: %d, error: %d", ret, err);
            return RES_ERROR;
        }
        buff += SPI_NAND_PAGE_SIZE; // sector size == page size
        sector++;
    }

    return RES_OK;
}

DRESULT nand_ftl_diskio_write(const BYTE *buff, LBA_t sector, UINT count)
{
    dhara_error_t err;
    // write *count* consecutive sectors
    for (int i = 0; i < count; i++) {
        int ret = dhara_map_write(&map, sector, buff, &err);
        if (ret) {
            shell_printf_line("dhara write failed: %d, error: %d", ret, err);
            return RES_ERROR;
        }
        buff += SPI_NAND_PAGE_SIZE; // sector size == page size
        sector++;
    }

    return RES_OK;
}

DRESULT nand_ftl_diskio_ioctl(BYTE cmd, void *buff)
{
    dhara_error_t err;

    switch (cmd) {
        case CTRL_SYNC:;
            ;
            int ret = dhara_map_sync(&map, &err);
            if (ret) {
                shell_printf_line("dhara sync failed: %d, error: %d", ret, err);
                return RES_ERROR;
            }
            break;
        case GET_SECTOR_COUNT:;
            ;
            dhara_sector_t sector_count = dhara_map_capacity(&map);
            shell_printf_line("dhara capacity: %d", sector_count);
            LBA_t *sector_count_out = (LBA_t *)buff;
            *sector_count_out = sector_count;
            break;
        case GET_SECTOR_SIZE:
            ;
            WORD *sector_size_out = (WORD *)buff;
            *sector_size_out = SPI_NAND_PAGE_SIZE;
            break;
        case GET_BLOCK_SIZE:
            ;
            DWORD *block_size_out = (DWORD *)buff;
            *block_size_out = SPI_NAND_PAGES_PER_BLOCK;
            break;
        case CTRL_TRIM:
            ;
            LBA_t *args = (LBA_t *)buff;
            LBA_t start = args[0];
            LBA_t end = args[1];
            while (start <= end) {
                int ret = dhara_map_trim(&map, start, &err);
                if (ret) {
                    shell_printf_line("dhara trim failed: %d, error: %d", ret, err);
                    return RES_ERROR;
                }
                start++;
            }
            break;
        default:
            return RES_PARERR;
    }

    return RES_OK;
}
