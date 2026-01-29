/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: Adapted for Raspberry Pi Pico (RP2040)
 */

#pragma once

#include <stdint.h>
#include "hardware/spi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct spi_nand_flash_device_t spi_nand_flash_device_t;

/** @brief SPI mode used for reading from SPI NAND Flash */
typedef enum {
    SPI_NAND_IO_MODE_SIO = 0,
    // Note: Multi-IO modes are not supported on RP2040 standard SPI
    // SPI_NAND_IO_MODE_DOUT,
    // SPI_NAND_IO_MODE_DIO,
    // SPI_NAND_IO_MODE_QOUT,
    // SPI_NAND_IO_MODE_QIO,
} spi_nand_flash_io_mode_t;

/** @brief PICO SPI NAND configuration structure */
struct spi_nand_flash_config_t {
    spi_inst_t *spi;            ///< SPI instance (spi0 or spi1)
    uint8_t cs_pin;             ///< Chip select GPIO pin
    uint8_t gc_factor;          ///< The gc factor controls the number of blocks to spare block ratio.
                                ///< Lower values will reduce the available space but increase performance
    spi_nand_flash_io_mode_t io_mode;  ///< IO mode (only SIO supported on PICO)
};

typedef struct spi_nand_flash_config_t spi_nand_flash_config_t;

/** @brief Error codes compatible with ESP-IDF style */
typedef int32_t esp_err_t;

#define ESP_OK          0       ///< Success
#define ESP_FAIL        -1      ///< General failure
#define ESP_ERR_NO_MEM          0x101   ///< Out of memory
#define ESP_ERR_INVALID_ARG     0x102   ///< Invalid argument
#define ESP_ERR_INVALID_STATE   0x103   ///< Invalid state
#define ESP_ERR_INVALID_SIZE    0x104   ///< Invalid size
#define ESP_ERR_NOT_FOUND       0x105   ///< Not found
#define ESP_ERR_NOT_SUPPORTED   0x106   ///< Not supported
#define ESP_ERR_TIMEOUT         0x107   ///< Timeout
#define ESP_ERR_INVALID_RESPONSE 0x108  ///< Invalid response
#define ESP_ERR_NOT_FINISHED    0x109   ///< Operation not finished
#define ESP_ERR_FLASH_BASE      0x6000  ///< Flash error base

/** @brief Initialise SPI nand flash chip interface.
 *
 * This function must be called before calling any other API functions for the nand flash.
 *
 * @param config Pointer to SPI nand flash config structure.
 * @param[out] handle The handle to the SPI nand flash chip is returned in this variable.
 * @return ESP_OK on success, or a flash error code if the initialisation failed.
 */
esp_err_t spi_nand_flash_init_device(spi_nand_flash_config_t *config, spi_nand_flash_device_t **handle);

/** @brief Read a sector from the nand flash.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] buffer The output buffer to put the read data into.
 * @param sector_id The id of the sector to read.
 * @return ESP_OK on success, or a flash error code if the read failed.
 */
esp_err_t spi_nand_flash_read_sector(spi_nand_flash_device_t *handle, uint8_t *buffer, uint32_t sector_id);

/** @brief Copy a sector to another sector from the nand flash.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param src_sec The source sector id from which data to be copied.
 * @param dst_sec The destination sector id to which data should be copied.
 * @return ESP_OK on success, or a flash error code if the copy failed.
 */
esp_err_t spi_nand_flash_copy_sector(spi_nand_flash_device_t *handle, uint32_t src_sec, uint32_t dst_sec);

/** @brief Write a sector to the nand flash.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] buffer The input buffer containing the data to write.
 * @param sector_id The id of the sector to write.
 * @return ESP_OK on success, or a flash error code if the write failed.
 */
esp_err_t spi_nand_flash_write_sector(spi_nand_flash_device_t *handle, const uint8_t *buffer, uint32_t sector_id);

/** @brief Trim sector from the nand flash.
 *
 * This function marks specified sector as free to optimize memory usage
 * and support wear-leveling.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param sector_id The id of the sector to be trimmed.
 * @return ESP_OK on success, or a flash error code if the trim failed.
 */
esp_err_t spi_nand_flash_trim(spi_nand_flash_device_t *handle, uint32_t sector_id);

/** @brief Synchronizes any cache to the device.
 *
 * After this method is called, the nand flash chip should be synchronized with the results of any previous read/writes.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @return ESP_OK on success, or a flash error code if the synchronization failed.
 */
esp_err_t spi_nand_flash_sync(spi_nand_flash_device_t *handle);

/** @brief Retrieve the number of sectors available.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] number_of_sectors A pointer of where to put the return value
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_get_capacity(spi_nand_flash_device_t *handle, uint32_t *number_of_sectors);

/** @brief Retrieve the size of each sector.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] sectors_size A pointer of where to put the return value
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_get_sector_size(spi_nand_flash_device_t *handle, uint32_t *sector_size);

/** @brief Retrieve the size of each block.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] block_size A pointer of where to put the return value
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_get_block_size(spi_nand_flash_device_t *handle, uint32_t *block_size);

/** @brief Erases the entire chip, invalidating any data on the chip.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @return ESP_OK on success, or a flash error code if the erase failed.
 */
esp_err_t spi_nand_erase_chip(spi_nand_flash_device_t *handle);

/** @brief Retrieve the number of blocks available.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @param[out] number_of_blocks A pointer of where to put the return value
 * @return ESP_OK on success, or a flash error code if the operation failed.
 */
esp_err_t spi_nand_flash_get_block_num(spi_nand_flash_device_t *handle, uint32_t *number_of_blocks);

/** @brief De-initialize the handle, releasing any resources reserved.
 *
 * @param handle The handle to the SPI nand flash chip.
 * @return ESP_OK on success, or a flash error code if the de-initialization failed.
 */
esp_err_t spi_nand_flash_deinit_device(spi_nand_flash_device_t *handle);

/** @brief Print NAND flash information including manufacturer name.
 *
 * Uses the internal static device handle, so no parameters needed.
 */
void spi_nand_flash_print_info(void);

/** @brief Get NAND flash manufacturer name.
 *
 * Uses the internal static device handle, so no parameters needed.
 *
 * @return Pointer to manufacturer name string, or empty string if no device initialized.
 */
const char *spi_nand_flash_print_manufacturer(void);

#ifdef __cplusplus
}
#endif
