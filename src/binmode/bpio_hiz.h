#ifndef BPIO_HIZ_H
#define BPIO_HIZ_H

#include <stdint.h>
#include <stdbool.h>
#include "bpio_reader.h"

// Forward declaration of the request structure (defined in bpio_transactions.h)
struct bpio_data_request_t;

/**
 * @brief Perform a HIZ (High Impedance) transaction
 * @param request Transaction request structure containing debug flags, byte counts, and control flags
 * @param data_write Flatbuffer vector containing data to write
 * @param data_read Buffer to store read data
 * @return Always returns error (true) as HIZ mode doesn't support transactions
 */
uint32_t bpio_hiz_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);

#endif // BPIO_HIZ_H