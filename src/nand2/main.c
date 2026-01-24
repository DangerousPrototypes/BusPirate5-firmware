/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * SPDX-FileContributor: Adapted for Raspberry Pi Pico (RP2040)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "spi_nand_flash.h"

// Pin mapping for Raspberry Pi Pico
// Using SPI0
#define SPI_PORT spi0
#define PIN_MISO 16  // GP16 - SPI0 RX
#define PIN_CS   26  // GP17 - Chip Select (directly controlled)
#define PIN_CLK  18  // GP18 - SPI0 SCK
#define PIN_MOSI 19  // GP19 - SPI0 TX

// Flash frequency in Hz (40 MHz)
#define FLASH_FREQ_HZ 10000000

static const char *TAG = "example";

// Logging macros
#define LOG_I(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_E(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)

void read_id(){
    // Function to read and print the NAND flash ID
    gpio_put(PIN_CS, 0); // Select the chip
    uint8_t cmd = 0x9F; // Read ID command
    spi_write_blocking(SPI_PORT, &cmd, 1);
    uint8_t id[3] = {0};
    spi_read_blocking(SPI_PORT, 0xff, id, 3);
    gpio_put(PIN_CS, 1); // Deselect the chip
    LOG_I(TAG, "NAND Flash ID: %02X %02X", id[1], id[2]);
}


static void example_init_spi(void)
{
    // Initialize SPI port at specified frequency
    spi_init(SPI_PORT, FLASH_FREQ_HZ);

    // Set SPI format: 8 bits, mode 0 (CPOL=0, CPHA=0)
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Initialize GPIO pins for SPI
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CLK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Initialize CS pin as GPIO (directly controlled)
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);  // Deselect

    //shared SPI devices to CS high
    gpio_init(20);
    gpio_set_dir(20, GPIO_OUT);
    gpio_put(20, 0);
    gpio_init(21);
    gpio_set_dir(21, GPIO_OUT);
    gpio_put(21, 1);
    gpio_init(37);
    gpio_set_dir(37, GPIO_OUT);
    gpio_put(37, 1);

    LOG_I(TAG, "SPI initialized at %d Hz", spi_get_baudrate(SPI_PORT));

    for(int i=0;i<5;i++){
        read_id();
    }

    
}

static void example_deinit_spi(void)
{
    spi_deinit(SPI_PORT);
}

static void example_init_nand_flash(spi_nand_flash_device_t **out_handle)
{
    spi_nand_flash_config_t nand_flash_config = {
        .spi = SPI_PORT,
        .cs_pin = PIN_CS,
        .io_mode = SPI_NAND_IO_MODE_SIO,
        .gc_factor = 4, //same as previous NAND driver
    };

    spi_nand_flash_device_t *nand_flash_device_handle;
    esp_err_t ret = spi_nand_flash_init_device(&nand_flash_config, &nand_flash_device_handle);
    if (ret != ESP_OK) {
        LOG_E(TAG, "Failed to initialize NAND flash: %d", ret);
        *out_handle = NULL;
        return;
    }

    *out_handle = nand_flash_device_handle;
    LOG_I(TAG, "NAND flash initialized successfully");
}

static void example_deinit_nand_flash(spi_nand_flash_device_t *flash)
{
    spi_nand_flash_deinit_device(flash);
}

int main(void)
{
    // Initialize stdio for USB serial output
    stdio_init_all();

    // Wait for USB serial connection (optional, for debugging)
    sleep_ms(2000);

    LOG_I(TAG, "SPI NAND Flash Example for Raspberry Pi Pico");
    LOG_I(TAG, "==============================================");

    // Initialize SPI bus
    example_init_spi();

    // Initialize the external SPI Flash chip
    spi_nand_flash_device_t *flash;
    example_init_nand_flash(&flash);

    if (flash == NULL) {
        LOG_E(TAG, "Failed to initialize NAND flash");
        example_deinit_spi();
        return 1;
    }

    // Get flash information
    uint32_t num_blocks, block_size, sector_size, capacity;
    spi_nand_flash_get_block_num(flash, &num_blocks);
    spi_nand_flash_get_block_size(flash, &block_size);
    spi_nand_flash_get_sector_size(flash, &sector_size);
    spi_nand_flash_get_capacity(flash, &capacity);

    LOG_I(TAG, "Flash Info:");
    LOG_I(TAG, "  Number of blocks: %" PRIu32, num_blocks);
    LOG_I(TAG, "  Block size: %" PRIu32 " bytes", block_size);
    LOG_I(TAG, "  Sector size: %" PRIu32 " bytes", sector_size);
    LOG_I(TAG, "  Total capacity: %" PRIu32 " sectors", capacity);
    LOG_I(TAG, "  Total size: %" PRIu32 " KB", (capacity * sector_size) / 1024);
#if 0
    // Allocate buffers for read/write operations
    uint8_t *write_buffer = malloc(sector_size);
    uint8_t *read_buffer = malloc(sector_size);

    if (!write_buffer || !read_buffer) {
        LOG_E(TAG, "Failed to allocate buffers");
        free(write_buffer);
        free(read_buffer);
        example_deinit_nand_flash(flash);
        example_deinit_spi();
        return 1;
    }

    // Fill write buffer with test pattern
    const char *test_message = "Hello from Raspberry Pi Pico!";
    memset(write_buffer, 0, sector_size);
    memcpy(write_buffer, test_message, strlen(test_message) + 1);

    // Write test pattern to sector 0
    LOG_I(TAG, "Writing test data to sector 0...");
    esp_err_t ret = spi_nand_flash_write_sector(flash, write_buffer, 0);
    if (ret != ESP_OK) {
        LOG_E(TAG, "Write failed: %d", ret);
    } else {
        LOG_I(TAG, "Write successful");
    }

    // Sync to ensure data is written
    spi_nand_flash_sync(flash);
#endif
     uint8_t read_buffer[2048];
    // Read back the data
    LOG_I(TAG, "Reading data from sector 0...");
    memset(read_buffer, 0, sector_size);
    esp_err_t ret = spi_nand_flash_read_sector(flash, read_buffer, 0);
    if (ret != ESP_OK) {
        LOG_E(TAG, "Read failed: %d", ret);
    } else {
        LOG_I(TAG, "Read successful");
        for(uint32_t i=0;i<2048;i++){
            printf("%02X ",read_buffer[i]);
        }
        printf("\n");
        //LOG_I(TAG, "Read data: '%s'", (char *)read_buffer);
#if 0
        // Verify the data
        if (memcmp(write_buffer, read_buffer, strlen(test_message) + 1) == 0) {
            LOG_I(TAG, "Data verification: PASSED");
        } else {
            LOG_E(TAG, "Data verification: FAILED");
        }
#endif
    }

    // Deinitialize
    example_deinit_nand_flash(flash);
    example_deinit_spi();

    LOG_I(TAG, "Example complete!");

    // Keep the program running
    while (1) {
        tight_loop_contents();
    }

    return 0;
}
