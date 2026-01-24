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

#define TAG "nand_micron"
#define NAND_LOGD(tag, fmt, ...) printf("[D][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define NAND_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)

#define RETURN_ON_ERROR(x) do { esp_err_t err_rc_ = (x); if (err_rc_ != ESP_OK) return err_rc_; } while(0)

esp_err_t spi_nand_micron_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint8_t device_id = 0;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 0xffff,
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

    dev->chip.has_quad_enable_bit = 0;
    dev->chip.quad_enable_bit_pos = 0;
    dev->chip.ecc_data.ecc_status_reg_len_in_bits = 3;
    dev->chip.erase_block_delay_us = 2000;
    NAND_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case MICRON_DI_34:
        dev->chip.read_page_delay_us = 115;
        dev->chip.program_page_delay_us = 240;
        dev->chip.num_blocks = 2048;
        dev->chip.log2_ppb = 6;        // 64 pages per block
        dev->chip.log2_page_size = 12; // 4096 bytes per page
        break;
    case MICRON_DI_14:
    case MICRON_DI_15:
        dev->chip.read_page_delay_us = 46;
        dev->chip.program_page_delay_us = 220;
        dev->chip.num_blocks = 1024;
        dev->chip.log2_ppb = 6;          // 64 pages per block
        dev->chip.log2_page_size = 11;   // 2048 bytes per page
        break;
    case MICRON_DI_24:
        dev->chip.read_page_delay_us = 55;
        dev->chip.program_page_delay_us = 220;
        dev->chip.num_blocks = 2048;
        dev->chip.log2_ppb = 6;        // 64 pages per block
        dev->chip.log2_page_size = 11; // 2048 bytes per page
        dev->chip.flags = NAND_FLAG_HAS_PROG_PLANE_SELECT | NAND_FLAG_HAS_READ_PLANE_SELECT;
        dev->chip.num_planes = 2;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
