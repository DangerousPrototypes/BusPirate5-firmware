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
#include <inttypes.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "../include/spi_nand_oper.h"
#include "../include/spi_nand_flash.h"
#include "../include/nand.h"

#define ROM_WAIT_THRESHOLD_US 1000

#define MAX_PAGE_SIZE 2048
//static uint8_t copy_buffer[MAX_PAGE_SIZE];
//static mutex_t copy_buffer_mutex;
//static bool copy_buffer_mutex_initialized = false;

#define TAG "spi_nand"

// Logging macros
#define NAND_LOGD(tag, fmt, ...) printf("[D][%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#define NAND_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#define NAND_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#define NAND_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#define NAND_LOGV(tag, fmt, ...) ((void)0) // Verbose disabled

// Helper macros
#define RETURN_ON_ERROR(x) do { esp_err_t err_rc_ = (x); if (err_rc_ != ESP_OK) return err_rc_; } while(0)
#define GOTO_ON_ERROR(x, label) do { ret = (x); if (ret != ESP_OK) goto label; } while(0)

static esp_err_t wait_for_ready(spi_nand_flash_device_t *dev, uint32_t expected_operation_time_us, uint8_t *status_out)
{
    if (expected_operation_time_us < ROM_WAIT_THRESHOLD_US) {
        busy_wait_us_32(expected_operation_time_us);
    }

    while (true) {
        uint8_t status;
        RETURN_ON_ERROR(spi_nand_read_register(dev, REG_STATUS, &status));

        if ((status & STAT_BUSY) == 0) {
            if (status_out) {
                *status_out = status;
            }
            break;
        }

        if (expected_operation_time_us >= ROM_WAIT_THRESHOLD_US) {
            // Use 100us delay for better responsiveness while still yielding CPU
            busy_wait_us_32(100);
        }
    }

    return ESP_OK;
}

static esp_err_t read_page_and_wait(spi_nand_flash_device_t *dev, uint32_t page, uint8_t *status_out)
{
    RETURN_ON_ERROR(spi_nand_read_page(dev, page));
    return wait_for_ready(dev, dev->chip.read_page_delay_us, status_out);
}

static esp_err_t program_execute_and_wait(spi_nand_flash_device_t *dev, uint32_t page, uint8_t *status_out)
{
    RETURN_ON_ERROR(spi_nand_program_execute(dev, page));
    return wait_for_ready(dev, dev->chip.program_page_delay_us, status_out);
}

static uint16_t get_column_address(spi_nand_flash_device_t *handle, uint32_t block, uint32_t offset)
{
    uint16_t column_addr = offset;

    if ((handle->chip.flags & NAND_FLAG_HAS_READ_PLANE_SELECT) || (handle->chip.flags & NAND_FLAG_HAS_PROG_PLANE_SELECT)) {
        uint32_t plane = block % handle->chip.num_planes;
        column_addr += plane << (handle->chip.log2_page_size + 1);
    }
    return column_addr;
}
#if 0
esp_err_t nand_is_bad(spi_nand_flash_device_t *handle, uint32_t block, bool *is_bad_status)
{
    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    uint16_t bad_block_indicator;
    esp_err_t ret = ESP_OK;

    GOTO_ON_ERROR(read_page_and_wait(handle, first_block_page, NULL), fail);

    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);

    GOTO_ON_ERROR(spi_nand_read(handle, (uint8_t *)handle->read_buffer, column_addr, 2), fail);

    memcpy(&bad_block_indicator, handle->read_buffer, sizeof(bad_block_indicator));
    NAND_LOGD(TAG, "is_bad, block=%"PRIu32", page=%"PRIu32",indicator = %04x", block, first_block_page, bad_block_indicator);
    *is_bad_status = (bad_block_indicator != 0xFFFF);
    return ret;

fail:
    NAND_LOGE(TAG, "Error in nand_is_bad %d", ret);
    return ret;
}
#endif

