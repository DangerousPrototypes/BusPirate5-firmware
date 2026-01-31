/**
 * @file bpio_i2c.h
 * @brief BPIO I2C protocol transaction handler.
 * @details Provides binary protocol I2C transaction handling.
 */

#ifndef BPIO_I2C_H
#define BPIO_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include "bpio_reader.h"

// Forward declaration of the request structure (defined in bpio_transactions.h)
struct bpio_data_request_t;

/**
 * @brief Perform an I2C transaction.
 * @param request     Transaction request structure containing debug flags, byte counts, and control flags
 * @param data_write  Flatbuffer vector containing data to write
 * @param data_read   Buffer to store read data
 * @return            HWI2C_OK on success, error code on failure
 */
uint32_t bpio_hwi2c_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);

#endif // BPIO_I2C_H