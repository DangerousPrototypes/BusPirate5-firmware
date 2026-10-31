/**
 * @file bpio_spi.h
 * @brief BPIO SPI protocol transaction handler.
 * @details Provides binary protocol SPI transaction handling.
 */

#ifndef BPIO_SPI_H
#define BPIO_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include "bpio_reader.h"

// Forward declaration of the request structure (defined in bpio_transactions.h)
struct bpio_data_request_t;

/**
 * @brief Perform an SPI transaction.
 * @param request     Transaction request structure containing debug flags, byte counts, and control flags
 * @param data_write  Flatbuffer vector containing data to write
 * @param data_read   Buffer to store read data
 * @return            0 on success, non-zero on error
 */
uint32_t bpio_hwspi_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);

#endif // BPIO_SPI_H