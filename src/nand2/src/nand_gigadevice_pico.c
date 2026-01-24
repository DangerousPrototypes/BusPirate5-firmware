/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: Adapted for Raspberry Pi Pico (RP2040)
 */

#include <string.h>
#include <stdio.h>
#include "../include/nand.h"
#include "../include/spi_nand_oper.h"
#include "../include/nand_private/nand_flash_devices.h"

#define TAG "nand_gigadevice"
#define NAND_LOGD(tag, fmt, ...) ((void)0)
#define NAND_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)

#define RETURN_ON_ERROR(x) do { esp_err_t err_rc_ = (x); if (err_rc_ != ESP_OK) return err_rc_; } while(0)

esp_err_t spi_nand_gigadevice_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint8_t device_id = 0;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 0,
        .address_bytes = 2,
        .miso_len = 1,
        .miso_data = &device_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    ret = spi_nand_execute_transaction(dev, &t);
    if (ret != ESP_OK) {
        NAND_LOGE(TAG, "%s, Failed to get the device ID %d", __func__, ret);
        return ret;
    }

    dev->chip.has_quad_enable_bit = 1;
    dev->chip.quad_enable_bit_pos = 0;
    dev->chip.read_page_delay_us = 25;
    dev->chip.erase_block_delay_us = 3200;
    dev->chip.program_page_delay_us = 380;
    NAND_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case GIGADEVICE_DI_51:
    case GIGADEVICE_DI_41:
    case GIGADEVICE_DI_31:
    case GIGADEVICE_DI_21:
        dev->chip.num_blocks = 1024;
        break;
    case GIGADEVICE_DI_52:
    case GIGADEVICE_DI_42:
    case GIGADEVICE_DI_32:
    case GIGADEVICE_DI_22:
    case GIGADEVICE_DI_92:
    case GIGADEVICE_DI_82:
        dev->chip.num_blocks = 2048;
        break;
    case GIGADEVICE_DI_55:
    case GIGADEVICE_DI_45:
    case GIGADEVICE_DI_35:
    case GIGADEVICE_DI_25:
    case GIGADEVICE_DI_95:
    case GIGADEVICE_DI_85:
        dev->chip.num_blocks = 4096;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
