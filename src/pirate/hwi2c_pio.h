/**
 * @file hwi2c_pio.h
 * @brief I2C protocol implementation using PIO.
 * @details PIO-based I2C master with clock stretching support and timeout handling.
 *          Heavily modified from Raspberry Pi Pico SDK PIO I2C example.
 * @copyright Copyright (c) 2021 Raspberry Pi (Trading) Ltd. (BSD-3-Clause)
 * @note Modified by Bus Pirate project 2022-2024 (Ian Lesnet)
 */

/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PIO_I2C_H
#define _PIO_I2C_H

#include "hwi2c.pio.h"

/**
 * @brief I2C transaction result codes.
 */
typedef enum {
    HWI2C_OK = 0,       ///< Transaction successful
    HWI2C_NACK = 1,     ///< NACK received (address or data not acknowledged)
    HWI2C_TIMEOUT = 2   ///< Timeout waiting for bus or slave
} hwi2c_status_t;

/**
 * @brief Initialize I2C PIO and state machine.
 * @param sda            GPIO pin for I2C SDA (data)
 * @param scl            GPIO pin for I2C SCL (clock)
 * @param dir_sda        GPIO pin for SDA buffer direction
 * @param dir_scl        GPIO pin for SCL buffer direction
 * @param baudrate       I2C clock frequency in Hz
 * @param clock_stretch  true to enable clock stretching support
 */
void pio_i2c_init(uint sda, uint scl, uint dir_sda, uint dir_scl, uint baudrate, bool clock_stretch);

/**
 * @brief Clean up and remove I2C PIO program.
 */
void pio_i2c_cleanup(void);

/**
 * @name High-level I2C transaction functions
 * @details These functions handle addressing, START/STOP conditions, and provide
 *          user-friendly error messages.
 * @{
 */

/**
 * @brief Perform combined I2C write-then-read transaction.
 * @param addr        7-bit I2C device address
 * @param write_data  Pointer to write data buffer
 * @param write_len   Number of bytes to write
 * @param read_data   Pointer to read data buffer
 * @param read_len    Number of bytes to read
 * @return true on success, false on NACK or timeout
 */
bool i2c_transaction(uint8_t addr, uint8_t *write_data, uint8_t write_len, uint8_t *read_data, uint16_t read_len);

/**
 * @brief Write data to I2C device.
 * @param addr  7-bit I2C device address
 * @param data  Pointer to write data
 * @param len   Number of bytes to write
 * @return true on success, false on NACK or timeout
 */
bool i2c_write(uint8_t addr, uint8_t *data, uint16_t len);

/**
 * @brief Read data from I2C device.
 * @param addr  7-bit I2C device address
 * @param data  Pointer to receive buffer
 * @param len   Number of bytes to read
 * @return true on success, false on NACK or timeout
 */
bool i2c_read(uint8_t addr, uint8_t *data, uint16_t len);

/**
 * @brief Write to I2C device register.
 * @param addr      7-bit I2C device address
 * @param reg       Pointer to register address bytes
 * @param reg_len   Number of register address bytes
 * @param data      Pointer to write data
 * @param data_len  Number of data bytes to write
 * @return true on success, false on NACK or timeout
 */
bool i2c_write_reg(uint8_t addr, uint8_t *reg, uint8_t reg_len, const uint8_t *data, uint8_t data_len);

/**
 * @brief Read from I2C device register.
 * @param addr      7-bit I2C device address
 * @param reg       Pointer to register address bytes
 * @param reg_len   Number of register address bytes
 * @param data      Pointer to receive buffer
 * @param data_len  Number of data bytes to read
 * @return true on success, false on NACK or timeout
 */
bool i2c_read_reg(uint8_t addr, uint8_t *reg, uint8_t reg_len, uint8_t *data, uint8_t data_len);

/** @} */

/**
 * @name Low-level I2C functions with timeout
 * @details Primitive I2C operations with explicit timeout handling.
 * @{
 */

/**
 * @brief Issue I2C START condition.
 * @param timeout  Timeout in microseconds
 * @return HWI2C_OK or HWI2C_TIMEOUT
 */
hwi2c_status_t pio_i2c_start_timeout(uint32_t timeout);

/**
 * @brief Issue I2C STOP condition.
 * @param timeout  Timeout in microseconds
 * @return HWI2C_OK or HWI2C_TIMEOUT
 */
hwi2c_status_t pio_i2c_stop_timeout(uint32_t timeout);

/**
 * @brief Issue I2C RESTART condition.
 * @param timeout  Timeout in microseconds
 * @return HWI2C_OK or HWI2C_TIMEOUT
 */
hwi2c_status_t pio_i2c_restart_timeout(uint32_t timeout);

/**
 * @brief Write single byte to I2C bus.
 * @param out_data  Byte to transmit
 * @param timeout   Timeout in microseconds
 * @return HWI2C_OK, HWI2C_NACK, or HWI2C_TIMEOUT
 */
hwi2c_status_t pio_i2c_write_timeout(uint8_t out_data, uint32_t timeout);

/**
 * @brief Read single byte from I2C bus.
 * @param[out] in_data  Pointer to store received byte
 * @param ack           true to send ACK, false to send NACK
 * @param timeout       Timeout in microseconds
 * @return HWI2C_OK or HWI2C_TIMEOUT
 */
hwi2c_status_t pio_i2c_read_timeout(uint8_t* in_data, bool ack, uint32_t timeout);

/**
 * @brief Read array from I2C device.
 * @param addr    7-bit I2C device address
 * @param rxbuf   Pointer to receive buffer
 * @param len     Number of bytes to read
 * @param timeout Timeout in microseconds
 * @return HWI2C_OK, HWI2C_NACK, or HWI2C_TIMEOUT
 */
hwi2c_status_t pio_i2c_read_array_timeout(uint8_t addr, uint8_t* rxbuf, uint len, uint32_t timeout);

/**
 * @brief Write array to I2C device.
 * @param addr    7-bit I2C device address
 * @param txbuf   Pointer to write buffer
 * @param len     Number of bytes to write
 * @param timeout Timeout in microseconds
 * @return HWI2C_OK, HWI2C_NACK, or HWI2C_TIMEOUT
 */
hwi2c_status_t pio_i2c_write_array_timeout(uint8_t addr, uint8_t* txbuf, uint len, uint32_t timeout);

/**
 * @brief Perform combined write-then-read transaction.
 * @param addr    7-bit I2C device address
 * @param txbuf   Pointer to write buffer
 * @param txlen   Number of bytes to write
 * @param rxbuf   Pointer to receive buffer
 * @param rxlen   Number of bytes to read
 * @param timeout Timeout in microseconds
 * @return HWI2C_OK, HWI2C_NACK, or HWI2C_TIMEOUT
 */
hwi2c_status_t pio_i2c_transaction_array_timeout(
    uint8_t addr, uint8_t* txbuf, uint txlen, uint8_t* rxbuf, uint rxlen, uint32_t timeout);
hwi2c_status_t pio_i2c_transaction_array_repeat_start(uint8_t addr, uint8_t* txbuf, uint txlen, uint8_t* rxbuf, uint rxlen, uint32_t timeout);
hwi2c_status_t pio_i2c_wait_idle_extern(uint32_t timeout) ;
//hwi2c_status_t pio_i2c_transaction_bpio(uint8_t* txbuf, uint txlen, uint8_t* rxbuf, uint rxlen, uint32_t timeout);
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

/** @} */

#endif