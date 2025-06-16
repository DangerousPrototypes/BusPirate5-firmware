/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PIO_I2C_H
#define _PIO_I2C_H

#include "hwi2c.pio.h"

typedef enum {
    HWI2C_OK = 0,
    HWI2C_NACK = 1,
    HWI2C_TIMEOUT = 2
} hwi2c_status_t;

void pio_i2c_init(uint sda, uint scl, uint dir_sda, uint dir_scl, uint baudrate, bool clock_stretch);
void pio_i2c_cleanup(void);

// ---------------------------------------------------------------
// Functions with timeout
hwi2c_status_t pio_i2c_start_timeout(uint32_t timeout);
hwi2c_status_t pio_i2c_stop_timeout(uint32_t timeout);
hwi2c_status_t pio_i2c_restart_timeout(uint32_t timeout);
hwi2c_status_t pio_i2c_write_timeout(uint8_t out_data, uint32_t timeout);
hwi2c_status_t pio_i2c_read_timeout(uint8_t* in_data, bool ack, uint32_t timeout);
hwi2c_status_t pio_i2c_read_array_timeout(uint8_t addr, uint8_t* rxbuf, uint len, uint32_t timeout);
hwi2c_status_t pio_i2c_write_array_timeout(uint8_t addr, uint8_t* txbuf, uint len, uint32_t timeout);
hwi2c_status_t pio_i2c_transaction_array_timeout(
    uint8_t addr, uint8_t* txbuf, uint txlen, uint8_t* rxbuf, uint rxlen, uint32_t timeout);
hwi2c_status_t pio_i2c_transaction_array_repeat_start(uint8_t addr, uint8_t* txbuf, uint txlen, uint8_t* rxbuf, uint rxlen, uint32_t timeout);

// ----------------------------------------------------------------------------
// Low-level functions

//void pio_i2c_start(void);
//void pio_i2c_stop(void);
//void pio_i2c_repstart(void);

//bool pio_i2c_check_error(void);
void pio_i2c_resume_after_error(void);

// If I2C is ok, block and push data. Otherwise fall straight through.
//void pio_i2c_put_or_err(uint16_t data);
//uint8_t pio_i2c_get(void);

// ----------------------------------------------------------------------------
// Transaction-level functions

//int pio_i2c_write_blocking(uint8_t addr, uint8_t* txbuf, uint len);
//int pio_i2c_read_blocking(uint8_t addr, uint8_t* rxbuf, uint len);

// exposed
//void pio_i2c_rx_enable(bool en);
//void pio_i2c_wait_idle(void);
//void pio_i2c_put16(uint16_t data);

#endif