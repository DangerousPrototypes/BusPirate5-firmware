/**
 * @file		spi.h
 * @author		Andrew Loebs
 * @brief		Header file of the spi module
 *
 * SPI1 master driver (interfaces SPI NAND flash chip)
 *
 */

#ifndef __SPI_H
#define __SPI_H

#include <stddef.h>
#include <stdint.h>

/// @brief SPI return statuses
#define SPI_RET_OK 0
#define SPI_RET_TIMEOUT -1
#define SPI_RET_NULL_PTR -2

/// @brief Initializes the spi driver
void nand_spi_init(void);

/// @brief Writes data to the bus
/// @note Caller is expected to drive the chip select line for the relevant device
int nand_spi_write(const uint8_t* write_buff, size_t write_len, uint32_t timeout_ms);

/// @brief Reads data from the bus
/// @note Caller is expected to drive the chip select line for the relevant device
/// @note Transmits 0x00 on the MOSI line during the transaction
int nand_spi_read(uint8_t* read_buff, size_t read_len, uint32_t timeout_ms);

/// @brief Writes/reads data to/from to the bus
/// @note Caller is expected to drive the chip select line for the relevant device
int nand_spi_write_read(const uint8_t* write_buff, uint8_t* read_buff, size_t transfer_len, uint32_t timeout_ms);

#endif // __SPI_H
