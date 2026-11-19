/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "../spi_nand_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t spi_nand_winbond_init(spi_nand_flash_device_t *dev);
esp_err_t spi_nand_alliance_init(spi_nand_flash_device_t *dev);
esp_err_t spi_nand_gigadevice_init(spi_nand_flash_device_t *dev);
esp_err_t spi_nand_micron_init(spi_nand_flash_device_t *dev);
esp_err_t spi_nand_zetta_init(spi_nand_flash_device_t *dev);
esp_err_t spi_nand_xtx_init(spi_nand_flash_device_t *dev);

#ifdef __cplusplus
}
#endif
