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
#include "dhara/nand.h"
#include "dhara/map.h"
#include "dhara/error.h"
#include "../include/spi_nand_oper.h"
#include "../include/nand_impl.h"
#include "../include/nand.h"

// Container_of macro for dhara
#ifndef __containerof
#define __containerof(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

typedef struct {
    struct dhara_nand dhara_nand;
    struct dhara_map dhara_map;
    spi_nand_flash_device_t *parent_handle;
} spi_nand_flash_dhara_priv_data_t;

static esp_err_t dhara_init(spi_nand_flash_device_t *handle)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = calloc(1, sizeof(spi_nand_flash_dhara_priv_data_t));
    handle->ops_priv_data = dhara_priv_data;
    dhara_priv_data->parent_handle = handle;

    dhara_priv_data->dhara_nand.log2_page_size = handle->chip.log2_page_size;
    dhara_priv_data->dhara_nand.log2_ppb = handle->chip.log2_ppb;
    dhara_priv_data->dhara_nand.num_blocks = handle->chip.num_blocks;

    dhara_map_init(&dhara_priv_data->dhara_map, &dhara_priv_data->dhara_nand, handle->work_buffer, handle->config.gc_factor);
    dhara_error_t ignored=0;
    int i = dhara_map_resume(&dhara_priv_data->dhara_map, &ignored);
    printf("Error code from dhara_map_resume: %d, %d\r\n", i, ignored);
    return ESP_OK;
}

static esp_err_t dhara_deinit(spi_nand_flash_device_t *handle)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_map_init(&dhara_priv_data->dhara_map, &dhara_priv_data->dhara_nand, handle->work_buffer, handle->config.gc_factor);
    dhara_map_clear(&dhara_priv_data->dhara_map);
    return ESP_OK;
}

static esp_err_t dhara_read(spi_nand_flash_device_t *handle, uint8_t *buffer, dhara_sector_t sector_id)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_read(&dhara_priv_data->dhara_map, sector_id, handle->read_buffer, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    memcpy(buffer, handle->read_buffer, handle->chip.page_size);
    return ESP_OK;
}

static esp_err_t dhara_write(spi_nand_flash_device_t *handle, const uint8_t *buffer, dhara_sector_t sector_id)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_write(&dhara_priv_data->dhara_map, sector_id, buffer, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    return ESP_OK;
}

static esp_err_t dhara_copy_sector(spi_nand_flash_device_t *handle, dhara_sector_t src_sec, dhara_sector_t dst_sec)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_copy_sector(&dhara_priv_data->dhara_map, src_sec, dst_sec, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    return ESP_OK;
}

static esp_err_t dhara_trim(spi_nand_flash_device_t *handle, dhara_sector_t sector_id)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_trim(&dhara_priv_data->dhara_map, sector_id, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    return ESP_OK;
}

static esp_err_t dhara_sync(spi_nand_flash_device_t *handle)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    dhara_error_t err;
    if (dhara_map_sync(&dhara_priv_data->dhara_map, &err)) {
        return ESP_ERR_FLASH_BASE + err;
    }
    return ESP_OK;
}

static esp_err_t dhara_get_capacity(spi_nand_flash_device_t *handle, dhara_sector_t *number_of_sectors)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = (spi_nand_flash_dhara_priv_data_t *)handle->ops_priv_data;
    *number_of_sectors = dhara_map_capacity(&dhara_priv_data->dhara_map);
    return ESP_OK;
}

static esp_err_t dhara_erase_chip(spi_nand_flash_device_t *handle)
{
    return nand_erase_chip(handle);
}

static esp_err_t dhara_erase_block(spi_nand_flash_device_t *handle, uint32_t block)
{
    return nand_erase_block(handle, block);
}


const spi_nand_ops dhara_nand_ops = {
    .init = &dhara_init,
    .deinit = &dhara_deinit,
    .read = &dhara_read,
    .write = &dhara_write,
    .erase_chip = &dhara_erase_chip,
    .erase_block = &dhara_erase_block,
    .trim = &dhara_trim,
    .sync = &dhara_sync,
    .copy_sector = &dhara_copy_sector,
    .get_capacity = &dhara_get_capacity,
};

esp_err_t nand_register_dev(spi_nand_flash_device_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    handle->ops = &dhara_nand_ops;
    return ESP_OK;
}

esp_err_t nand_unregister_dev(spi_nand_flash_device_t *handle)
{
    free(handle->ops_priv_data);
    handle->ops = NULL;
    return ESP_OK;
}

/*------------------------------------------------------------------------------------------------------*/

// The following APIs are implementations required by the Dhara library.
// Please refer to the header file dhara/nand.h for details.

int dhara_nand_is_bad(const struct dhara_nand *n, dhara_block_t b)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    bool is_bad_status = false;
    if (nand_is_bad(dev_handle, b, &is_bad_status)) {
        return 1;
    }
    if (is_bad_status == true) {
        return 1;
    }
    return 0;
}

void dhara_nand_mark_bad(const struct dhara_nand *n, dhara_block_t b)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    nand_mark_bad(dev_handle, b);
    return;
}

int dhara_nand_erase(const struct dhara_nand *n, dhara_block_t b, dhara_error_t *err)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    esp_err_t ret = nand_erase_block(dev_handle, b);
    if (ret) {
        if (ret == ESP_ERR_NOT_FINISHED) {
            dhara_set_error(err, DHARA_E_BAD_BLOCK);
        }
        return -1;
    }
    return 0;
}

int dhara_nand_prog(const struct dhara_nand *n, dhara_page_t p, const uint8_t *data, dhara_error_t *err)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    esp_err_t ret = nand_prog(dev_handle, p, data);
    if (ret) {
        if (ret == ESP_ERR_NOT_FINISHED) {
            dhara_set_error(err, DHARA_E_BAD_BLOCK);
        }
        return -1;
    }
    return 0;
}

int dhara_nand_is_free(const struct dhara_nand *n, dhara_page_t p)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    bool is_free_status = true;
    if (nand_is_free(dev_handle, p, &is_free_status)) {
        return 0;
    }
    if (is_free_status == true) {
        return 1;
    }
    return 0;
}

int dhara_nand_read(const struct dhara_nand *n, dhara_page_t p, size_t offset, size_t length,
                    uint8_t *data, dhara_error_t *err)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    if (nand_read(dev_handle, p, offset, length, data)) {
        if (dev_handle->chip.ecc_data.ecc_corrected_bits_status == STAT_ECC_NOT_CORRECTED) {
            dhara_set_error(err, DHARA_E_ECC);
        }
        return -1;
    }
    return 0;
}

int dhara_nand_copy(const struct dhara_nand *n, dhara_page_t src, dhara_page_t dst, dhara_error_t *err)
{
    spi_nand_flash_dhara_priv_data_t *dhara_priv_data = __containerof(n, spi_nand_flash_dhara_priv_data_t, dhara_nand);
    spi_nand_flash_device_t *dev_handle = dhara_priv_data->parent_handle;
    esp_err_t ret = nand_copy(dev_handle, src, dst);
    if (ret) {
        if (dev_handle->chip.ecc_data.ecc_corrected_bits_status == STAT_ECC_NOT_CORRECTED) {
            dhara_set_error(err, DHARA_E_ECC);
        }
        if (ret == ESP_ERR_NOT_FINISHED) {
            dhara_set_error(err, DHARA_E_BAD_BLOCK);
        }
        return -1;
    }
    return 0;
}
/*------------------------------------------------------------------------------------------------------*/
