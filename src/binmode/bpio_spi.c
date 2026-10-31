#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "bpio_spi.h"
#include "bpio_reader.h"
#include "bpio_transactions.h"
#include "hardware/spi.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "pirate/hwspi.h"

uint32_t bpio_hwspi_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read) {
    if(request->debug) printf("[SPI] Performing transaction\r\n");

    uint8_t *read_ptr = data_read; 

    if(request->start_main||request->start_alt) {
        // CS active
        if(request->debug) printf("[SPI] CS active\r\n");
        spi_set_cs(M_SPI_SELECT);
    }

    if(request->bytes_write > 0) {
        if(request->debug) printf("[SPI] Writing %d bytes\r\n", request->bytes_write);
        // write data
        if(!request->start_alt){
            for(uint32_t i = 0; i < request->bytes_write; i++) {
                while (!spi_is_writable(M_SPI_PORT)) {
                    tight_loop_contents();
                }
                spi_get_hw(M_SPI_PORT)->dr = flatbuffers_uint8_vec_at(data_write, i);
            }
            while (spi_is_busy(M_SPI_PORT)) {
                tight_loop_contents();
            }
            while (spi_is_readable(M_SPI_PORT)) {
                uint8_t rx_byte = (uint8_t)spi_get_hw(M_SPI_PORT)->dr;
            }
        }else{
            for(uint32_t i = 0; i < request->bytes_write; i++) {
                while (!spi_is_writable(M_SPI_PORT)) {
                    tight_loop_contents();
                }
                spi_get_hw(M_SPI_PORT)->dr = flatbuffers_uint8_vec_at(data_write, i);
                while (spi_is_busy(M_SPI_PORT)) {
                    tight_loop_contents();
                }
                while (!spi_is_readable(M_SPI_PORT)){
                    tight_loop_contents();
                }
                uint8_t rx_byte = (uint8_t)spi_get_hw(M_SPI_PORT)->dr;
                *read_ptr++ = rx_byte;  // Store byte and increment pointer                               
            }            

        }           
    }    

    if(request->bytes_read > 0) {
        if(request->debug) printf("[SPI] Reading %d bytes\r\n", request->bytes_read);
        // read data
        for(uint32_t i = 0; i < request->bytes_read; i++) {
            *read_ptr++ = (uint8_t)hwspi_write_read(0xFF);  // Store and increment
        } 
    }

    if(request->stop_main || request->stop_alt) {
        // CS inactive
        if(request->debug) printf("[SPI] CS inactive\r\n");
        spi_set_cs(M_SPI_DESELECT);
    }

    return false;
}