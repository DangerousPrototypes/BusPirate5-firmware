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

#define TAG "nand_winbond"
#define NAND_DEBUG_ENABLE 0

static const char MANUFACTURER_NAME[] = "Winbond";

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

esp_err_t spi_nand_winbond_init(spi_nand_flash_device_t *dev)
{
    esp_err_t ret = ESP_OK;
    uint8_t device_id_buf[2] = {0};
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 0,
        .address_bytes = 2,
        .miso_len = 2,
        .miso_data = device_id_buf,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    ret = spi_nand_execute_transaction(dev, &t);
    if (ret != ESP_OK) {
        NAND_LOGE(TAG, "%s, Failed to get the device ID %d", __func__, ret);
        return ret;
    }

    dev->chip.has_quad_enable_bit = 0;
    dev->chip.quad_enable_bit_pos = 0;
    uint16_t device_id = (device_id_buf[0] << 8) + device_id_buf[1];
    dev->chip.read_page_delay_us = 10;
    dev->chip.erase_block_delay_us = 2500;
    dev->chip.program_page_delay_us = 320;
    NAND_LOGD(TAG, "%s: device_id: %x\n", __func__, device_id);
    switch (device_id) {
    case WINBOND_DI_AA20:
    case WINBOND_DI_BA20:
        dev->chip.num_blocks = 512;
        break;
    case WINBOND_DI_AA21:
    case WINBOND_DI_BA21:
    case WINBOND_DI_BC21:
        dev->chip.num_blocks = 1024;
        break;
    case WINBOND_DI_AA22:
        dev->chip.num_blocks = 2048;
        break;
    case WINBOND_DI_AA23:
        dev->chip.num_blocks = 4096;
        break;
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