esp_err_t nand_is_bad(spi_nand_flash_device_t *handle, uint32_t block, bool *is_bad_status)
{
    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    uint8_t bad_block_indicator;  // Change to single byte
    esp_err_t ret = ESP_OK;

    GOTO_ON_ERROR(read_page_and_wait(handle, first_block_page, NULL), fail);
    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);
    GOTO_ON_ERROR(spi_nand_read(handle, &bad_block_indicator, column_addr, 1), fail);  // Read 1 byte

    *is_bad_status = (bad_block_indicator == 0x00);  // Bad if first byte is 0x00
    NAND_LOGD(TAG, "is_bad, block=%"PRIu32", page=%"PRIu32", indicator=0x%02x", 
              block, first_block_page, bad_block_indicator);
    return ret;
fail:
    NAND_LOGE(TAG, "Error in nand_is_bad %d", ret);
    return ret;
}

esp_err_t nand_mark_bad(spi_nand_flash_device_t *handle, uint32_t block)
{
    esp_err_t ret = ESP_OK;

    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);
    uint8_t bad_block_indicator = 0x00;
    uint8_t status;
    NAND_LOGD(TAG, "mark_bad, block=%"PRIu32", page=%"PRIu32",indicator = %02x", block, first_block_page, bad_block_indicator);

    GOTO_ON_ERROR(read_page_and_wait(handle, first_block_page, NULL), fail);
    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
    GOTO_ON_ERROR(spi_nand_erase_block(handle, first_block_page), fail);
    GOTO_ON_ERROR(wait_for_ready(handle, handle->chip.erase_block_delay_us, &status), fail);
    if ((status & STAT_ERASE_FAILED) != 0) {
        ret = ESP_ERR_NOT_FINISHED;
        goto fail;
    }

    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);

    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);

    GOTO_ON_ERROR(spi_nand_program_load(handle, (const uint8_t *)&bad_block_indicator, column_addr, 1), fail);
    GOTO_ON_ERROR(program_execute_and_wait(handle, first_block_page, NULL), fail);

    return ret;
fail:
    NAND_LOGE(TAG, "Error in nand_mark_bad %d", ret);
    return ret;
}

esp_err_t nand_erase_chip(spi_nand_flash_device_t *handle)
{
    esp_err_t ret = ESP_OK;
    uint8_t status;

    for (uint32_t i = 0; i < handle->chip.num_blocks; i++) {
        GOTO_ON_ERROR(spi_nand_write_enable(handle), end);
        GOTO_ON_ERROR(spi_nand_erase_block(handle, i * (1 << handle->chip.log2_ppb)), end);
        GOTO_ON_ERROR(wait_for_ready(handle, handle->chip.erase_block_delay_us, &status), end);
        if ((status & STAT_ERASE_FAILED) != 0) {
            ret = ESP_ERR_NOT_FINISHED;
        }
    }
    return ret;

end:
    NAND_LOGE(TAG, "Error in nand_erase_chip %d", ret);
    return ret;
}

esp_err_t nand_erase_block(spi_nand_flash_device_t *handle, uint32_t block)
{
    NAND_LOGD(TAG, "erase_block, block=%"PRIu32",", block);
    esp_err_t ret = ESP_OK;
    uint8_t status;

    uint32_t first_block_page = block * (1 << handle->chip.log2_ppb);

    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
    GOTO_ON_ERROR(spi_nand_erase_block(handle, first_block_page), fail);
    GOTO_ON_ERROR(wait_for_ready(handle, handle->chip.erase_block_delay_us, &status), fail);

    if ((status & STAT_ERASE_FAILED) != 0) {
        ret = ESP_ERR_NOT_FINISHED;
    }
    return ret;

fail:
    NAND_LOGE(TAG, "Error in nand_erase %d", ret);
    return ret;
}

