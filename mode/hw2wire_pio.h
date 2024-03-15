/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PIO_I2C_H
#define _PIO_I2C_H

#include "hw2wire.pio.h"

// Low-level functions
void pio_hw2wire_reset(PIO pio, uint sm);
void pio_hw2wire_clock_tick(PIO pio, uint sm);
void pio_hw2wire_start(PIO pio, uint sm);
void pio_hw2wire_stop(PIO pio, uint sm);
void pio_hw2wire_repstart(PIO pio, uint sm);
//exposed
uint8_t pio_hw2wire_get(PIO pio, uint sm);
void pio_hw2wire_rx_enable(PIO pio, uint sm, bool en);
static void pio_hw2wire_wait_idle(PIO pio, uint sm);
void pio_hw2wire_put16(PIO pio, uint sm, uint16_t data);
void pio_hw2wire_get16(PIO pio, uint sm, uint32_t *data) ;

#endif