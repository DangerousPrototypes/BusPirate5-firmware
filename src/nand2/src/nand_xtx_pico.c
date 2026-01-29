/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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

#define TAG "nand_xtx"
#define NAND_DEBUG_ENABLE 0

static const char MANUFACTURER_NAME[] = "XTX";

// Logging macros
#if NAND_DEBUG_ENABLE
    #define NAND_LOGD(tag, fmt, ...) printf("[D][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define NAND_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define NAND_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define NAND_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define NAND_LOGV(tag, fmt, ...) ((void)0) // Verbose disabled
#else
    #define NAND_LOGD(tag, fmt, ...) ((void)0)
    #define NAND_LOGI(tag, fmt, ...) ((void)0)
    #define NAND_LOGW(tag, fmt, ...) ((void)0)
    #define NAND_LOGE(tag, fmt, ...) ((void)0)
    #define NAND_LOGV(tag, fmt, ...) ((void)0) // Verbose disabled
#endif

#define RETURN_ON_ERROR(x) do { esp_err_t err_rc_ = (x); if (err_rc_ != ESP_OK) return err_rc_; } while(0)

esp_err_t spi_nand_xtx_init(spi_nand_flash_device_t *dev)
{
    dev->manufacturer_name = MANUFACTURER_NAME;
    esp_err_t ret = ESP_OK;
    uint8_t device_id = 0;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 1,
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
    dev->chip.erase_block_delay_us = 3500;
    dev->chip.program_page_delay_us = 650;
    dev->chip.read_page_delay_us = 50;
    NAND_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case XTX_DI_37: //XT26G08D
        dev->chip.num_blocks = 4096;
        dev->chip.log2_ppb = 6;        // 64 pages per block
        dev->chip.log2_page_size = 12; // 4096 bytes per page
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