esp_err_t nand_prog(spi_nand_flash_device_t *handle, uint32_t page, const uint8_t *data)
{
    NAND_LOGV(TAG, "prog, page=%"PRIu32",", page);
    esp_err_t ret = ESP_OK;
    uint8_t status;

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, 0);

    GOTO_ON_ERROR(read_page_and_wait(handle, page, NULL), fail);
    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
    GOTO_ON_ERROR(spi_nand_program_load(handle, data, column_addr, handle->chip.page_size), fail);

    GOTO_ON_ERROR(program_execute_and_wait(handle, page, &status), fail);

    if ((status & STAT_PROGRAM_FAILED) != 0) {
        NAND_LOGD(TAG, "prog failed, page=%"PRIu32",", page);
        return ESP_ERR_NOT_FINISHED;
    }

    return ret;
fail:
    NAND_LOGE(TAG, "Error in nand_prog %d", ret);
    return ret;
}
#if 0
esp_err_t nand_is_free(spi_nand_flash_device_t *handle, uint32_t page, bool *is_free_status)
{
    esp_err_t ret = ESP_OK;
    uint16_t used_marker;

    GOTO_ON_ERROR(read_page_and_wait(handle, page, NULL), fail);

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size + 2);

    GOTO_ON_ERROR(spi_nand_read(handle, (uint8_t *)handle->read_buffer, column_addr, 2), fail);

    memcpy(&used_marker, handle->read_buffer, sizeof(used_marker));
    NAND_LOGD(TAG, "is free, page=%"PRIu32", used_marker=%04x,", page, used_marker);
    *is_free_status = (used_marker == 0xFFFF);
    return ret;
fail:
    NAND_LOGE(TAG, "Error in nand_is_free %d", ret);
    return ret;
}
#endif
esp_err_t nand_is_free(spi_nand_flash_device_t *handle, uint32_t page, bool *is_free_status)
{
    esp_err_t ret = ESP_OK;
    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, 0);
    size_t check_len = handle->chip.page_size + 4; // page + some spare area
    
    GOTO_ON_ERROR(read_page_and_wait(handle, page, NULL), fail);
    GOTO_ON_ERROR(spi_nand_read(handle, handle->read_buffer, column_addr, check_len), fail);
    
    *is_free_status = true;
    uint32_t comp_word = 0xFFFFFFFF;
    for (size_t i = 0; i < check_len; i += sizeof(comp_word)) {
        if (memcmp(&comp_word, &handle->read_buffer[i], sizeof(comp_word)) != 0) {
            *is_free_status = false;
            break;
        }
    }
    NAND_LOGD(TAG, "is free, page=%"PRIu32", is_free=%d,", page, *is_free_status);
    return ret;
fail:
    NAND_LOGE(TAG, "Error in nand_is_free %d", ret);
    return ret;
}

#define PACK_2BITS_STATUS(status, bit1, bit0)         ((((status) & (bit1)) << 1) | ((status) & (bit0)))
#define PACK_3BITS_STATUS(status, bit2, bit1, bit0)   ((((status) & (bit2)) << 2) | (((status) & (bit1)) << 1) | ((status) & (bit0)))

static bool is_ecc_error(spi_nand_flash_device_t *dev, uint8_t status)
{
    bool is_ecc_err = false;
    ecc_status_t bits_corrected_status = STAT_ECC_OK;
    if (dev->chip.ecc_data.ecc_status_reg_len_in_bits == 2) {
        bits_corrected_status = PACK_2BITS_STATUS(status, STAT_ECC1, STAT_ECC0);
    } else if (dev->chip.ecc_data.ecc_status_reg_len_in_bits == 3) {
        bits_corrected_status = PACK_3BITS_STATUS(status, STAT_ECC2, STAT_ECC1, STAT_ECC0);
    } else {
        bits_corrected_status = STAT_ECC_MAX;
    }
    dev->chip.ecc_data.ecc_corrected_bits_status = bits_corrected_status;
    if (bits_corrected_status) {
        if (bits_corrected_status == STAT_ECC_MAX) {
            NAND_LOGE(TAG, "%s: Error while initializing value of ecc_status_reg_len_in_bits", __func__);
            is_ecc_err = true;
        } else if (bits_corrected_status == STAT_ECC_NOT_CORRECTED) {
            is_ecc_err = true;
        }
    }
    return is_ecc_err;
}

