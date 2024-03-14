/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PIO_I2C_H
#define _PIO_I2C_H

#include "hw2wire.pio.h"

// ---------------------------------------------------------------
// Functions with timeout
uint32_t pio_hw2wire_start_timeout(PIO pio, uint sm, uint32_t timeout);
uint32_t pio_hw2wire_stop_timeout(PIO pio, uint sm, uint32_t timeout);
uint32_t pio_hw2wire_restart_timeout(PIO pio, uint sm, uint32_t timeout);
uint32_t pio_hw2wire_write_timeout(PIO pio, uint sm, uint32_t data, uint32_t timeout);
uint32_t pio_hw2wire_read_timeout(PIO pio, uint sm, uint32_t *data, bool ack, uint32_t timeout);
uint32_t pio_hw2wire_read_blocking_timeout(PIO pio, uint sm, uint8_t addr, uint8_t *rxbuf, uint len, uint32_t timeout);
uint32_t pio_hw2wire_write_blocking_timeout(PIO pio, uint sm, uint8_t addr, uint8_t *txbuf, uint len, uint32_t timeout);
uint32_t pio_hw2wire_transaction_blocking_timeout(PIO pio, uint sm, uint8_t addr, uint8_t *txbuf, uint txlen, uint8_t *rxbuf, uint rxlen, uint32_t timeout);
// ----------------------------------------------------------------------------
// Low-level functions
void pio_hw2wire_reset(PIO pio, uint sm);
void pio_hw2wire_clock_tick(PIO pio, uint sm);
void pio_hw2wire_start(PIO pio, uint sm);
void pio_hw2wire_stop(PIO pio, uint sm);
void pio_hw2wire_repstart(PIO pio, uint sm);

bool pio_hw2wire_check_error(PIO pio, uint sm);
void pio_hw2wire_resume_after_error(PIO pio, uint sm);

// If I2C is ok, block and push data. Otherwise fall straight through.
void pio_hw2wire_put_or_err(PIO pio, uint sm, uint16_t data);
uint8_t pio_hw2wire_get(PIO pio, uint sm);

// ----------------------------------------------------------------------------
// Transaction-level functions

int pio_hw2wire_write_blocking(PIO pio, uint sm, uint8_t addr, uint8_t *txbuf, uint len);
int pio_hw2wire_read_blocking(PIO pio, uint sm, uint8_t addr, uint8_t *rxbuf, uint len);


//exposed
void pio_hw2wire_rx_enable(PIO pio, uint sm, bool en);
void pio_hw2wire_wait_idle(PIO pio, uint sm);
void pio_hw2wire_put16(PIO pio, uint sm, uint16_t data);
void pio_hw2wire_get16(PIO pio, uint sm, uint32_t *data) ;

#endif