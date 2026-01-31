/**
 * @file bpio_uart.h
 * @brief BPIO UART protocol transaction handler.
 * @details Provides binary protocol UART transaction and async handling.
 */

#ifndef BPIO_UART_H
#define BPIO_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "bpio_reader.h"

// Forward declaration of the request structure (defined in bpio_transactions.h)
struct bpio_data_request_t;

/**
 * @brief UART transaction handler for BPIO.
 * @param request     Transaction request structure
 * @param data_write  Data to transmit
 * @param data_read   Buffer for received data
 * @return            Number of bytes received
 */
uint32_t bpio_hwuart_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);

/**
 * @brief UART async handler for BPIO (unsolicited incoming data).
 * @param data_read  Buffer for received data
 * @return           Number of bytes received
 */
uint32_t bpio_hwuart_async_handler(uint8_t *data_read);

#endif // BPIO_UART_H
