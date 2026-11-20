/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileContributor: Adapted for Raspberry Pi Pico (RP2040)
 * SPDX-FileContributor: Documented GigaDevice M7 3-bit ECC status support
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

#define TAG "spi_nand"

#define NAND_DEBUG_ENABLE 0
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

// Helper macros
#define RETURN_ON_ERROR(x) do { esp_err_t err_rc_ = (x); if (err_rc_ != ESP_OK) return err_rc_; } while(0)
#define GOTO_ON_ERROR(x, label) do { ret = (x); if (ret != ESP_OK) goto label; } while(0)

void nand_op_begin(spi_nand_flash_device_t *handle)
{
    handle->op_deadline = make_timeout_time_ms(NAND_OP_DEADLINE_MS);
}

static esp_err_t wait_for_ready(spi_nand_flash_device_t *dev, uint32_t expected_operation_time_us, uint8_t *status_out)
{
    // Once the chip has stopped responding, fail immediately instead of burning the full timeout on
    // every page; otherwise a journal scan/sync can stall boot or the terminal for minutes.
    if (dev->io_fault) {
        return ESP_ERR_TIMEOUT;
    }
    if (!is_nil_time(dev->op_deadline) && time_reached(dev->op_deadline)) {
        NAND_LOGE(TAG, "FTL operation exceeded %d ms, giving up", NAND_OP_DEADLINE_MS);
        dev->stats.deadline_hits++;
        dev->io_fault = true;
        return ESP_ERR_TIMEOUT;
    }

    // Always wait before the first poll: some parts (e.g. Zetta) do not raise OIP immediately
    // after CS# goes high, so an immediate poll could report "ready" while an erase is starting.
    busy_wait_us_32(MIN(expected_operation_time_us, ROM_WAIT_THRESHOLD_US));

    // Bound the poll so a stalled/unresponsive device can never hang the firmware.
    // Worst-case NAND op (block erase) is ~5ms; 500ms is a safe upper limit.
    absolute_time_t deadline = make_timeout_time_ms(500);
    while (true) {
        uint8_t status;
        RETURN_ON_ERROR(spi_nand_read_register(dev, REG_STATUS, &status));

        if ((status & STAT_BUSY) == 0) {
            if (status_out) {
                *status_out = status;
            }
            break;
        }

        if (time_reached(deadline)) {
            NAND_LOGE(TAG, "wait_for_ready timed out (status=0x%02x)", status);
            dev->stats.busy_timeouts++;
            dev->io_fault = true;
            return ESP_ERR_TIMEOUT;
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
    handle->stats.marked_bad++;

    GOTO_ON_ERROR(read_page_and_wait(handle, first_block_page, NULL), fail);
    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
    GOTO_ON_ERROR(spi_nand_erase_block(handle, first_block_page), fail);
    GOTO_ON_ERROR(wait_for_ready(handle, handle->chip.erase_block_delay_us, &status), fail);
    if ((status & STAT_ERASE_FAILED) != 0) {
        ret = ESP_ERR_NOT_FINISHED;
        goto fail;
    }

    uint16_t column_addr = get_column_address(handle, block, handle->chip.page_size);

    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
    GOTO_ON_ERROR(spi_nand_program_load(handle, (const uint8_t *)&bad_block_indicator, column_addr, 1), fail);
    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
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

    handle->stats.erases++;
    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
    GOTO_ON_ERROR(spi_nand_erase_block(handle, first_block_page), fail);
    GOTO_ON_ERROR(wait_for_ready(handle, handle->chip.erase_block_delay_us, &status), fail);

    if ((status & STAT_ERASE_FAILED) != 0) {
        handle->stats.erase_fail++;
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

    handle->stats.progs++;
    GOTO_ON_ERROR(read_page_and_wait(handle, page, NULL), fail);
    // All supported datasheets (Micron, GigaDevice, Zetta) require WRITE ENABLE before PROGRAM LOAD;
    // repeating it before PROGRAM EXECUTE is harmless and guards parts that drop WEL on the load.
    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
    GOTO_ON_ERROR(spi_nand_program_load(handle, data, column_addr, handle->chip.page_size), fail);
    GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);

    GOTO_ON_ERROR(program_execute_and_wait(handle, page, &status), fail);

    if ((status & STAT_PROGRAM_FAILED) != 0) {
        NAND_LOGD(TAG, "prog failed, page=%"PRIu32",", page);
        handle->stats.prog_fail++;
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

// Shift the ECCS field (contiguous bits, bit0 lowest) down to a 0-based value matching ecc_status_t.
// Verified against the Micron MT29F1G01 and GigaDevice GD5F1GM7/GD5F2GM7
// (3-bit, C0h[6:4]), and Zetta ZD35X1GA (2-bit, C0h[5:4]) datasheets.
#define PACK_2BITS_STATUS(status, bit1, bit0)         (((status) & ((bit1) | (bit0))) / (bit0))
#define PACK_3BITS_STATUS(status, bit2, bit1, bit0)   (((status) & ((bit2) | (bit1) | (bit0))) / (bit0))

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
    if (is_ecc_err) {
        dev->stats.ecc_uncorrectable++;
    } else if (bits_corrected_status) {
        dev->stats.ecc_corrected++;
    }
    return is_ecc_err;
}

esp_err_t nand_read(spi_nand_flash_device_t *handle, uint32_t page, size_t offset, size_t length, uint8_t *data)
{
    NAND_LOGV(TAG, "read, page=%"PRIu32", offset=%zu, length=%zu", page, offset, length);
    handle->stats.reads++;
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
    handle->stats.copies++;
    GOTO_ON_ERROR(read_page_and_wait(handle, src, &status), fail);

    if (is_ecc_error(handle, status)) {
        NAND_LOGD(TAG, "copy, ecc error");
        return ESP_FAIL;
    }

    uint32_t src_block = src >> handle->chip.log2_ppb;
    uint32_t dst_block = dst >> handle->chip.log2_ppb;
    uint16_t src_column_addr = get_column_address(handle, src_block, 0);
    uint16_t dst_column_addr = get_column_address(handle, dst_block, 0);

    if ((src_column_addr != dst_column_addr) || (handle->chip.flags & NAND_FLAG_NO_INTERNAL_COPY)) {
        // Copy through RAM when the pages are on different planes, or when the chip needs a
        // PROGRAM LOAD between PAGE READ and PROGRAM EXECUTE (Zetta: 13h, 84h, 06h, 10h).
        copy_buf = handle->read_buffer;
        if (! copy_buf) {
            ret = ESP_ERR_NO_MEM;
            goto fail;
        }

        GOTO_ON_ERROR(spi_nand_read(handle, copy_buf, src_column_addr, handle->chip.page_size), fail);

        GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
        GOTO_ON_ERROR(spi_nand_program_load(handle, copy_buf, dst_column_addr, handle->chip.page_size), fail);
        GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);

        GOTO_ON_ERROR(program_execute_and_wait(handle, dst, &status), fail);

        if ((status & STAT_PROGRAM_FAILED) != 0) {
            NAND_LOGD(TAG, "copy, prog failed");
            handle->stats.prog_fail++;
            return ESP_ERR_NOT_FINISHED;
        }
        copy_buf = NULL;
    } else {
        GOTO_ON_ERROR(spi_nand_write_enable(handle), fail);
        GOTO_ON_ERROR(program_execute_and_wait(handle, dst, &status), fail);
        if ((status & STAT_PROGRAM_FAILED) != 0) {
            NAND_LOGD(TAG, "copy, prog failed");
            handle->stats.prog_fail++;
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
