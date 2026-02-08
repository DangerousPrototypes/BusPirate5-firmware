/**
 * @file hw3wire_pio.h
 * @brief 3-Wire (SPI) protocol implementation using PIO.
 * @details PIO-based SPI master interface with bit-banging capabilities.
 * @copyright Copyright (c) 2021 Raspberry Pi (Trading) Ltd. (BSD-3-Clause)
 */

/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PIO_I2C_H
#define _PIO_I2C_H

/**
 * @brief Initialize 3-Wire (SPI) PIO and state machine.
 * @param mosi  GPIO pin for SPI MOSI (master out, slave in)
 * @param sclk  GPIO pin for SPI clock
 * @param miso  GPIO pin for SPI MISO (master in, slave out)
 * @param freq  SPI clock frequency in Hz
 */
void pio_hw3wire_init(uint mosi, uint sclk, uint miso, uint32_t freq);

/**
 * @brief Clean up and remove 3-Wire PIO program.
 */
void pio_hw3wire_cleanup(void);

/**
 * @brief Transmit 8-bit data on SPI bus.
 * @param data  Data byte to transmit (lower 8 bits)
 */
void pio_hw3wire_put16(uint16_t data);

/**
 * @brief Receive 8-bit data from SPI bus.
 * @param[out] data  Pointer to store received byte
 */
void pio_hw3wire_get16(uint8_t* data);

/**
 * @brief Issue SPI bus reset sequence.
 */
void pio_hw3wire_reset(void);

/**
 * @brief Generate single SPI clock tick.
 */
void pio_hw3wire_clock_tick(void);

/**
 * @brief Set pin states with mask.
 * @param pin_mask   Mask of pins to modify
 * @param pin_value  Values to set for masked pins
 */
void pio_hw3wire_set_mask(uint32_t pin_mask, uint32_t pin_value);

/**
 * @brief Initiate SPI transfer (CS assert).
 */
void pio_hw3wire_start(void);

/**
 * @brief End SPI transfer (CS deassert).
 */
void pio_hw3wire_stop(void);

/**
 * @brief Restart SPI transfer (CS pulse).
 */
void pio_hw3wire_restart(void);
#endif