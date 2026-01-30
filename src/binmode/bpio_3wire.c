#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bytecode.h"
#include "bpio_reader.h"
#include "bpio_3wire.h"
#include "bpio_transactions.h"
#include "pirate/hw3wire_pio.h"
#include "pirate/bio.h"
#include "ui/ui_format.h"
#include "system_config.h"

// Forward declare hw3wire_set_cs since we can't include mode/hw3wire.h
// (would create circular dependency)
extern void hw3wire_set_cs(uint8_t cs);

uint32_t bpio_hw3wire_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read) {
    if(request->debug) printf("[3WIRE] Performing transaction\r\n");

    uint8_t *read_ptr = data_read; 

    // Start condition - CS active
    if(request->start_main||request->start_alt) {
        if(request->debug) printf("[3WIRE] CS active\r\n");
        hw3wire_set_cs(M_3WIRE_SELECT);
    }

    // Write data
    if(request->bytes_write > 0) {
        if(request->debug) printf("[3WIRE] Writing %d bytes\r\n", request->bytes_write);
        
        // Check if start_alt flag is set - this means read-with-write mode
        if(!request->start_alt){
            // Normal write mode - just write the data
            for(uint32_t i = 0; i < request->bytes_write; i++) {
                uint32_t temp = flatbuffers_uint8_vec_at(data_write, i);
                ui_format_bitorder_manual(&temp, 8, system_config.bit_order);
                pio_hw3wire_get16((uint8_t*)&temp);  // Sends and receives
            }
        } else {
            // Read-with-write mode - capture the read data during write
            if(request->debug) printf("[3WIRE] Read-with-write mode\r\n");
            for(uint32_t i = 0; i < request->bytes_write; i++) {
                uint32_t temp = flatbuffers_uint8_vec_at(data_write, i);
                ui_format_bitorder_manual(&temp, 8, system_config.bit_order);
                pio_hw3wire_get16((uint8_t*)&temp);
                uint32_t read_temp = temp;
                ui_format_bitorder_manual(&read_temp, 8, system_config.bit_order);
                *read_ptr++ = (uint8_t)read_temp;  // Store byte and increment pointer
            }
        }           
    }    

    // Read data (additional pure read phase)
    if(request->bytes_read > 0) {
        if(request->debug) printf("[3WIRE] Reading %d bytes\r\n", request->bytes_read);
        for(uint32_t i = 0; i < request->bytes_read; i++) {
            uint8_t temp8 = 0xff;  // Send 0xFF to clock out data
            pio_hw3wire_get16(&temp8);
            uint32_t temp = temp8;
            ui_format_bitorder_manual(&temp, 8, system_config.bit_order);
            *read_ptr++ = (uint8_t)temp;  // Store and increment
        } 
    }

    // Stop condition - CS inactive
    if(request->stop_main || request->stop_alt) {
        if(request->debug) printf("[3WIRE] CS inactive\r\n");
        hw3wire_set_cs(M_3WIRE_DESELECT);
    }

    // Bitwise pin operations
    if(request->bitwise_ops) {
        uint32_t bitwise_ops_len = flatbuffers_uint8_vec_len(request->bitwise_ops);
        if(bitwise_ops_len > 0) {
            if(request->debug) printf("[3WIRE] Processing %d bitwise operations\r\n", bitwise_ops_len);
            
            for(uint32_t i = 0; i < bitwise_ops_len; i++) {
                uint8_t op = flatbuffers_uint8_vec_at(request->bitwise_ops, i);
                
                if(request->debug) printf("[3WIRE] Bitwise op[%d]: 0x%02X\r\n", i, op);
                
                uint8_t pin_mask = 0;
                uint8_t pin_value = 0;
                
                // Data pin (MOSI) operations (bits 0-1)
                // 3wire: bit 0 = MOSI
                if(op & 0x01) { pin_mask |= 0x01; pin_value &= ~0x01; }  // DATA_LOW
                if(op & 0x02) { pin_mask |= 0x01; pin_value |= 0x01; }   // DATA_HIGH
                
                // Clock pin (SCLK) operations (bits 2-3)
                // 3wire: bit 1 = SCLK
                if(op & 0x04) { pin_mask |= 0x02; pin_value &= ~0x02; }  // CLOCK_LOW
                if(op & 0x08) { pin_mask |= 0x02; pin_value |= 0x02; }   // CLOCK_HIGH
                
                // Special case: CLOCK_PULSE (0x0C = both clock bits set)
                if((op & 0x0C) == 0x0C) {
                    if(request->debug) printf("[3WIRE] Clock pulse\r\n");
                    pio_hw3wire_clock_tick();
                } else if(pin_mask) {
                    // Set pins according to mask and value
                    if(request->debug) printf("[3WIRE] Set mask 0x%02X value 0x%02X\r\n", pin_mask, pin_value);
                    pio_hw3wire_set_mask(pin_mask, pin_value);
                }
                
                // Read operation (bit 4) - read MISO pin
                if(op & 0x10) {
                    if(request->debug) printf("[3WIRE] Read bit\r\n");
                    *read_ptr++ = bio_get(M_3WIRE_MISO);
                }
            }
        }
    }

    return false;
}