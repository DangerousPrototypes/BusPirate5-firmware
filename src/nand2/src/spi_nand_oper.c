/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: Adapted for Raspberry Pi Pico (RP2040)
 */

#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "../include/spi_nand_oper.h"

// Select the chip (active low)
static inline void cs_select(spi_nand_flash_device_t *handle)
{
    gpio_put(handle->config.cs_pin, 0);
}

// Deselect the chip (active low)
static inline void cs_deselect(spi_nand_flash_device_t *handle)
{
    gpio_put(handle->config.cs_pin, 1);
}

esp_err_t spi_nand_execute_transaction(spi_nand_flash_device_t *handle, spi_nand_transaction_t *transaction)
{
    spi_inst_t *spi = handle->config.spi;
    uint8_t tx_buf[16];
    size_t tx_idx = 0;

    // Build the command + address packet
    tx_buf[tx_idx++] = transaction->command;

    // Add address bytes (big-endian)
    if (transaction->address_bytes == 1) {
        tx_buf[tx_idx++] = transaction->address & 0xFF;
    } else if (transaction->address_bytes == 2) {
        tx_buf[tx_idx++] = (transaction->address >> 8) & 0xFF;
        tx_buf[tx_idx++] = transaction->address & 0xFF;
    } else if (transaction->address_bytes == 3) {
        tx_buf[tx_idx++] = (transaction->address >> 16) & 0xFF;
        tx_buf[tx_idx++] = (transaction->address >> 8) & 0xFF;
        tx_buf[tx_idx++] = transaction->address & 0xFF;
    }

    // Add dummy bytes if needed
    uint32_t dummy_bytes = transaction->dummy_bits / 8;
    for (uint32_t i = 0; i < dummy_bytes; i++) {
        tx_buf[tx_idx++] = 0x00;
    }

    cs_select(handle);

    // Send command + address + dummy bytes
    spi_write_blocking(spi, tx_buf, tx_idx);

    // Handle data phase
    if (transaction->mosi_len > 0 && transaction->mosi_data != NULL) {
        // Write data
        spi_write_blocking(spi, transaction->mosi_data, transaction->mosi_len);
    }

    if (transaction->miso_len > 0 && transaction->miso_data != NULL) {
        // Read data
        spi_read_blocking(spi, 0xff, transaction->miso_data, transaction->miso_len);
    }

    cs_deselect(handle);

    return ESP_OK;
}

esp_err_t spi_nand_read_register(spi_nand_flash_device_t *handle, uint8_t reg, uint8_t *val)
{
    spi_nand_transaction_t t = {
        .command = CMD_READ_REGISTER,
        .address_bytes = 1,
        .address = reg,
        .miso_len = 1,
        .miso_data = val,
        .flags = SPI_TRANS_USE_RXDATA,
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_write_register(spi_nand_flash_device_t *handle, uint8_t reg, uint8_t val)
{
    spi_nand_transaction_t t = {
        .command = CMD_SET_REGISTER,
        .address_bytes = 1,
        .address = reg,
        .mosi_len = 1,
        .mosi_data = &val,
        .flags = SPI_TRANS_USE_TXDATA,
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_write_enable(spi_nand_flash_device_t *handle)
{
    spi_nand_transaction_t t = {
        .command = CMD_WRITE_ENABLE
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_read_page(spi_nand_flash_device_t *handle, uint32_t page)
{
    spi_nand_transaction_t t = {
        .command = CMD_PAGE_READ,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_read(spi_nand_flash_device_t *handle, uint8_t *data, uint16_t column, uint16_t length)
{
    // Use fast read command with dummy byte
    spi_nand_transaction_t t = {
        .command = CMD_READ_FAST,
        .address_bytes = 2,
        .address = column,
        .miso_len = length,
        .miso_data = data,
        .dummy_bits = 8,  // Fast read requires 8 dummy bits
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_program_execute(spi_nand_flash_device_t *handle, uint32_t page)
{
    spi_nand_transaction_t t = {
        .command = CMD_PROGRAM_EXECUTE,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_program_load(spi_nand_flash_device_t *handle, const uint8_t *data, uint16_t column, uint16_t length)
{
    spi_nand_transaction_t t = {
        .command = CMD_PROGRAM_LOAD,
        .address_bytes = 2,
        .address = column,
        .mosi_len = length,
        .mosi_data = data,
    };

    return spi_nand_execute_transaction(handle, &t);
}

esp_err_t spi_nand_erase_block(spi_nand_flash_device_t *handle, uint32_t page)
{
    spi_nand_transaction_t t = {
        .command = CMD_ERASE_BLOCK,
        .address_bytes = 3,
        .address = page
    };

    return spi_nand_execute_transaction(handle, &t);
}
