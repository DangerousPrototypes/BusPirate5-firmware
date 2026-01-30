#ifndef BPIO_UART_H
#define BPIO_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "bpio_reader.h"

// Forward declaration of the request structure (defined in bpio_transactions.h)
struct bpio_data_request_t;

// UART transaction handler for BPIO (transmit/receive in response to request)
uint32_t bpio_hwuart_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);

// UART async handler for BPIO (unsolicited incoming data)
uint32_t bpio_hwuart_async_handler(uint8_t *data_read);

#endif // BPIO_UART_H
