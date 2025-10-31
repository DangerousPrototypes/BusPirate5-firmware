#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bytecode.h"
#include "bpio_reader.h"
#include "command_struct.h"
#include "bpio_2wire.h"
#include "bpio_transactions.h"
#include "mode/hw2wire.h"
#include "pirate/hw2wire_pio.h"
#include "pirate/bio.h"
#include "ui/ui_format.h"
#include "system_config.h"

uint32_t bpio_hw2wire_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read) {
    if(request->debug) printf("[2WIRE] Performing transaction\r\n");

    uint8_t *read_ptr = data_read; 

    // Start main
    if(request->start_main||request->start_alt) {
        if(request->debug) printf("[2WIRE] START\r\n");
        pio_hw2wire_start();
    }

    //start alt
    if(request->start_alt) {
        if(request->debug) printf("[2WIRE] RST high\r\n");
        bio_input(M_2WIRE_RST);
    }

    //write data
    if(request->bytes_write > 0) {
        if(request->debug) printf("[2WIRE] Writing %d bytes\r\n", request->bytes_write);
        // write data
        for(uint32_t i = 0; i < request->bytes_write; i++) {
            uint32_t temp = flatbuffers_uint8_vec_at(data_write, i);
            ui_format_bitorder_manual(&temp, 8, system_config.bit_order);
            pio_hw2wire_put16(temp);            
        }   
    }    

    if(request->bytes_read > 0) {
        if(request->debug) printf("[2WIRE] Reading %d bytes\r\n", request->bytes_read);
        // read data
        for(uint32_t i = 0; i < request->bytes_read; i++) {
            uint8_t temp8;
            pio_hw2wire_get16(&temp8);
            uint32_t temp = temp8;
            ui_format_bitorder_manual(&temp, 8, system_config.bit_order);
            *read_ptr++ = (uint8_t)temp; // Store and increment
        } 
    }

    if(request->stop_main) {
        // CS inactive
        if(request->debug) printf("[2WIRE] STOP\r\n");
        pio_hw2wire_stop();
    }

    if(request->stop_alt) {
        // CS inactive
        if(request->debug) printf("[2WIRE] CS inactive (alt)\r\n");
        bio_output(M_2WIRE_RST);
    }

    return false;
}