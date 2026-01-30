#ifndef BPIO_LED_H
#define BPIO_LED_H

#include <stdint.h>
#include <stdbool.h>
#include "bpio_reader.h"

// Forward declaration
struct bpio_data_request_t;

/**
 * Configure LED mode for BPIO
 * @param bpio_mode_config Configuration structure
 * @return true on success, false on failure
 */
bool bpio_led_configure(bpio_mode_configuration_t *bpio_mode_config);

/**
 * Handle LED transaction
 * @param request Data request structure
 * @param data_write Data to write
 * @param data_read Buffer for read data
 * @return Number of bytes read
 */
uint32_t bpio_led_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);

#endif // BPIO_LED_H
