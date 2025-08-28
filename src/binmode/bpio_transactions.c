#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate.h"
#include "command_struct.h"
#include "bytecode.h"
#include "pirate/hwi2c_pio.h"
#include "mode/hwi2c.h"
#include "hardware/spi.h"
#include "mode/hwspi.h"
#include "pirate/hwspi.h"
#include "mode/hw1wire.h"
#include "pirate/hw1wire_pio.h"
#include "bpio_reader.h"  
#include "bpio_transactions.h"

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
            //onewire_tx_byte((uint8_t)request->data_buf[i]);
            onewire_tx_byte(flatbuffers_uint8_vec_at(data_write, i));
        }
        onewire_wait_for_idle(); // wait for the bus to be idle after writing
    }

    if(request->bytes_read > 0) {
        if(request->debug) printf("[1WIRE] Reading %d bytes\r\n", request->bytes_read);
        // read data
        //uint8_t *data_buf = (uint8_t *)request->data_buf;
        for(uint32_t i = 0; i < request->bytes_read; i++) {
            //data_buf[i] = onewire_rx_byte();
            data_read[i] = (uint8_t)onewire_rx_byte();
            onewire_wait_for_idle(); //temp test
        } 
    }

    return false;
}

uint32_t bpio_hwi2c_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read) {
    if(request->debug) printf("[I2C] Performing transaction\r\n");
    hwi2c_status_t i2c_result = HWI2C_OK;
    const uint32_t timeout = 0xfffff; // Default timeout, can be adjusted
    #if 0
    if(request->bytes_write == 0) {
        if(request->debug) printf("[I2C] Missing I2C read/address in write byte 0\r\n");
        return true; // Nothing to do
    }
    #endif
    if(request->debug) printf("[I2C] START\r\n");

    if(request->start_main||request->start_alt) {
        if(pio_i2c_start_timeout(timeout)) return HWI2C_TIMEOUT;
    }

    if(request->bytes_write > 1 || (request->bytes_read == 0 && request->bytes_write > 0)) {
        if(request->debug) printf("[I2C] Writing %d bytes\r\n", request->bytes_write);
        //write data
        for(uint32_t i = 0; i < request->bytes_write; i++) {
            // send txbuf[0] (i2c address) with the last bit low
            if(i ==0 && (request->start_main||request->start_alt)) {
                if(request->debug) printf("[I2C] Write address 0x%02X\r\n", request->data_buf[0]&~0b1);
                // if we have a start condition, we need to write the address with the last bit low
                //i2c_result = pio_i2c_write_timeout(request->data_buf[0]&~0b1, timeout);
                i2c_result = pio_i2c_write_timeout(flatbuffers_uint8_vec_at(data_write, 0)&~0b1, timeout);
                if(i2c_result != HWI2C_OK) return i2c_result;
            }else{
                // if we have no start condition, we write the data as is
                //i2c_result = pio_i2c_write_timeout(request->data_buf[i], timeout);
                i2c_result = pio_i2c_write_timeout(flatbuffers_uint8_vec_at(data_write, i), timeout);
                if(i2c_result != HWI2C_OK) return i2c_result;
            }
        }

        // if we have no read data, we can stop here
        if(request->bytes_read == 0) {
            goto i2c_bpio_cleanup;
        }
        if(request->debug) printf("[I2C] RESTART\r\n");
        // if we have read data, we need to restart
        if(pio_i2c_restart_timeout(timeout)) return HWI2C_TIMEOUT;

    }

    if(request->start_main || request->start_alt){
        if(request->debug) printf("[I2C] Read address 0x%02X\r\n", request->data_buf[0]|0b1);
        //send the read address with the last bit high
        //i2c_result = pio_i2c_write_timeout(request->data_buf[0]|0b1, timeout);
        i2c_result = pio_i2c_write_timeout(flatbuffers_uint8_vec_at(data_write, 0)|0b1, timeout);
        if(i2c_result != HWI2C_OK) return i2c_result;
    }
    

    if(request->debug) printf("[I2C] Reading %d bytes\r\n", request->bytes_read);
    // read data
    // only nack the last byte if we have a stop condition
    bool ack_all = !(request->stop_main || request->stop_alt);
    for(uint32_t i = 0; i<request->bytes_read; i++) {
        //i2c_result = pio_i2c_read_timeout((uint8_t*)&request->data_buf[i], ack_all || (i<(request->bytes_read-1)), timeout);
        i2c_result = pio_i2c_read_timeout((uint8_t*)&data_read[i], ack_all || (i<(request->bytes_read-1)), timeout);
        if(i2c_result != HWI2C_OK) return i2c_result;
    }

    // stop and wait for PIO to be idle
i2c_bpio_cleanup:
    if(request->stop_main || request->stop_alt) {
        if(request->debug) printf("[I2C] STOP\r\n");
        if (pio_i2c_stop_timeout(timeout)) return HWI2C_TIMEOUT;
    }
    // Wait for PIO to be idle
    //if (pio_i2c_wait_idle_extern(timeout)) return HWI2C_TIMEOUT;
    return HWI2C_OK;
}

uint32_t bpio_hwspi_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read) {
    if(request->debug) printf("[SPI] Performing transaction\r\n");

    uint8_t *read_ptr = data_read; 

    if(request->start_main||request->start_alt) {
        // CS active
        if(request->debug) printf("[SPI] CS active\r\n");
        spi_set_cs(M_SPI_SELECT);
    }

    //utilize the SPI buffer
    if(request->bytes_write > 0) {
        if(request->debug) printf("[SPI] Writing %d bytes\r\n", request->bytes_write);
        // write data
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
            if(request->start_alt) {
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