esp_err_t nand_read(spi_nand_flash_device_t *handle, uint32_t page, size_t offset, size_t length, uint8_t *data)
{
    NAND_LOGV(TAG, "read, page=%"PRIu32", offset=%zu, length=%zu", page, offset, length);
    esp_err_t ret = ESP_OK;
    uint8_t status;

    GOTO_ON_ERROR(read_page_and_wait(handle, page, &status), fail);

    if (is_ecc_error(handle, status)) {
        NAND_LOGD(TAG, "read ecc error, page=%"PRIu32"", page);
        return ESP_FAIL;
    }

    uint32_t block = page >> handle->chip.log2_ppb;
    uint16_t column_addr = get_column_address(handle, block, offset);

    GOTO_ON_ERROR(spi_nand_read(handle, data, column_addr, length), fail);

    return ret;
fail:
    NAND_LOGE(TAG, "Error in nand_read %d", ret);
    return ret;
}

esp_err_t nand_copy(spi_nand_flash_device_t *handle, uint32_t src, uint32_t dst)
{
    NAND_LOGD(TAG, "copy, src=%"PRIu32", dst=%"PRIu32"", src, dst);
    esp_err_t ret = ESP_OK;
    uint8_t *copy_buf = NULL;

    uint8_t status;
    GOTO_ON_ERROR(read_page_and_wait(handle, src, &status), fail);

    if (is_ecc_error(handle, status)) {
        NAND_LOGD(TAG, "copy, ecc error");
        return ESP_FAIL;
    }

    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
    uint32_t src_block = src >> handle->chip.log2_ppb;
    uint32_t dst_block = dst >> handle->chip.log2_ppb;
    uint16_t src_column_addr = get_column_address(handle, src_block, 0);
    uint16_t dst_column_addr = get_column_address(handle, dst_block, 0);

    if (src_column_addr != dst_column_addr) {
        // In a 2 plane structure of the flash, if the pages are not on the same plane, the data must be copied through RAM. 
        // Use the device's temp_buffer instead of malloc (temp_buffer size is page_size + 1)
        copy_buf = handle->read_buffer;
        if (! copy_buf) {
            ret = ESP_ERR_NO_MEM;
            goto fail;
        }

        GOTO_ON_ERROR(spi_nand_read(handle, copy_buf, src_column_addr, handle->chip.page_size), fail);

        GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);

        GOTO_ON_ERROR(spi_nand_program_load(handle, copy_buf, dst_column_addr, handle->chip.page_size), fail);

        GOTO_ON_ERROR(program_execute_and_wait(handle, dst, &status), fail);

        if ((status & STAT_PROGRAM_FAILED) != 0) {
            NAND_LOGD(TAG, "copy, prog failed");
            return ESP_ERR_NOT_FINISHED;
        }
        copy_buf = NULL;
    } else {
        GOTO_ON_ERROR(program_execute_and_wait(handle, dst, &status), fail);
        if ((status & STAT_PROGRAM_FAILED) != 0) {
            NAND_LOGD(TAG, "copy, prog failed");
            return ESP_ERR_NOT_FINISHED;
        }
    }

    return ret;

fail:
    NAND_LOGE(TAG, "Error in nand_copy %d", ret);
    return ret;
}

esp_err_t nand_get_ecc_status(spi_nand_flash_device_t *handle, uint32_t page)
{
    esp_err_t ret = ESP_OK;
    uint8_t status;
    GOTO_ON_ERROR(read_page_and_wait(handle, page, &status), fail);

    if (is_ecc_error(handle, status)) {
        NAND_LOGD(TAG, "read ecc error, page=%"PRIu32"", page);
    }
    return ret;

fail:
    NAND_LOGE(TAG, "Error in nand_is_ecc_error %d", ret);
    return ret;
}
