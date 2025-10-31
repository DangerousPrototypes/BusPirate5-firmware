/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PIO_HW2WIRE_H
#define _PIO_HW2WIRE_H

#include "hw2wire.pio.h"

void pio_hw2wire_init(uint sda, uint scl, uint dir_sda, uint dir_scl, uint baudrate);
void pio_hw2wire_cleanup(void);

void pio_hw2wire_put16(uint16_t data);
void pio_hw2wire_get16(uint8_t* data);

void pio_hw2wire_reset(void);
void pio_hw2wire_clock_tick(void);
void pio_hw2wire_set_mask(uint32_t pin_mask, uint32_t pin_value);
void pio_hw2wire_start(void);
void pio_hw2wire_stop(void);
void pio_hw2wire_restart(void);
#endif