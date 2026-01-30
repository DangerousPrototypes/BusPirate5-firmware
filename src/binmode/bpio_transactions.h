#ifndef BPIO_TRANSACTIONS_H
#define BPIO_TRANSACTIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "bpio_reader.h"

// Shared data structure used by all BPIO transaction handlers
struct bpio_data_request_t {
    bool debug;              // Debug flag
    bool start_main;         // Start main condition
    bool start_alt;          // Start alternate condition
    uint16_t bytes_write;    // Bytes to write
    uint16_t bytes_read;     // Bytes to read
    const char *data_buf;    // Data buffer 
    bool stop_main;          // Stop main condition  
    bool stop_alt;           // Stop alternate condition
    // Bitwise pin operations (2wire/3wire modes only)
    flatbuffers_uint8_vec_t bitwise_ops;  // Vector of BitwiseOps values
};

// All transaction function prototypes are now in individual module headers:
// - bpio_1wire.h, bpio_2wire.h, bpio_3wire.h
// - bpio_i2c.h, bpio_spi.h
// - bpio_hiz.h, bpio_dio.h
// - bpio_led.h, bpio_infrared.h

#endif // BPIO_TRANSACTIONS_H

