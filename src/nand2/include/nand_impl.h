/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: Adapted for Raspberry Pi Pico (RP2040)
 */

#pragma once

#include <stdint.h>
#include "spi_nand_flash.h"
#include "nand.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nand_is_bad(spi_nand_flash_device_t *handle, uint32_t b, bool *is_bad_status);
esp_err_t nand_mark_bad(spi_nand_flash_device_t *handle, uint32_t b);
esp_err_t nand_erase_chip(spi_nand_flash_device_t *handle);
esp_err_t nand_erase_block(spi_nand_flash_device_t *handle, uint32_t b);
esp_err_t nand_prog(spi_nand_flash_device_t *handle, uint32_t p, const uint8_t *data);
esp_err_t nand_is_free(spi_nand_flash_device_t *handle, uint32_t p, bool *is_free_status);
esp_err_t nand_read(spi_nand_flash_device_t *handle, uint32_t p, size_t offset, size_t length, uint8_t *data);
esp_err_t nand_copy(spi_nand_flash_device_t *handle, uint32_t src, uint32_t dst);
esp_err_t nand_get_ecc_status(spi_nand_flash_device_t *handle, uint32_t page);

#ifdef __cplusplus
}
#endif
