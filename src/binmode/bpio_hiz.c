#include <stdio.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bytecode.h"
#include "bpio_reader.h"
#include "bpio_hiz.h"
#include "bpio_transactions.h"

uint32_t bpio_hiz_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read) {
    if(request->debug) printf("[HIZ] No transaction support in HiZ mode\r\n");
    
    // Suppress unused parameter warnings
    (void)data_write;
    (void)data_read;
    
    return true;
}