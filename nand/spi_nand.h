/**
 * @file		spi_nand.h
 * @author		Andrew Loebs
 * @brief		Header file of the spi nand module
 *
 * SPI NAND flash chip driver for the Micron MT29F1G01ABAFDWB.
 *
 */

#ifndef __SPI_NAND_H
#define __SPI_NAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// @brief SPI return statuses
enum {
    SPI_NAND_RET_OK = 0,
    SPI_NAND_RET_BAD_SPI = -1,
    SPI_NAND_RET_TIMEOUT = -2,
    SPI_NAND_RET_DEVICE_ID = -3,
    SPI_NAND_RET_BAD_ADDRESS = -4,
    SPI_NAND_RET_INVALID_LEN = -5,
    SPI_NAND_RET_ECC_REFRESH = -6,
    SPI_NAND_RET_ECC_ERR = -7,
    SPI_NAND_RET_P_FAIL = -8,
    SPI_NAND_RET_E_FAIL = -9,
};

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// TODO: allow the following to be dynamic values from the flash's
//       parameter page, rather than hard-coded?
//
// Unfortunately, multiple files use these values directly, so the
// change for dynamically supporting multiple flash chips would be
// larger.  Files that would be impacted:
//
// 1. dhara/nand.c
// 2. nand/nand_ftl_diskio.c
// 3. nand/spi_nand.c
// 4. nand/attic/mem.c
// 5. nand/attic/shell_cmd.c
//
// Instead, define relevant values here in a single location, within
// #ifdef guards for each supported chip.  This way, the externally
// impacting values are at least well-defined.  Also define the
// INTERNAL-ONLY value with similar #ifdef guards in spi_nand.c.
//
#define SPI_NAND_PAGE_SIZE       2048 // dhara/nand.c uses this exactly once
#define SPI_NAND_OOB_SIZE        64
#define SPI_NAND_PAGES_PER_BLOCK 64
#define SPI_NAND_BLOCKS_PER_LUN  1024

#define SPI_NAND_LOG2_PAGE_SIZE       11
#define SPI_NAND_LOG2_PAGES_PER_BLOCK 6

#define SPI_NAND_MAX_PAGE_ADDRESS  (SPI_NAND_PAGES_PER_BLOCK - 1) // zero-indexed
#define SPI_NAND_MAX_BLOCK_ADDRESS (SPI_NAND_BLOCKS_PER_LUN - 1)  // zero-indexed
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


/// @brief Nand row address
typedef union {
    uint32_t whole;
    struct {
        /// valid range 0-63
        uint32_t page  :  6;    // TODO: bitcount == SPI_NAND_LOG2_PAGES_PER_BLOCK
        /// valid range 0-1023
        uint32_t block : 26;    // TODO: bitcount == 32 - SPI_NAND_LOG2_PAGES_PER_BLOCK
    };
} row_address_t;
/// @brief Nand column address (valid range 0-2175)
typedef uint16_t column_address_t;

/// @brief Initializes the spi nand driver
int spi_nand_init(void);

/// @brief Performs a read page operation
int spi_nand_page_read(row_address_t row, column_address_t column, uint8_t *data_out,
                       size_t read_len);

/// @brief Performs a page program operation
int spi_nand_page_program(row_address_t row, column_address_t column, const uint8_t *data_in,
                          size_t write_len);

/// @brief Copies the source page to the destination page using nand's internal cache
int spi_nand_page_copy(row_address_t src, row_address_t dest);

/// @brief Performs a block erase operation
/// @note Block operation -- page component of row address is ignored
int spi_nand_block_erase(row_address_t row);

/// @brief Checks if a given block is bad
/// @note Block operation -- page component of row address is ignored
/// @return SPI_NAND_RET_OK if good block, SPI_NAND_RET_BAD_BLOCK if bad, other returns if error is
/// encountered
int spi_nand_block_is_bad(row_address_t row, bool *is_bad);

/// @brief Marks a given block as bad
/// @note Block operation -- page component of row address is ignored
int spi_nand_block_mark_bad(row_address_t row);

/// @brief Checks if a given page is free
int spi_nand_page_is_free(row_address_t row, bool *is_free);

/// @brief Erases all blocks from the device, ignoring those marked as bad
int spi_nand_clear(void);

#endif // __SPI_NAND_H
