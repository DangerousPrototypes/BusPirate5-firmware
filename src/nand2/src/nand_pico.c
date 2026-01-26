/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: Adapted for Raspberry Pi Pico (RP2040)
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "pirate.h"
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "../include/spi_nand_flash.h"
#include "../include/spi_nand_oper.h"
#include "../include/nand.h"
#include "../include/nand_impl.h"
#include "../include/nand_private/nand_flash_devices.h"
#include "../include/nand_private/nand_flash_chip.h"

#define TAG "nand_flash"

// Logging macros - set NAND_DEBUG_ENABLE to 1 to enable debug output
#define NAND_DEBUG_ENABLE 0

#if NAND_DEBUG_ENABLE
#define NAND_LOGD(tag, fmt, ...) printf("[D][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define NAND_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define NAND_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define NAND_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
#define NAND_LOGD(tag, fmt, ...) ((void)0)
#define NAND_LOGI(tag, fmt, ...) ((void)0)
#define NAND_LOGW(tag, fmt, ...) ((void)0)
#define NAND_LOGE(tag, fmt, ...) ((void)0)
#endif

// Helper macros
#define RETURN_ON_ERROR(x) do { esp_err_t err_rc_ = (x); if (err_rc_ != ESP_OK) return err_rc_; } while(0)
#define RETURN_ON_FALSE(cond, err, tag, msg) do { if (!(cond)) { NAND_LOGE(tag, msg); return err; } } while(0)
#define GOTO_ON_ERROR(x, label, tag, msg) do { ret = (x); if (ret != ESP_OK) { NAND_LOGE(tag, msg); goto label; } } while(0)
#define GOTO_ON_FALSE(cond, err, label, tag, msg) do { if (!(cond)) { ret = err; NAND_LOGE(tag, msg); goto label; } } while(0)

