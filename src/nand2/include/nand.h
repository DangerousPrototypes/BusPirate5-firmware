/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: Adapted for Raspberry Pi Pico (RP2040)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "spi_nand_flash.h"
#include "pico/mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_PAGE 0xFFFF

#define NAND_FLAG_HAS_PROG_PLANE_SELECT       (1 << 0)
#define NAND_FLAG_HAS_READ_PLANE_SELECT       (1 << 1)
// Chip does not support PAGE READ -> PROGRAM EXECUTE copy-back without an intervening PROGRAM LOAD;
// copies are routed through RAM (13h, 0Bh, 84h, 06h, 10h) instead.
#define NAND_FLAG_NO_INTERNAL_COPY            (1 << 2)

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

typedef enum {
    STAT_ECC_OK = 0,
    STAT_ECC_1_TO_3_BITS_CORRECTED = 1,
    STAT_ECC_BITS_CORRECTED = STAT_ECC_1_TO_3_BITS_CORRECTED,
    STAT_ECC_NOT_CORRECTED = 2,
    STAT_ECC_4_TO_6_BITS_CORRECTED = 3,
    STAT_ECC_MAX_BITS_CORRECTED = STAT_ECC_4_TO_6_BITS_CORRECTED,
    STAT_ECC_7_8_BITS_CORRECTED = 5,
    STAT_ECC_MAX
} ecc_status_t;

typedef struct {
    uint8_t ecc_status_reg_len_in_bits;
    uint8_t ecc_data_refresh_threshold;
    ecc_status_t ecc_corrected_bits_status;
} ecc_data_t;

typedef struct {
    uint8_t log2_page_size; //is power of 2, log2_page_size shift (1<<log2_page_size) is stored to page_size
    uint8_t log2_ppb;       //is power of 2, log2_ppb shift ((1<<log2_ppb) * page_size) will be stored in block size
    uint32_t block_size;
    uint32_t page_size;
    uint32_t num_blocks;
    uint32_t read_page_delay_us;
    uint32_t erase_block_delay_us;
    uint32_t program_page_delay_us;
    uint32_t num_planes;
    uint32_t flags;
    ecc_data_t ecc_data;
    uint8_t has_quad_enable_bit;    // Not used on PICO (no quad SPI support in standard mode)
    uint8_t quad_enable_bit_pos;
} spi_nand_chip_t;

typedef struct {
    esp_err_t (*init)(spi_nand_flash_device_t *handle);
    esp_err_t (*deinit)(spi_nand_flash_device_t *handle);
    esp_err_t (*read)(spi_nand_flash_device_t *handle, uint8_t *buffer, uint32_t sector_id);
    esp_err_t (*write)(spi_nand_flash_device_t *handle, const uint8_t *buffer, uint32_t sector_id);
    esp_err_t (*erase_chip)(spi_nand_flash_device_t *handle);
    esp_err_t (*erase_block)(spi_nand_flash_device_t *handle, uint32_t block);
    esp_err_t (*trim)(spi_nand_flash_device_t *handle, uint32_t sector_id);
    esp_err_t (*sync)(spi_nand_flash_device_t *handle);
    esp_err_t (*copy_sector)(spi_nand_flash_device_t *handle, uint32_t src_sec, uint32_t dst_sec);
    esp_err_t (*get_capacity)(spi_nand_flash_device_t *handle, uint32_t *number_of_sectors);
} spi_nand_ops;

// Diagnostic counters, cumulative since init (reported by the 'nand info' command).
typedef struct {
    uint32_t reads;
    uint32_t progs;
    uint32_t erases;
    uint32_t copies;
    uint32_t prog_fail;        // PROGRAM EXECUTE reported P_FAIL
    uint32_t erase_fail;       // BLOCK ERASE reported E_FAIL
    uint32_t ecc_corrected;    // reads with corrected bit errors
    uint32_t ecc_uncorrectable;// reads with uncorrectable ECC errors
    uint32_t marked_bad;       // blocks marked bad by the FTL
    uint32_t busy_timeouts;    // OIP never cleared within the busy timeout
    uint32_t deadline_hits;    // a single FTL operation exceeded NAND_OP_DEADLINE_MS
    int32_t last_dhara_err;    // last dhara_error_t returned to the FTL layer, -1 if none
} spi_nand_stats_t;

struct spi_nand_flash_device_t {
    spi_nand_flash_config_t config;
    spi_nand_chip_t chip;
    const spi_nand_ops *ops;
    void *ops_priv_data;
    uint8_t *work_buffer;
    uint8_t *read_buffer;
    uint8_t *temp_buffer;
    const char *manufacturer_name;
    mutex_t mutex;
    bool io_fault; // latched after a busy timeout or deadline: fail fast until recovered
    // Wall-clock limit for the FTL operation in progress (see nand_op_begin). Dhara's bad-block
    // recovery retries are bounded per call but can chain across the whole chip; this makes any
    // single sector read/write/gc give up and report an error instead of appearing to hang.
    absolute_time_t op_deadline;
    spi_nand_stats_t stats;
};

// Maximum wall-clock time one FTL-level operation (sector read/write, gc step, sync, resume) may spend
// in NAND I/O before io_fault is latched and the operation fails.
#define NAND_OP_DEADLINE_MS 20000

esp_err_t nand_register_dev(spi_nand_flash_device_t *handle);
esp_err_t nand_unregister_dev(spi_nand_flash_device_t *handle);

#ifdef __cplusplus
}
#endif
