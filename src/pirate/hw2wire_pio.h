/**
 * @file hw2wire_pio.h
 * @brief 2-Wire (I2C) protocol implementation using PIO.
 * @details PIO-based I2C master interface with bit-banging capabilities.
 * @copyright Copyright (c) 2021 Raspberry Pi (Trading) Ltd. (BSD-3-Clause)
 */

/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PIO_HW2WIRE_H
#define _PIO_HW2WIRE_H

#include "hw2wire.pio.h"

/**
 * @brief Initialize 2-Wire (I2C) PIO and state machine.
 * @param sda      GPIO pin for I2C SDA (data)
 * @param scl      GPIO pin for I2C SCL (clock)
 * @param dir_sda  GPIO pin for SDA buffer direction
 * @param dir_scl  GPIO pin for SCL buffer direction
 * @param baudrate I2C clock frequency in Hz
 */
void pio_hw2wire_init(uint sda, uint scl, uint dir_sda, uint dir_scl, uint baudrate);

/**
 * @brief Clean up and remove 2-Wire PIO program.
 */
void pio_hw2wire_cleanup(void);

/**
 * @brief Transmit 8-bit data on I2C bus.
 * @param data  Data byte to transmit (lower 8 bits)
 */
void pio_hw2wire_put16(uint16_t data);

/**
 * @brief Receive 8-bit data from I2C bus.
 * @param[out] data  Pointer to store received byte
 */
void pio_hw2wire_get16(uint8_t* data);

/**
 * @brief Issue I2C bus reset sequence.
 */
void pio_hw2wire_reset(void);

/**
 * @brief Generate single I2C clock tick.
 */
void pio_hw2wire_clock_tick(void);

/**
 * @brief Set pin states with mask.
 * @param pin_mask   Mask of pins to modify
 * @param pin_value  Values to set for masked pins
 */
void pio_hw2wire_set_mask(uint32_t pin_mask, uint32_t pin_value);

/**
 * @brief Issue I2C START condition.
 */
void pio_hw2wire_start(void);

/**
 * @brief Issue I2C STOP condition.
 */
void pio_hw2wire_stop(void);

/**
 * @brief Issue I2C RESTART (repeated START) condition.
 */
void pio_hw2wire_restart(void);
#endif