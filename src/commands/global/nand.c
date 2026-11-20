#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "lib/bp_args/bp_cmd.h"    // Unified command parsing, validation, prompting, help, hints
#include "system_config.h"
#include "pirate/button.h"
#include "commands/global/nand.h"
#ifdef BP_HW_STORAGE_NAND
#include "hardware/spi.h"
#include "nand2/include/spi_nand_flash.h"
#include "nand2/include/nand.h"
#include "nand2/include/nand_impl.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include "nand2/nand_ftl_diskio.h"
#include "commands/global/disk.h"
#include "msc_disk.h"
#endif

static const char* const usage[] = {
    "nand [info|id|raw|regs|gc|scan|stress|move|wipe]\r\n\t[-i(terations) <count>] [-n <sectors>] [-y(es)]",
    "Show FTL info, driver counters and registers:%s nand info",
    "Dump captured READ ID bytes (works even if detection failed):%s nand id",
    "Raw READ ID on the SPI bus, bypassing the driver:%s nand raw",
    "Dump protect/feature/status registers:%s nand regs",
    "Dump registers and clear block protection:%s nand regs -y",
    "Force a full garbage collection pass:%s nand gc",
    "Read-only surface scan (verify all sectors readable):%s nand scan",
    "Destructive write/GC/verify stress test (4 cycles):%s nand stress -i 4",
    "Stress test without the confirmation prompt:%s nand stress -y",
    "Test raw page moves with and without RANDOM PROGRAM LOAD:%s nand move",
    "Page-move test without the confirmation prompt:%s nand move -y",
    "Erase all good blocks, reset the FTL and format:%s nand wipe",
};

enum nand_actions {
    NAND_INFO = 1, // enum values start at 1 (0 = no action)
    NAND_ID,
    NAND_RAW,
    NAND_REGS,
    NAND_GC,
    NAND_SCAN,
    NAND_STRESS,
    NAND_MOVE,
    NAND_WIPE,
};

static const bp_command_action_t nand_action_defs[] = {
    { NAND_INFO,   "info",   0 },
    { NAND_ID,     "id",     0 },
    { NAND_RAW,    "raw",    0 },
    { NAND_REGS,   "regs",   0 },
    { NAND_GC,     "gc",     0 },
    { NAND_SCAN,   "scan",   0 },
    { NAND_STRESS, "stress", 0 },
    { NAND_MOVE,   "move",   0 },
    { NAND_WIPE,   "wipe",   0 },
};

static const bp_val_constraint_t iterations_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 1000, .def = 4 },
    .prompt = 0,
    .hint = 0,
};

static const bp_val_constraint_t sectors_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 65535, .def = 64 },
    .prompt = 0,
    .hint = 0,
};

static const bp_command_opt_t nand_opts[] = {
    { "iterations", 'i', BP_ARG_REQUIRED, "count",   0, &iterations_range },
    { "sectors",    'n', BP_ARG_REQUIRED, "sectors", 0, &sectors_range },
    { "yes",        'y', BP_ARG_NONE,     NULL,      0 },
    { 0 }, // sentinel
};

const bp_command_def_t nand_def = {
    .name = "nand",
    .description = 0x00,
    .actions = nand_action_defs,
    .action_count = count_of(nand_action_defs),
    .opts = nand_opts,
    .usage = usage,
    .usage_count = count_of(usage),
};

#ifdef BP_HW_STORAGE_NAND

// Deterministic, seed-based pattern so the verify pass can regenerate the expected data.
static void nand_fill_pattern(uint8_t* buf, uint32_t len, uint32_t sector, uint32_t iteration) {
    uint32_t x = (sector * 2654435761u) ^ (iteration * 40503u) ^ 0xB5297A4Du;
    for (uint32_t i = 0; i < len; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        buf[i] = (uint8_t)(x >> 24);
    }
}

static spi_nand_flash_device_t* nand_get_ready_handle(void) {
    spi_nand_flash_device_t* handle = spi_nand_flash_get_handle();
    if (handle == NULL) {
        printf("Error: NAND flash not initialized. Mount storage first (try 'i').\r\n");
    }
    return handle;
}

