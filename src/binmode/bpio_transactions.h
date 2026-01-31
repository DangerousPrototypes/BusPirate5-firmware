/**
 * @file bpio_transactions.h
 * @brief Common data structures for BPIO transaction handlers
 * 
 * This file defines the shared data request structure used by all
 * Binary Protocol IO (BPIO) mode transaction handlers.
 * 
 * @author Bus Pirate Project
 * @date 2024-2026
 */

#ifndef BPIO_TRANSACTIONS_H
#define BPIO_TRANSACTIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "bpio_reader.h"

/**
 * @brief BPIO transaction request structure
 * 
 * This structure encapsulates all parameters needed for a binary
 * protocol transaction across different protocol modes (I2C, SPI,
 * 1-Wire, 2-Wire, 3-Wire, etc.).
 * 
 * @note Not all fields are used by all protocol handlers
 * @note bitwise_ops is specific to 2-wire and 3-wire modes
 */
struct bpio_data_request_t {
    bool debug;                               /**< Enable debug output to console */
    bool start_main;                          /**< Assert start/CS condition */
    bool start_alt;                           /**< Assert alternate start/mode */
    uint16_t bytes_write;                     /**< Number of bytes to write */
    uint16_t bytes_read;                      /**< Number of bytes to read */
    const char *data_buf;                     /**< Pointer to data buffer */
    bool stop_main;                           /**< Assert stop/CS deassert */
    bool stop_alt;                            /**< Assert alternate stop */
    flatbuffers_uint8_vec_t bitwise_ops;      /**< Bitwise pin operations (2-wire/3-wire only) */
};

/**
 * @note Transaction function prototypes are declared in individual module headers:
 * - bpio_1wire.h: 1-Wire protocol transactions
 * - bpio_2wire.h: 2-Wire protocol transactions
 * - bpio_3wire.h: 3-Wire SPI transactions
 * - bpio_i2c.h: I2C protocol transactions
 * - bpio_spi.h: SPI protocol transactions
 * - bpio_hiz.h: High-impedance mode
 * - bpio_dio.h: Digital I/O operations
 * - bpio_led.h: LED control
 * - bpio_infrared.h: IR transmit/receive
 */

#endif // BPIO_TRANSACTIONS_H

