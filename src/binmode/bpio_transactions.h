#ifndef BPIO_TRANSACTIONS_H
#define BPIO_TRANSACTIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "bpio_reader.h"


struct bpio_data_request_t {
    bool debug; // Debug flag
    bool start_main; // Start main condition
    bool start_alt;  // Start alternate condition
    uint16_t bytes_write; // Bytes to write
    uint16_t bytes_read; // Bytes to read
    const char *data_buf; // Data buffer 
    bool stop_main; // Stop main condition  
    bool stop_alt;  // Stop alternate condition
};

// Transaction function prototypes (now included from individual headers)
// Legacy function prototypes for transactions not yet split out
uint32_t bpio_led_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);
uint32_t bpio_infrared_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);

#endif // BPIO_TRANSACTIONS_H