// The FTL is shared with USB MSC on core 1, so every driver call is serialized with the disk lock.
// The lock must only be held for the duration of one call: core 1 drains the terminal TX FIFO and
// delivers keyboard input, so holding it across a printf() or a prompt deadlocks both cores as soon as
// the host touches the drive.
static esp_err_t nand_locked_write(spi_nand_flash_device_t* handle, const uint8_t* buf, uint32_t sector) {
    diskio_lock();
    esp_err_t ret = spi_nand_flash_write_sector(handle, buf, sector);
    diskio_unlock();
    return ret;
}

static esp_err_t nand_locked_read(spi_nand_flash_device_t* handle, uint8_t* buf, uint32_t sector) {
    diskio_lock();
    esp_err_t ret = spi_nand_flash_read_sector(handle, buf, sector);
    diskio_unlock();
    return ret;
}

static esp_err_t nand_locked_gc(spi_nand_flash_device_t* handle, uint32_t* steps) {
    diskio_lock();
    esp_err_t ret = spi_nand_flash_gc(handle, steps);
    diskio_unlock();
    return ret;
}

static esp_err_t nand_locked_sync(spi_nand_flash_device_t* handle) {
    diskio_lock();
    esp_err_t ret = spi_nand_flash_sync(handle);
    diskio_unlock();
    return ret;
}

static void nand_locked_recover(spi_nand_flash_device_t* handle) {
    diskio_lock();
    spi_nand_flash_recover(handle);
    diskio_unlock();
}

