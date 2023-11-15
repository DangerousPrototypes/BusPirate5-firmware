/**
 * @file		nand.c
 * @author		Andrew Loebs
 * @brief		"Glue" layer between dhara and spi_nand module
 *
 */

#include "nand.h"

#include <stdbool.h>

#include "../modules/spi_nand.h"

// public function definitions
int dhara_nand_is_bad(const struct dhara_nand *n, dhara_block_t b)
{
    // construct row address
    row_address_t row = {.block = b, .page = 0};
    // call spi_nand layer for block status
    bool is_bad;
    int ret = spi_nand_block_is_bad(row, &is_bad);
    if (SPI_NAND_RET_OK != ret) {
        // if we get a bad return, we'll just call this block bad
        is_bad = true;
    }

    return (int)is_bad;
}

void dhara_nand_mark_bad(const struct dhara_nand *n, dhara_block_t b)
{
    // construct row address
    row_address_t row = {.block = b, .page = 0};
    // call spi_nand layer
    spi_nand_block_mark_bad(row); // ignore ret
}

int dhara_nand_erase(const struct dhara_nand *n, dhara_block_t b, dhara_error_t *err)
{
    // construct row address
    row_address_t row = {.block = b, .page = 0};
    // call spi_nand layer
    int ret = spi_nand_block_erase(row);
    if (SPI_NAND_RET_OK == ret) { // success
        return 0;
    }
    else if (SPI_NAND_RET_E_FAIL == ret) { // failed internally on nand
        *err = DHARA_E_BAD_BLOCK;
        return -1;
    }
    else { // failed for some other reason
        return -1;
    }
}

int dhara_nand_prog(const struct dhara_nand *n, dhara_page_t p, const uint8_t *data,
                    dhara_error_t *err)
{
    // construct row address -- dhara's page address is identical to an MT29F row address
    row_address_t row = {.whole = p};
    // call spi_nand layer
    int ret = spi_nand_page_program(row, 0, data, SPI_NAND_PAGE_SIZE);
    if (SPI_NAND_RET_OK == ret) { // success
        return 0;
    }
    else if (SPI_NAND_RET_P_FAIL == ret) { // failed internally on nand
        *err = DHARA_E_BAD_BLOCK;
        return -1;
    }
    else { // failed for some other reason
        return -1;
    }
}

int dhara_nand_is_free(const struct dhara_nand *n, dhara_page_t p)
{
    // construct row address -- dhara's page address is identical to an MT29F row address
    row_address_t row = {.whole = p};
    // call spi_nand layer
    bool is_free;
    int ret = spi_nand_page_is_free(row, &is_free);
    if (SPI_NAND_RET_OK != ret) {
        // if we get a bad return, we'll report the page as "not free"
        is_free = false;
    }

    return (int)is_free;
}

int dhara_nand_read(const struct dhara_nand *n, dhara_page_t p, size_t offset, size_t length,
                    uint8_t *data, dhara_error_t *err)
{
    // construct row address -- dhara's page address is identical to an MT29F row address
    row_address_t row = {.whole = p};
    // call spi_nand layer
    int ret = spi_nand_page_read(row, offset, data, length);
    if (SPI_NAND_RET_OK == ret) { // success
        return 0;
    }
    else if (SPI_NAND_RET_ECC_ERR == ret) { // ECC failure
        *err = DHARA_E_ECC;
        return -1;
    }
    else { // failed for some other reason
        return -1;
    }
}

/* Read a page from one location and reprogram it in another location.
 * This might be done using the chip's internal buffers, but it must use
 * ECC.
 */
int dhara_nand_copy(const struct dhara_nand *n, dhara_page_t src, dhara_page_t dst,
                    dhara_error_t *err)
{
    // construct row addresses -- dhara's page address is identical to an MT29F row address
    row_address_t source = {.whole = src};
    row_address_t dest = {.whole = dst};
    // call spi_nand layer
    int ret = spi_nand_page_copy(source, dest);
    if (SPI_NAND_RET_OK == ret) { // success
        return 0;
    }
    else if (SPI_NAND_RET_ECC_ERR == ret) { // ECC failure on read
        *err = DHARA_E_ECC;
        return -1;
    }
    else if (SPI_NAND_RET_P_FAIL == ret) { // program failure
        *err = DHARA_E_BAD_BLOCK;
        return -1;
    }
    else { // failed for some other reason
        return -1;
    }
}
