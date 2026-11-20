/*
 * SPDX-FileCopyrightText: 2026 Bus Pirate contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "../include/nand.h"
#include "../include/nand_private/nand_flash_devices.h"
#include "../include/spi_nand_oper.h"

#define TAG "nand_dosilicon"
#define NAND_DEBUG_ENABLE 0

#if NAND_DEBUG_ENABLE
#define NAND_LOGD(tag, fmt, ...) printf("[D][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define NAND_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
#define NAND_LOGD(tag, fmt, ...) ((void)0)
#define NAND_LOGE(tag, fmt, ...) ((void)0)
#endif

static const char MANUFACTURER_NAME[] = "Dosilicon";

esp_err_t spi_nand_dosilicon_init(spi_nand_flash_device_t *dev)
{
    uint8_t device_id[2] = { 0 };
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 0,
        .address_bytes = 1,
        .miso_len = sizeof(device_id),
        .miso_data = device_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    esp_err_t ret = spi_nand_execute_transaction(dev, &t);
    if (ret != ESP_OK) {
        NAND_LOGE(TAG, "%s: Failed to get the device ID %d", __func__, ret);
        return ret;
    }

    dev->manufacturer_name = MANUFACTURER_NAME;
    dev->chip.has_quad_enable_bit = 1;
    dev->chip.quad_enable_bit_pos = 0;
    dev->chip.read_page_delay_us = 60;
    dev->chip.erase_block_delay_us = 2000;
    dev->chip.program_page_delay_us = 320;
    dev->chip.flags |= NAND_FLAG_NO_INTERNAL_COPY;

    NAND_LOGD(TAG, "%s: device_id: %02x%02x", __func__, device_id[0], device_id[1]);
    if (device_id[0] != SPI_NAND_FLASH_DOSILICON_MI || device_id[1] != DOSILICON_DI_71) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    dev->chip.num_blocks = 1024;
    return ESP_OK;
}
