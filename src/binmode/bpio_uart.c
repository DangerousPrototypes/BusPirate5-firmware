#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "pirate.h"
#include "pirate/hwuart_pio.h"
#include "pirate/bio.h"
#include "bpio_uart.h"
#include "bpio_reader.h"
#include "bpio_transactions.h"

// Maximum bytes to read in async handler
#define BPIO_MAX_READ_SIZE 512

// UART transaction handler for BPIO
// Handles transmit and/or receive operations in response to DataRequest
uint32_t bpio_hwuart_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read) {
    if(request->debug) printf("[UART] Performing transaction\r\n");

    // Transmit bytes if requested
    if(request->bytes_write > 0) {
        if(request->debug) printf("[UART] Writing %d bytes\r\n", request->bytes_write);
        for(uint32_t i = 0; i < request->bytes_write; i++) {
            //uart_putc_raw(M_UART_PORT, flatbuffers_uint8_vec_at(data_write, i));
            uart_putc_raw(M_UART_PORT, 'G');
        }
    }

    // Receive bytes if requested
    if(request->bytes_read > 0) {
        if(request->debug) printf("[UART] Reading %d bytes\r\n", request->bytes_read);
        for(uint32_t i = 0; i < request->bytes_read; i++) {
            // Wait for data with timeout (avoid blocking forever)
            if(!uart_is_readable_within_us(M_UART_PORT, 1000000)) { // 1 second timeout
                if(request->debug) printf("[UART] Read timeout at byte %d\r\n", i);
                return true; // Error - timeout
            }
            data_read[i] = (uint8_t)uart_getc(M_UART_PORT);
        }
    }

    return false; // Success
}

// UART async handler for BPIO
// Checks for unsolicited incoming UART data and reads it into buffer
// Returns number of bytes read (0 if no data available)
//TODO: UART is dispatched to multiple processes!!!
uint32_t bpio_hwuart_async_handler(uint8_t *data_read) {
    // Check if UART has data available
    if(!uart_is_readable(M_UART_PORT)) {
        return 0;
    }
    
    // Read first byte
    data_read[0] = (uint8_t)uart_getc(M_UART_PORT);
    uint32_t bytes_read = 1;
    
    // Wait up to 200us for more data to arrive and batch into single packet
    // Returns immediately if data arrives sooner, improving responsiveness
    // At 115200 baud: ~87us per byte, so typically captures 2-3 bytes
    // At 9600 baud: ~1042us per byte, so may not capture additional bytes
    // Only wait if FIFO empty - then batch whatever arrives
    if(!uart_is_readable(M_UART_PORT)) {
        uart_is_readable_within_us(M_UART_PORT, 200);
    }

    // Drain all available data
    while(uart_is_readable(M_UART_PORT) && bytes_read < BPIO_MAX_READ_SIZE) {
        data_read[bytes_read++] = (uint8_t)uart_getc(M_UART_PORT);
    }
    
    return bytes_read;
}
