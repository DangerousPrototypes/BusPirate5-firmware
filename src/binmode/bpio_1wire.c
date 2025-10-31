#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bytecode.h"
#include "command_struct.h"
#include "bpio_reader.h"
#include "bpio_1wire.h"
#include "bpio_transactions.h"
#include "mode/hw1wire.h"
#include "pirate/hw1wire_pio.h"

uint32_t bpio_hw1wire_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read) {
    if(request->debug) printf("[1WIRE] Performing transaction\r\n");

    if(request->start_main||request->start_alt) {
        if(request->debug) printf("[1WIRE] RESET\r\n");
        uint8_t device_detect = onewire_reset();
        if(!device_detect) {
            if(request->debug) printf("[1WIRE] No device detected\r\n");
            return true; 
        } else {
            if(request->debug) printf("[1WIRE] Device detected\r\n");
        }
    }

    if(request->bytes_write > 0) {
        if(request->debug) printf("[1WIRE] Writing %d bytes\r\n", request->bytes_write);
        for(uint32_t i = 0; i < request->bytes_write; i++) {
            onewire_tx_byte(flatbuffers_uint8_vec_at(data_write, i));
        }
        onewire_wait_for_idle(); // wait for the bus to be idle after writing
    }

    if(request->bytes_read > 0) {
        if(request->debug) printf("[1WIRE] Reading %d bytes\r\n", request->bytes_read);
        // read data
        for(uint32_t i = 0; i < request->bytes_read; i++) {
            data_read[i] = (uint8_t)onewire_rx_byte();
            onewire_wait_for_idle(); //temp test
        } 
    }

    return false;
}