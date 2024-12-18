/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PIO_I2C_H
#define _PIO_I2C_H

void pio_hw3wire_init(uint mosi, uint sclk, uint miso, uint32_t freq);
void pio_hw3wire_cleanup(void);

void pio_hw3wire_put16(uint16_t data);
void pio_hw3wire_get16(uint8_t* data);

void pio_hw3wire_reset(void);
void pio_hw3wire_clock_tick(void);
void pio_hw3wire_set_mask(uint32_t pin_mask, uint32_t pin_value);
void pio_hw3wire_start(void);
void pio_hw3wire_stop(void);
void pio_hw3wire_restart(void);
#endif