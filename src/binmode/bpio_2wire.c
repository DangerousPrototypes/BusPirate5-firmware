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
        if(request->debug) printf("[2WIRE] STOP\r\n");
        pio_hw2wire_stop();
    }

    if(request->stop_alt) {
        if(request->debug) printf("[2WIRE] RST low\r\n");
        bio_output(M_2WIRE_RST);
    }

    // Bitwise pin operations
    if(request->bitwise_ops) {
        uint32_t bitwise_ops_len = flatbuffers_uint8_vec_len(request->bitwise_ops);
        if(bitwise_ops_len > 0) {
            if(request->debug) printf("[2WIRE] Processing %d bitwise operations\r\n", bitwise_ops_len);
            
            for(uint32_t i = 0; i < bitwise_ops_len; i++) {
                uint8_t op = flatbuffers_uint8_vec_at(request->bitwise_ops, i);
                
                if(request->debug) printf("[2WIRE] Bitwise op[%d]: 0x%02X\r\n", i, op);
                
                uint8_t pin_mask = 0;
                uint8_t pin_value = 0;
                
                // Data pin (SDA) operations (bits 0-1)
                // 2wire: bit 0 = SDA
                if(op & 0x01) { pin_mask |= 0x01; pin_value &= ~0x01; }  // DATA_LOW
                if(op & 0x02) { pin_mask |= 0x01; pin_value |= 0x01; }   // DATA_HIGH
                
                // Clock pin (SCL) operations (bits 2-3)
                // 2wire: bit 1 = SCL
                if(op & 0x04) { pin_mask |= 0x02; pin_value &= ~0x02; }  // CLOCK_LOW
                if(op & 0x08) { pin_mask |= 0x02; pin_value |= 0x02; }   // CLOCK_HIGH
                
                // Special case: CLOCK_PULSE (0x0C = both clock bits set)
                if((op & 0x0C) == 0x0C) {
                    if(request->debug) printf("[2WIRE] Clock pulse\r\n");
                    pio_hw2wire_clock_tick();
                } else if(pin_mask) {
                    // Set pins according to mask and value
                    if(request->debug) printf("[2WIRE] Set mask 0x%02X value 0x%02X\r\n", pin_mask, pin_value);
                    pio_hw2wire_set_mask(pin_mask, pin_value);
                }
                
                // Read operation (bit 4)
                if(op & 0x10) {
                    if(request->debug) printf("[2WIRE] Read bit\r\n");
                    *read_ptr++ = bio_get(M_2WIRE_SDA);
                }
            }
        }
    }

    return false;
}