static esp_err_t detect_chip(spi_nand_flash_device_t *dev)
{
    uint8_t manufacturer_id;
    spi_nand_transaction_t t = {
        .command = CMD_READ_ID,
        .address = 0xff,
        .address_bytes = 1,
        .miso_len = 1,
        .miso_data = &manufacturer_id,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    spi_nand_execute_transaction(dev, &t);
    NAND_LOGD(TAG, "%s: manufacturer_id: %x\n", __func__, manufacturer_id); 

    // Uncomment for early boot debugging (before printf initialized)
    // printf("Detected SPI NAND Manufacturer ID: 0x%02X\n", manufacturer_id);

    switch (manufacturer_id) {
    case SPI_NAND_FLASH_ALLIANCE_MI:
        return spi_nand_alliance_init(dev);
    case SPI_NAND_FLASH_WINBOND_MI:
        return spi_nand_winbond_init(dev);
    case SPI_NAND_FLASH_GIGADEVICE_MI:
        return spi_nand_gigadevice_init(dev);
    case SPI_NAND_FLASH_MICRON_MI:
        return spi_nand_micron_init(dev);
    case SPI_NAND_FLASH_ZETTA_MI:
        return spi_nand_zetta_init(dev);
    case SPI_NAND_FLASH_XTX_MI:
        return spi_nand_xtx_init(dev);
    default:
        NAND_LOGE(TAG, "Unknown manufacturer ID: 0x%02X", manufacturer_id);
        return ESP_ERR_INVALID_RESPONSE;
    }
}

static esp_err_t unprotect_chip(spi_nand_flash_device_t *dev)
{
    uint8_t status;
    esp_err_t ret = spi_nand_read_register(dev, REG_PROTECT, &status);
    if (ret != ESP_OK) {
        return ret;
    }

    if (status != 0x00) {
        ret = spi_nand_write_register(dev, REG_PROTECT, 0);
    }

    return ret;
}

static uint8_t work[2048+4];
static uint8_t read[2048+4];

esp_err_t spi_nand_flash_init_device(spi_nand_flash_config_t *config, spi_nand_flash_device_t **handle)
{
    RETURN_ON_FALSE(config->spi != NULL, ESP_ERR_INVALID_ARG, TAG, "SPI instance pointer cannot be NULL");

    if (!config->gc_factor) {
        config->gc_factor = 4;
    }

    *handle = calloc(1, sizeof(spi_nand_flash_device_t));
    if (*handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(&(*handle)->config, config, sizeof(spi_nand_flash_config_t));

    (*handle)->chip.ecc_data.ecc_status_reg_len_in_bits = 2;
    (*handle)->chip.ecc_data.ecc_data_refresh_threshold = 4;
    (*handle)->chip.log2_ppb = 6;         // 64 pages per block is standard
    (*handle)->chip.log2_page_size = 11;  // 2048 bytes per page is fairly standard
    (*handle)->chip.num_planes = 1;
    (*handle)->chip.flags = 0;

    esp_err_t ret = ESP_OK;
    // Uncomment for early boot debugging (before printf initialized)
    // printf("Starting NAND chip detection...\r\n");
    GOTO_ON_ERROR(detect_chip(*handle), fail, TAG, "Failed to detect nand chip");
    GOTO_ON_ERROR(unprotect_chip(*handle), fail, TAG, "Failed to clear protection register");
    // printf("NAND chip detected and unprotected successfully.\r\n");
    (*handle)->chip.page_size = 1 << (*handle)->chip.log2_page_size;
    (*handle)->chip.block_size = (1 << (*handle)->chip.log2_ppb) * (*handle)->chip.page_size;

    // we only support up to 2048 byte page size on PICO due to memory constraints
    GOTO_ON_FALSE((*handle)->chip.page_size <= 2048, ESP_ERR_NO_MEM, fail, TAG, "nomem");

    #if 1 //make static buffers for PICO
    (*handle)->work_buffer = work; //this buffer is for dhara internal use
    (*handle)->read_buffer = read; //this is for passing pages from dhara to nand
    #else
    (*handle)->work_buffer = malloc((*handle)->chip.page_size);
    GOTO_ON_FALSE((*handle)->work_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

    (*handle)->read_buffer = malloc((*handle)->chip.page_size);
    GOTO_ON_FALSE((*handle)->read_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");

    (*handle)->temp_buffer = malloc((*handle)->chip.page_size + 1);
    GOTO_ON_FALSE((*handle)->temp_buffer != NULL, ESP_ERR_NO_MEM, fail, TAG, "nomem");
    #endif
    mutex_init(&(*handle)->mutex);

    GOTO_ON_ERROR(nand_register_dev(*handle), fail, TAG, "Failed to register nand dev");

    if ((*handle)->ops->init == NULL) {
        NAND_LOGE(TAG, "Failed to initialize spi_nand_ops");
        ret = ESP_FAIL;
        goto fail;
    }
    (*handle)->ops->init(*handle);

    return ret;

fail:
    free(*handle);
    return ret;
}

esp_err_t spi_nand_erase_chip(spi_nand_flash_device_t *handle)
{
    NAND_LOGW(TAG, "Entire chip is being erased");
    esp_err_t ret = ESP_OK;

    ret = handle->ops->erase_chip(handle);
    if (ret) {
        return ret;
    }
    handle->ops->deinit(handle);

    return ret;
}

static bool s_need_data_refresh(spi_nand_flash_device_t *handle)
{
    uint8_t min_bits_corrected = 0;
    bool ret = false;
    if (handle->chip.ecc_data.ecc_corrected_bits_status == STAT_ECC_1_TO_3_BITS_CORRECTED) {
        min_bits_corrected = 1;
    } else if (handle->chip.ecc_data.ecc_corrected_bits_status == STAT_ECC_4_TO_6_BITS_CORRECTED) {
        min_bits_corrected = 4;
    } else if (handle->chip.ecc_data.ecc_corrected_bits_status == STAT_ECC_7_8_BITS_CORRECTED) {
        min_bits_corrected = 7;
    }

    if (min_bits_corrected >= handle->chip.ecc_data.ecc_data_refresh_threshold) {
        ret = true;
    }
    return ret;
}

esp_err_t spi_nand_flash_read_sector(spi_nand_flash_device_t *handle, uint8_t *buffer, uint32_t sector_id)
{
    esp_err_t ret = ESP_OK;

    ret = handle->ops->read(handle, buffer, sector_id);
    if (ret == ESP_OK && handle->chip.ecc_data.ecc_corrected_bits_status) {
        if (s_need_data_refresh(handle)) {
            ret = handle->ops->write(handle, buffer, sector_id);
        }
    }

    return ret;
}

esp_err_t spi_nand_flash_copy_sector(spi_nand_flash_device_t *handle, uint32_t src_sec, uint32_t dst_sec)
{
    esp_err_t ret = ESP_OK;

    ret = handle->ops->copy_sector(handle, src_sec, dst_sec);

    return ret;
}

esp_err_t spi_nand_flash_write_sector(spi_nand_flash_device_t *handle, const uint8_t *buffer, uint32_t sector_id)
{
    esp_err_t ret = ESP_OK;

    ret = handle->ops->write(handle, buffer, sector_id);

    return ret;
}

esp_err_t spi_nand_flash_trim(spi_nand_flash_device_t *handle, uint32_t sector_id)
{
    esp_err_t ret = ESP_OK;

    ret = handle->ops->trim(handle, sector_id);

    return ret;
}

esp_err_t spi_nand_flash_sync(spi_nand_flash_device_t *handle)
{
    esp_err_t ret = ESP_OK;

    ret = handle->ops->sync(handle);

    return ret;
}

esp_err_t spi_nand_flash_get_capacity(spi_nand_flash_device_t *handle, uint32_t *number_of_sectors)
{
    return handle->ops->get_capacity(handle, number_of_sectors);
}

esp_err_t spi_nand_flash_get_sector_size(spi_nand_flash_device_t *handle, uint32_t *sector_size)
{
    *sector_size = handle->chip.page_size;
    return ESP_OK;
}

esp_err_t spi_nand_flash_get_block_size(spi_nand_flash_device_t *handle, uint32_t *block_size)
{
    *block_size = handle->chip.block_size;
    return ESP_OK;
}

esp_err_t spi_nand_flash_get_block_num(spi_nand_flash_device_t *handle, uint32_t *num_blocks)
{
    *num_blocks = handle->chip.num_blocks;
    return ESP_OK;
}

esp_err_t spi_nand_flash_deinit_device(spi_nand_flash_device_t *handle)
{
    nand_unregister_dev(handle);
    free(handle);
    return ESP_OK;
}