// Driver-independent raw READ ID directly on the shared SPI0 bus (bypasses the NAND driver).
static void nand_cmd_raw_id(void) {
    uint8_t tx[2] = { 0x9F, 0x00 };   // READ ID command + 1 address byte
    uint8_t rx[4];

    spi_busy_wait(true);
    spi_set_format(BP_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    memset(rx, 0, sizeof(rx));
    gpio_put(FLASH_STORAGE_CS, 0);
    spi_write_blocking(BP_SPI_PORT, tx, sizeof(tx));
    spi_read_blocking(BP_SPI_PORT, 0xFF, rx, sizeof(rx));
    gpio_put(FLASH_STORAGE_CS, 1);
    printf("Raw READ ID (cold):        %02X %02X %02X %02X\r\n", rx[0], rx[1], rx[2], rx[3]);

    uint8_t rst = 0xFF;
    gpio_put(FLASH_STORAGE_CS, 0);
    spi_write_blocking(BP_SPI_PORT, &rst, 1);
    gpio_put(FLASH_STORAGE_CS, 1);
    busy_wait_us_32(2000);

    memset(rx, 0, sizeof(rx));
    gpio_put(FLASH_STORAGE_CS, 0);
    spi_write_blocking(BP_SPI_PORT, tx, sizeof(tx));
    spi_read_blocking(BP_SPI_PORT, 0xFF, rx, sizeof(rx));
    gpio_put(FLASH_STORAGE_CS, 1);
    printf("Raw READ ID (after reset): %02X %02X %02X %02X\r\n", rx[0], rx[1], rx[2], rx[3]);

    spi_busy_wait(false);
}

// Raw GET FEATURES (0Fh) of a feature register on SPI0. Caller holds the SPI bus.
static uint8_t nand_raw_get_feature(uint8_t reg) {
    uint8_t tx[2] = { 0x0F, reg };
    uint8_t val = 0;
    gpio_put(FLASH_STORAGE_CS, 0);
    spi_write_blocking(BP_SPI_PORT, tx, sizeof(tx));
    spi_read_blocking(BP_SPI_PORT, 0xFF, &val, 1);
    gpio_put(FLASH_STORAGE_CS, 1);
    return val;
}

// Raw SET FEATURES (1Fh) of a feature register on SPI0. Caller holds the SPI bus.
static void nand_raw_set_feature(uint8_t reg, uint8_t val) {
    uint8_t tx[3] = { 0x1F, reg, val };
    gpio_put(FLASH_STORAGE_CS, 0);
    spi_write_blocking(BP_SPI_PORT, tx, sizeof(tx));
    gpio_put(FLASH_STORAGE_CS, 1);
}

// Dump protection/feature/status registers; with unlock=true, clear block protection (A0h=0).
static void nand_cmd_regs(bool unlock) {
    spi_busy_wait(true);
    spi_set_format(BP_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    printf("Protect (A0h): 0x%02X\r\n", nand_raw_get_feature(0xA0));
    printf("Feature (B0h): 0x%02X\r\n", nand_raw_get_feature(0xB0));
    printf("Status  (C0h): 0x%02X\r\n", nand_raw_get_feature(0xC0));

    if (unlock) {
        nand_raw_set_feature(0xA0, 0x00);
        busy_wait_us_32(100);
        printf("After unlock, Protect (A0h): 0x%02X\r\n", nand_raw_get_feature(0xA0));
    }

    spi_busy_wait(false);
}

static void nand_print_stats(spi_nand_flash_device_t* handle) {
    uint32_t used = 0, total = 0, journal = 0;
    if (spi_nand_flash_get_stats(handle, &used, &total, &journal) != ESP_OK) {
        printf("Error: Failed to read FTL statistics\r\n");
        return;
    }
    printf("  Manufacturer: %s\r\n", spi_nand_flash_print_manufacturer());
    printf("  FTL sectors used: %lu / %lu\r\n", (unsigned long)used, (unsigned long)total);
    printf("  Journal size: %lu pages\r\n", (unsigned long)journal);
    if (total) {
        printf("  Utilization: %lu%%\r\n", (unsigned long)((uint64_t)used * 100 / total));
    }
}

static void nand_cmd_info(spi_nand_flash_device_t* handle) {
    spi_nand_flash_print_info();
    nand_print_stats(handle);
    spi_nand_flash_print_diag(handle);
    msc_disk_print_diag();
}

static void nand_cmd_wipe(spi_nand_flash_device_t* handle) {
    if (!bp_cmd_confirm(&nand_def,
                        "Wipe ERASES every block of the NAND flash (factory bad-block marks are kept),\r\n"
                        "resets the flash translation layer and formats a new file system.")) {
        printf("Wipe aborted.\r\n");
        return;
    }
    printf("Erasing %s NAND...\r\n", spi_nand_flash_print_manufacturer());
    uint32_t erased = 0, bad = 0, failed = 0;
    // Lock only the erase: disk_format() re-enters the disk layer, which takes the lock itself.
    diskio_lock();
    esp_err_t ret = spi_nand_flash_wipe(handle, &erased, &bad, &failed);
    diskio_unlock();
    printf("  %lu blocks erased, %lu bad blocks skipped, %lu erase failures\r\n", (unsigned long)erased,
           (unsigned long)bad, (unsigned long)failed);
    if (ret != ESP_OK) {
        printf("Wipe FAILED: error %ld\r\n", (long)ret);
        spi_nand_flash_print_diag(handle);
        return;
    }
    system_config.storage_available = 0;
    disk_format();
}

static void nand_cmd_gc(spi_nand_flash_device_t* handle) {
    uint32_t before = 0, after = 0, steps = 0;
    spi_nand_flash_get_stats(handle, NULL, NULL, &before);
    printf("Forcing garbage collection...\r\n");
    esp_err_t ret = nand_locked_gc(handle, &steps);
    if (ret != ESP_OK) {
        printf("Garbage collection FAILED: error %ld\r\n", (long)ret);
        return;
    }
    spi_nand_flash_get_stats(handle, NULL, NULL, &after);
    printf("Garbage collection OK: %lu steps, journal %lu -> %lu pages\r\n",
           (unsigned long)steps, (unsigned long)before, (unsigned long)after);
}

static void nand_cmd_scan(spi_nand_flash_device_t* handle) {
    uint32_t total = 0, sector_size = 0;
    if (spi_nand_flash_get_capacity(handle, &total) != ESP_OK ||
        spi_nand_flash_get_sector_size(handle, &sector_size) != ESP_OK) {
        printf("Error: Failed to query NAND geometry\r\n");
        return;
    }

    static uint8_t buf[2048];
    if (sector_size > sizeof(buf)) {
        printf("Error: sector size %lu exceeds scan buffer\r\n", (unsigned long)sector_size);
        return;
    }

    printf("Scanning %lu sectors (read-only). Press the button to abort.\r\n", (unsigned long)total);
    uint32_t read_errors = 0;
    for (uint32_t s = 0; s < total; s++) {
        if (button_get(0)) {
            printf("\r\nAborted at sector %lu\r\n", (unsigned long)s);
            break;
        }
        if (nand_locked_read(handle, buf, s) != ESP_OK) {
            read_errors++;
            if (read_errors <= 16) {
                printf("  Read error at sector %lu\r\n", (unsigned long)s);
            }
        }
        if ((s & 0x3FF) == 0) {
            printf("  ... sector %lu\r\n", (unsigned long)s);
        }
    }
    printf("Scan complete: %lu read errors\r\n", (unsigned long)read_errors);
}

static bool nand_confirm_destructive(void) {
    return bp_cmd_confirm(&nand_def,
                          "Stress test DESTROYS all data on the NAND flash and file system.\r\n"
                          "Run 'format' afterwards to restore the file system.");
}

static void nand_cmd_stress(spi_nand_flash_device_t* handle, uint32_t iterations, uint32_t sectors) {
    uint32_t total = 0, sector_size = 0;
    if (spi_nand_flash_get_capacity(handle, &total) != ESP_OK ||
        spi_nand_flash_get_sector_size(handle, &sector_size) != ESP_OK) {
        printf("Error: Failed to query NAND geometry\r\n");
        return;
    }

    static uint8_t write_buf[2048];
    static uint8_t read_buf[2048];
    if (sector_size > sizeof(write_buf)) {
        printf("Error: sector size %lu exceeds stress buffer\r\n", (unsigned long)sector_size);
        return;
    }

    if (sectors == 0 || sectors > total) {
        sectors = (total < 64) ? total : 64;
    }
    if (iterations == 0) {
        iterations = 4;
    }

    if (!nand_confirm_destructive()) {
        printf("Stress test aborted.\r\n");
        return;
    }

    printf("Stress test: %lu cycles over %lu sectors of %lu bytes\r\n",
           (unsigned long)iterations, (unsigned long)sectors, (unsigned long)sector_size);

    uint32_t total_write_errors = 0;
    uint32_t total_read_errors = 0;
    uint32_t total_mismatches = 0;

    for (uint32_t it = 0; it < iterations; it++) {
        if (button_get(0)) {
            printf("Aborted before cycle %lu\r\n", (unsigned long)it);
            break;
        }

        uint32_t write_errors = 0, read_errors = 0, mismatches = 0, gc_steps = 0;

        // write phase
        for (uint32_t s = 0; s < sectors; s++) {
            nand_fill_pattern(write_buf, sector_size, s, it);
            if (nand_locked_write(handle, write_buf, s) != ESP_OK) {
                write_errors++;
            }
        }

        // force garbage collection between write and verify to exercise page relocation
        esp_err_t gc_ret = nand_locked_gc(handle, &gc_steps);
        if (gc_ret != ESP_OK) {
            printf("  Cycle %lu: GC FAILED (error %ld)\r\n", (unsigned long)it, (long)gc_ret);
        }

        // verify phase
        for (uint32_t s = 0; s < sectors; s++) {
            if (nand_locked_read(handle, read_buf, s) != ESP_OK) {
                read_errors++;
                continue;
            }
            nand_fill_pattern(write_buf, sector_size, s, it);
            if (memcmp(write_buf, read_buf, sector_size) != 0) {
                mismatches++;
                if (mismatches <= 8) {
                    printf("  Cycle %lu: DATA MISMATCH at sector %lu\r\n",
                           (unsigned long)it, (unsigned long)s);
                }
            }
        }

        nand_locked_sync(handle);

        printf("  Cycle %lu: gc %lu steps, %lu write err, %lu read err, %lu mismatch\r\n",
               (unsigned long)it, (unsigned long)gc_steps, (unsigned long)write_errors,
               (unsigned long)read_errors, (unsigned long)mismatches);

        total_write_errors += write_errors;
        total_read_errors += read_errors;
        total_mismatches += mismatches;
    }

    printf("\r\nStress test complete:\r\n");
    printf("  Write errors:   %lu\r\n", (unsigned long)total_write_errors);
    printf("  Read errors:    %lu\r\n", (unsigned long)total_read_errors);
    printf("  Data mismatches:%lu\r\n", (unsigned long)total_mismatches);
    if (total_write_errors == 0 && total_read_errors == 0 && total_mismatches == 0) {
        printf("  RESULT: PASS - garbage collection preserved all data\r\n");
    } else {
        printf("  RESULT: FAIL - data integrity errors detected\r\n");
    }
    printf("File system was overwritten. Run 'format' before using storage.\r\n");
    system_config.storage_available = 0;
}

typedef struct {
    const char* stage;
    esp_err_t result;
} nand_move_case_result_t;

static nand_move_case_result_t nand_run_move_case(spi_nand_flash_device_t* handle,
                                                  uint32_t src,
                                                  uint32_t dst,
                                                  bool require_program_load,
                                                  uint8_t* write_buf,
                                                  uint8_t* read_buf) {
    uint32_t original_flags = handle->chip.flags;
    nand_move_case_result_t result = { "program source", ESP_OK };

    if (require_program_load) {
        handle->chip.flags |= NAND_FLAG_COPY_REQUIRES_PROGRAM_LOAD;
    } else {
        handle->chip.flags &= ~NAND_FLAG_COPY_REQUIRES_PROGRAM_LOAD;
    }

    nand_fill_pattern(write_buf, handle->chip.page_size, src, 0);
    result.result = nand_prog(handle, src, write_buf);
    if (result.result != ESP_OK) {
        goto done;
    }

    result.stage = "copy";
    result.result = nand_copy(handle, src, dst);
    if (result.result != ESP_OK) {
        goto done;
    }

    result.stage = "read destination";
    result.result = nand_read(handle, dst, 0, handle->chip.page_size, read_buf);
    if (result.result != ESP_OK) {
        goto done;
    }

    result.stage = "verify";
    if (memcmp(write_buf, read_buf, handle->chip.page_size) != 0) {
        result.result = ESP_FAIL;
    }

done:
    handle->chip.flags = original_flags;
    return result;
}

static void nand_cmd_move_test(spi_nand_flash_device_t* handle) {
    if (!bp_cmd_confirm(&nand_def,
                        "Raw page move test ERASES a physical NAND block and bypasses the file system.\r\n"
                        "Its data is destroyed. Run 'nand wipe' or 'format' afterwards.")) {
        printf("Move test aborted.\r\n");
        return;
    }

    if (handle->chip.page_size > 2048) {
        printf("Move test FAILED: page size %lu exceeds test buffers\r\n", (unsigned long)handle->chip.page_size);
        return;
    }

    uint8_t* write_buf = handle->work_buffer;
    uint8_t* read_buf = handle->read_buffer;
    uint32_t block = 0;
    bool found_block = false;
    bool block_erased = false;
    nand_move_case_result_t direct = { "erase", ESP_OK };
    nand_move_case_result_t random_load = { "not run", ESP_ERR_NOT_SUPPORTED };
    uint32_t original_flags = handle->chip.flags;
    uint32_t pages_per_block = 1u << handle->chip.log2_ppb;

    printf("Testing internal page moves on block near the end of the NAND.\r\n");
    diskio_lock();
    for (uint32_t candidate = handle->chip.num_blocks; candidate > 0;) {
        candidate--;
        bool is_bad = false;
        direct.result = nand_is_bad(handle, candidate, &is_bad);
        if (direct.result == ESP_OK && !is_bad) {
            block = candidate;
            found_block = true;
            break;
        }
    }

    if (!found_block) {
        direct.stage = "find good block";
        direct.result = ESP_ERR_NOT_FOUND;
        goto done;
    }

    direct.stage = "erase";
    direct.result = nand_erase_block(handle, block);
    if (direct.result != ESP_OK) {
        goto done;
    }
    block_erased = true;

    uint32_t first_page = block << handle->chip.log2_ppb;
    direct = nand_run_move_case(handle, first_page, first_page + 1, false, write_buf, read_buf);
    random_load =
        nand_run_move_case(handle, first_page + pages_per_block / 2, first_page + pages_per_block / 2 + 1,
                           true, write_buf, read_buf);

done:
    handle->chip.flags = original_flags;
    diskio_unlock();

    printf("  Direct 13h -> 10h: %s (%s, error %ld)\r\n",
           direct.result == ESP_OK ? "PASS" : "FAIL",
           direct.stage,
           (long)direct.result);
    printf("  Forced 84h random load: %s (%s, error %ld)\r\n",
           random_load.result == ESP_OK ? "PASS" : "FAIL",
           random_load.stage,
           (long)random_load.result);
    printf("  Original copy flag: %s\r\n",
           (original_flags & NAND_FLAG_COPY_REQUIRES_PROGRAM_LOAD) ? "set" : "clear");
    printf("  RESULT: %s\r\n", direct.result == ESP_OK && random_load.result == ESP_OK
                               ? "PASS - both paths preserved the page"
                               : "FAIL - inspect chip timing/cache requirements");
    if (block_erased) {
        printf("Physical block %lu was erased. Run 'format' or 'nand wipe' before using storage.\r\n",
               (unsigned long)block);
        system_config.storage_available = 0;
    }
}

#endif // BP_HW_STORAGE_NAND

void nand_handler(struct command_result* res) {
    if (bp_cmd_help_check(&nand_def, res->help_flag)) {
        return;
    }

#ifndef BP_HW_STORAGE_NAND
    printf("This hardware does not have NAND flash storage.\r\n");
    return;
#else
    uint32_t action = NAND_INFO;
    if (!bp_cmd_get_action(&nand_def, &action)) {
        action = NAND_INFO;
    }

    // id/raw/regs are hardware diagnostics that work even when detection failed (no handle required).
    switch (action) {
        case NAND_ID: {
            const uint8_t* id = spi_nand_flash_get_read_id_debug();
            for (uint8_t r = 0; r < 3; r++) {
                printf("READ ID addr_bytes=%u:", r);
                for (uint8_t c = 0; c < 6; c++) {
                    printf(" %02X", id[r * 6 + c]);
                }
                printf("\r\n");
            }
            return;
        }
        case NAND_RAW:
            nand_cmd_raw_id();
            return;
        case NAND_REGS:
            nand_cmd_regs(bp_cmd_find_flag(&nand_def, 'y'));
            return;
        default:
            break;
    }

    spi_nand_flash_device_t* handle = nand_get_ready_handle();
    if (handle == NULL) {
        return;
    }

    switch (action) {
        case NAND_GC:
            nand_locked_recover(handle); // clear a latched fault so the test actually runs
            nand_cmd_gc(handle);
            break;
        case NAND_SCAN:
            nand_locked_recover(handle);
            nand_cmd_scan(handle);
            break;
        case NAND_STRESS: {
            uint32_t iterations = 4, sectors = 64;
            if (bp_cmd_flag(&nand_def, 'i', &iterations) == BP_CMD_INVALID ||
                bp_cmd_flag(&nand_def, 'n', &sectors) == BP_CMD_INVALID) {
                system_config.error = true;
                return;
            }
            nand_locked_recover(handle);
            nand_cmd_stress(handle, iterations, sectors);
            break;
        }
        case NAND_MOVE:
            nand_locked_recover(handle);
            nand_cmd_move_test(handle);
            break;
        case NAND_WIPE:
            nand_cmd_wipe(handle);
            break;
        case NAND_INFO:
        default:
            nand_cmd_info(handle);
            break;
    }
#endif
}
