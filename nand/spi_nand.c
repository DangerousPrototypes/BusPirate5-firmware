/**
 * @file		spi_nand.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the spi nand module
 *
 */



//#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "spi_nand.h"
#include "spi.h"
#include "nand/sys_time.h"

// defines
#define CSEL_PORT GPIOB
#define CSEL_PIN  LL_GPIO_PIN_0

#define RESET_DELAY 2    // ms
#define OP_TIMEOUT  3000 // ms

#define CMD_RESET                    0xFF
#define CMD_READ_ID                  0x9F
#define CMD_SET_FEATURE              0x1F
#define CMD_GET_FEATURE              0x0F
#define CMD_PAGE_READ                0x13
#define CMD_READ_FROM_CACHE          0x03
#define CMD_WRITE_ENABLE             0x06
#define CMD_PROGRAM_LOAD             0x02
#define CMD_PROGRAM_LOAD_RANDOM_DATA 0x84
#define CMD_PROGRAM_EXECUTE          0x10
#define CMD_BLOCK_ERASE              0xD8

#define READ_ID_TRANS_LEN    4
#define READ_ID_MFR_INDEX    2
#define READ_ID_DEVICE_INDEX 3
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// See also: spi_nand.h, which contains external per-chip guards

/// @brief Since only support two chips, can differentiate them simply
///        using only the DeviceID.
typedef struct _nand_identity {
    uint8_t Manufacturer;
    uint8_t DeviceID;
} nand_identity_t;

/// @brief Only two chips supported.  List only the parts that differ for now.
///        Can later make this more dynamic by reading the parameter from the chip,
///        and more thoroughly validating the chip's identity.
typedef struct _supported_nand_device {
    nand_identity_t Identity;
    /// @brief Log2 of the number of planes in the device
    uint8_t Log2_plane_count;
    /// @brief Count of planes in the device ( === 2^Log2_plane_count )
    uint8_t Plane_count;
    /// @brief Mask to get plane from block ( === (2^Log2_plane_count) - 1 )
    uint8_t Plane_mask;
    /// @brief size, in bytes, of extra data per sector / block (typically used for ECC)
    uint8_t Oob_size;
} supported_nand_device_t;
static const supported_nand_device_t * g_Actual_Nand_Device = NULL; // stays NULL until set by `spi_nand_init()`
static const supported_nand_device_t bp5_supported_nand[] = {
    { .Identity = { .Manufacturer = 0x2C, .DeviceID = 0x14 },  .Log2_plane_count = 0, .Plane_count = 1, .Plane_mask = 0, .Oob_size =  64, },
    { .Identity = { .Manufacturer = 0x2C, .DeviceID = 0x24 },  .Log2_plane_count = 1, .Plane_count = 2, .Plane_mask = 1, .Oob_size = 128, },
};

#define SPI_NAND_LARGEST_OOB_SUPPORTED       128 // so can allocate buffer large enough for any supported NAND chip
#define SPI_NAND_ERASE_BLOCKS_PER_PLANE     1024 // both supported devices have the same count per plane...

static inline uint8_t SPI_NAND_OOB_SIZE() {
    assert(g_Actual_Nand_Device != NULL);
    return g_Actual_Nand_Device->Oob_size;
}
static inline uint32_t SPI_NAND_PLANE_COUNT() {
    assert(g_Actual_Nand_Device != NULL);
    return g_Actual_Nand_Device->Plane_count;
}
static inline uint32_t SPI_NAND_ERASE_BLOCKS_PER_LUN() {
    assert(g_Actual_Nand_Device != NULL);
    return g_Actual_Nand_Device->Plane_count * SPI_NAND_ERASE_BLOCKS_PER_PLANE;
}
static inline uint32_t SPI_NAND_MAX_BLOCK_ADDRESS() {
    return SPI_NAND_ERASE_BLOCKS_PER_LUN() - 1;
}
#define SPI_NAND_MAX_PAGE_ADDRESS  (SPI_NAND_PAGES_PER_ERASE_BLOCK - 1) // zero-indexed

static inline uint8_t get_plane(row_address_t row) {
    assert(g_Actual_Nand_Device != NULL);
    return row.block & g_Actual_Nand_Device->Plane_mask;
}
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^



#define FEATURE_TRANS_LEN  3
#define FEATURE_REG_INDEX  1
#define FEATURE_DATA_INDEX 2

#define PAGE_READ_TRANS_LEN                4
#define READ_FROM_CACHE_TRANS_LEN          4
#define PROGRAM_LOAD_TRANS_LEN             3
#define PROGRAM_LOAD_RANDOM_DATA_TRANS_LEN 3
#define PROGRAM_EXECUTE_TRANS_LEN          4
#define BLOCK_ERASE_TRANS_LEN              4

#define FEATURE_REG_STATUS        0xC0
#define FEATURE_REG_BLOCK_LOCK    0xA0
#define FEATURE_REG_CONFIGURATION 0xB0
#define FEATURE_REG_DIE_SELECT    0xC0

#define ECC_STATUS_NO_ERR         0b000
#define ECC_STATUS_1_3_NO_REFRESH 0b001
#define ECC_STATUS_4_6_REFRESH    0b011
#define ECC_STATUS_7_8_REFRESH    0b101
#define ECC_STATUS_NOT_CORRECTED  0b010

#define BAD_BLOCK_MARK 0

// private types
typedef union {
    uint8_t whole;
    struct {
        uint8_t : 1;
        uint8_t WP_HOLD_DISABLE : 1;
        uint8_t TB : 1;
        uint8_t BP0 : 1;
        uint8_t BP1 : 1;
        uint8_t BP2 : 1;
        uint8_t BP3 : 1;
        uint8_t BRWD : 1;
    };
} feature_reg_block_lock_t;

typedef union {
    uint8_t whole;
    struct {
        uint8_t : 1;
        uint8_t CFG0 : 1;
        uint8_t : 2;
        uint8_t ECC_EN : 1;
        uint8_t LOT_EN : 1;
        uint8_t CFG1 : 1;
        uint8_t CFG2 : 1;
    };
} feature_reg_configuration_t;

typedef union {
    uint8_t whole;
    struct {
        uint8_t OIP : 1;
        uint8_t WEL : 1;
        uint8_t E_FAIL : 1;
        uint8_t P_FAIL : 1;
        uint8_t ECCS0_3 : 3;
        uint8_t CRBSY : 1;
    };
} feature_reg_status_t;

typedef union {
    uint8_t whole;
    struct {
        uint8_t : 6;
        uint8_t DS0 : 1;
        uint8_t : 1;
    };
} feature_reg_die_select_t;

// private function prototypes
static void csel_setup(void);
static void csel_deselect(void);
static void csel_select(void);

static int spi_nand_reset(void);
static int read_id(nand_identity_t * identity_out);
static int set_feature(uint8_t reg, uint8_t data, uint32_t timeout);
static int get_feature(uint8_t reg, uint8_t *data_out, uint32_t timeout);
static int write_enable(uint32_t timeout);
static int page_read(row_address_t row, uint32_t timeout);
static int read_from_cache(row_address_t row, column_address_t column, void *data_out, size_t read_len,
                           uint32_t timeout);
static int program_load(row_address_t row, column_address_t column, const void *data_in, size_t write_len,
                        uint32_t timeout);
static int program_load_random_data(row_address_t row, column_address_t column, void *data_in, size_t write_len,
                                    uint32_t timeout);

static int program_execute(row_address_t row, uint32_t timeout);
static int block_erase(row_address_t row, uint32_t timeout);

static int unlock_all_blocks(void);
static int enable_ecc(void);
static int poll_for_oip_clear(feature_reg_status_t *status_out, uint32_t timeout);

static bool validate_row_address(row_address_t row);
static bool validate_column_address(column_address_t address);
static int get_ret_from_ecc_status(feature_reg_status_t status);

// private variables
// this buffer is needed for is_free, we don't want to allocate this on the stack
uint8_t page_main_and_largest_oob_buffer[SPI_NAND_PAGE_SIZE + SPI_NAND_LARGEST_OOB_SUPPORTED];

// public function definitions
int spi_nand_init(struct dhara_nand * dhara_parameters_out)
{
    memset(dhara_parameters_out, 0, sizeof(struct dhara_nand));

    // initialize chip select
    csel_deselect();
    csel_setup();

    // reset
    //sys_time_delay(RESET_DELAY);
    busy_wait_ms(RESET_DELAY);
    int ret = spi_nand_reset();
    if (SPI_NAND_RET_OK != ret) return ret;
    //sys_time_delay(RESET_DELAY);
    busy_wait_ms(RESET_DELAY);

    // read id
    nand_identity_t chip_id;
    ret = read_id(&chip_id);
    if (SPI_NAND_RET_OK != ret) return ret;

    // Check for a supported chip and set the dhara parameters accordingly
    for (int i = 0; i < count_of(bp5_supported_nand); ++i) {
        if ((bp5_supported_nand[i].Identity.Manufacturer == chip_id.Manufacturer) &&
            (bp5_supported_nand[i].Identity.DeviceID == chip_id.DeviceID)) {
            g_Actual_Nand_Device = &bp5_supported_nand[i];
            break; // out of for loop
        }
    }
    if (g_Actual_Nand_Device == NULL) {
        return SPI_NAND_RET_DEVICE_ID;
    }

    // unlock all blocks
    ret = unlock_all_blocks();
    if (SPI_NAND_RET_OK != ret) return ret;

    // enable ecc
    ret = enable_ecc();
    if (SPI_NAND_RET_OK != ret) return ret;

    // fill in the return structure
    dhara_parameters_out->log2_page_size = SPI_NAND_LOG2_PAGE_SIZE;
    dhara_parameters_out->log2_ppb       = SPI_NAND_LOG2_PAGES_PER_ERASE_BLOCK;
    dhara_parameters_out->num_blocks     = SPI_NAND_ERASE_BLOCKS_PER_LUN();

    return ret;
}

int spi_nand_page_read(row_address_t row, column_address_t column, void *data_out,
                       size_t read_len)
{
    // input validation
    if (!validate_row_address(row) || !validate_column_address(column)) {
        return SPI_NAND_RET_BAD_ADDRESS;
    }
    uint16_t max_read_len = (SPI_NAND_PAGE_SIZE + SPI_NAND_OOB_SIZE()) - column;
    if (read_len > max_read_len) return SPI_NAND_RET_INVALID_LEN;

    // setup timeout tracking
    uint32_t start = sys_time_get_ms();

    // read page into flash's internal cache
    int ret = page_read(row, OP_TIMEOUT);
    if (SPI_NAND_RET_OK != ret) return ret;

    // read from cache 
    uint32_t timeout = OP_TIMEOUT - sys_time_get_elapsed(start);
    return read_from_cache(row, column, data_out, read_len, timeout);
}

int spi_nand_page_program(row_address_t row, column_address_t column, const void *data_in,
                          size_t write_len)
{
    // input validation
    if (!validate_row_address(row) || !validate_column_address(column)) {
        return SPI_NAND_RET_BAD_ADDRESS;
    }
    uint16_t max_write_len = (SPI_NAND_PAGE_SIZE + SPI_NAND_OOB_SIZE()) - column;
    if (write_len > max_write_len) return SPI_NAND_RET_INVALID_LEN;

    // setup timeout tracking
    uint32_t start = sys_time_get_ms();

    // write enable
    int ret = write_enable(OP_TIMEOUT);
    if (SPI_NAND_RET_OK != ret) return ret;

    // load data into nand's internal cache
    uint32_t timeout = OP_TIMEOUT - sys_time_get_elapsed(start);
    ret = program_load(row, column, data_in, write_len, timeout);
    if (SPI_NAND_RET_OK != ret) return ret;

    // write to cell array from nand's internal cache
    timeout = OP_TIMEOUT - sys_time_get_elapsed(start);
    return program_execute(row, timeout);
}

int spi_nand_page_copy(row_address_t src, row_address_t dest)
{
    // input validation
    if (!validate_row_address(src) || !validate_row_address(src)) {
        return SPI_NAND_RET_BAD_ADDRESS;
    }
    if (!validate_row_address(dest) || !validate_row_address(dest)) {
        return SPI_NAND_RET_BAD_ADDRESS;
    }
    if (get_plane(src) != get_plane(dest)) {
        // If the source and destination are on different planes,
        // the optimized copy operation cannot be used.  This is because
        // there does not appear to be a command to move data from one
        // plane's cache register to another plane's cache register.
        //
        // Without this check, data would be corrupted on multi-plane
        // NAND when the source and destination are on different planes.
        //
        // As a result, must read the source sector to a host buffer, and
        // then write that host buffer to the destination sector.
        size_t page_and_oob_len = SPI_NAND_PAGE_SIZE + SPI_NAND_OOB_SIZE();
        int ret = spi_nand_page_read(src, 0, page_main_and_largest_oob_buffer, page_and_oob_len);
        if (SPI_NAND_RET_OK != ret) return ret;
        return spi_nand_page_program(dest, 0, page_main_and_largest_oob_buffer, page_and_oob_len);
    }

    // setup timeout tracking
    uint32_t start = sys_time_get_ms();

    // read page into flash's internal cache
    int ret = page_read(src, OP_TIMEOUT);
    if (SPI_NAND_RET_OK != ret) return ret;

    // write enable
    uint32_t timeout = OP_TIMEOUT - sys_time_get_elapsed(start);
    ret = write_enable(timeout);
    if (SPI_NAND_RET_OK != ret) return ret;

    // empty program load random data
    timeout = OP_TIMEOUT - sys_time_get_elapsed(start);
    uint8_t dummy_byte = 0; // avoid a null pointer
    ret = program_load_random_data(src, 0, &dummy_byte, 0, timeout);
    if (SPI_NAND_RET_OK != ret) return ret;

    // write to cell array from nand's internal cache
    timeout = OP_TIMEOUT - sys_time_get_elapsed(start);
    return program_execute(dest, timeout);
}

int spi_nand_block_erase(row_address_t row)
{
    row.page = 0; // make sure page address is zero
    // input validation
    if (!validate_row_address(row)) {
        return SPI_NAND_RET_BAD_ADDRESS;
    }

    // setup timeout tracking
    uint32_t start = sys_time_get_ms();

    // write enable
    int ret = write_enable(OP_TIMEOUT); // ignore the time elapsed since start since its negligible
    if (SPI_NAND_RET_OK != ret) return ret;

    // block erase
    uint32_t timeout = OP_TIMEOUT - sys_time_get_elapsed(start);
    return block_erase(row, timeout);
}

int spi_nand_block_is_bad(row_address_t row, bool *is_bad)
{
    uint8_t bad_block_mark[1];
    // page read will validate the block address
    int ret = spi_nand_page_read(row, SPI_NAND_PAGE_SIZE, bad_block_mark, sizeof(bad_block_mark));
    if (SPI_NAND_RET_OK != ret) return ret;

    // Refer to MT29F2G01ABAGD datasheet, table 11 on page 46:
    // Bad blocks can be detected by the value 0x00 in the
    // FIRST BYTE of the spare area.
    // This is ONFI-compliant, so should be universal nowadays.
    if (BAD_BLOCK_MARK == bad_block_mark[0]) {
        *is_bad = true;
    }
    else {
        *is_bad = false;
    }

    return SPI_NAND_RET_OK;
}

int spi_nand_block_mark_bad(row_address_t row)
{
    // Refer to MT29F2G01ABAGD datasheet, table 11 on page 46:
    // Bad blocks can be detected by the value 0x00 in the
    // FIRST BYTE of the spare area.
    // This is ONFI-compliant, so should be universal nowadays.

    uint8_t bad_block_mark[1] = {BAD_BLOCK_MARK};
    // page program will validate the block address
    return spi_nand_page_program(row, SPI_NAND_PAGE_SIZE, bad_block_mark, sizeof(bad_block_mark));
}

int spi_nand_page_is_free(row_address_t row, bool *is_free)
{
    // page read will validate block & page address
    size_t page_and_oob_len = SPI_NAND_PAGE_SIZE + SPI_NAND_OOB_SIZE();

    int ret = spi_nand_page_read(row, 0, page_main_and_largest_oob_buffer, page_and_oob_len);
    if (SPI_NAND_RET_OK != ret) return ret;

    *is_free = true; // innocent until proven guilty
    // iterate through page & oob to make sure its 0xff's all the way down

    // TODO: static_assert( sizeof(page_main_and_oob_buffer) % sizeof(uint32_t) == 0, "page_main_and_oob_buffer size must be a multiple of 4" );
    uint32_t comp_word = 0xffffffff;
    for (size_t i = 0; i < page_and_oob_len; i += sizeof(comp_word)) {
        if (0 != memcmp(&comp_word, &page_main_and_largest_oob_buffer[i], sizeof(comp_word))) {
            *is_free = false;
            break;
        }
    }

    return SPI_NAND_RET_OK;
}

int spi_nand_clear(void)
{
    bool is_bad;
    for (int i = 0; i < SPI_NAND_ERASE_BLOCKS_PER_LUN(); i++) {
        // get bad block flag
        row_address_t row = {.block = i, .page = 0};
        int ret = spi_nand_block_is_bad(row, &is_bad);
        if (SPI_NAND_RET_OK != ret) return ret;

        // erase if good block
        if (!is_bad) {
            int ret = spi_nand_block_erase(row);
            if (SPI_NAND_RET_OK != ret) return ret;
        }
    }

    // if we made it here, nothing returned a bad status
    return SPI_NAND_RET_OK;
}

// private function definitions
static void csel_setup(void)
{
    /*// enable peripheral clock
    if (!LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOB))
        LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

    // setup pin as output
    LL_GPIO_SetPinMode(CSEL_PORT, CSEL_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(CSEL_PORT, CSEL_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(CSEL_PORT, CSEL_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(CSEL_PORT, CSEL_PIN, LL_GPIO_PULL_NO);
    */
    //gpio_set_function(FLASH_STORAGE_CS, GPIO_FUNC_SIO);
    //gpio_put(FLASH_STORAGE_CS, 1);
    //gpio_set_dir(FLASH_STORAGE_CS, GPIO_OUT);    
}

static void csel_deselect(void)
{
    //LL_GPIO_SetOutputPin(CSEL_PORT, CSEL_PIN);
    gpio_put(FLASH_STORAGE_CS, 1); 
    spi_busy_wait(false);
}

static void csel_select(void)
{
    //LL_GPIO_ResetOutputPin(CSEL_PORT, CSEL_PIN);
    spi_busy_wait(true);
    gpio_put(FLASH_STORAGE_CS, 0); 
}

static int spi_nand_reset(void)
{
    // setup data
    uint8_t tx_data = CMD_RESET; // this is just a one-byte command
    // perform transaction
    csel_select();
    int ret = nand_spi_write(&tx_data, 1, OP_TIMEOUT);
    csel_deselect();
    if (SPI_RET_OK != ret) return SPI_NAND_RET_BAD_SPI;

    // wait until op is done or we timeout
    feature_reg_status_t status;
    return poll_for_oip_clear(&status, OP_TIMEOUT);
}

static int read_id(nand_identity_t * identity_out)
{
    memset(identity_out, 0, sizeof(nand_identity_t));

    // setup data
    uint8_t tx_data[READ_ID_TRANS_LEN] = {0};
    uint8_t rx_data[READ_ID_TRANS_LEN] = {0};
    tx_data[0] = CMD_READ_ID;
    // perform transaction
    csel_select();
    int ret = nand_spi_write_read(tx_data, rx_data, READ_ID_TRANS_LEN, OP_TIMEOUT);
    csel_deselect();

    // check spi return
    if (SPI_RET_OK == ret) {
        identity_out->Manufacturer = rx_data[READ_ID_MFR_INDEX];
        identity_out->DeviceID = rx_data[READ_ID_DEVICE_INDEX];
    }
    else {
        return SPI_NAND_RET_BAD_SPI;
    }
}

static int set_feature(uint8_t reg, uint8_t data, uint32_t timeout)
{
    // setup data
    uint8_t tx_data[FEATURE_TRANS_LEN] = {0};
    tx_data[0] = CMD_SET_FEATURE;
    tx_data[FEATURE_REG_INDEX] = reg;
    tx_data[FEATURE_DATA_INDEX] = data;
    // perform transaction
    csel_select();
    int ret = nand_spi_write(tx_data, FEATURE_TRANS_LEN, timeout);
    csel_deselect();

    return (SPI_RET_OK == ret) ? SPI_NAND_RET_OK : SPI_NAND_RET_BAD_SPI;
}

static int get_feature(uint8_t reg, uint8_t *data_out, uint32_t timeout)
{
    // setup data
    uint8_t tx_data[FEATURE_TRANS_LEN] = {0};
    uint8_t rx_data[FEATURE_TRANS_LEN] = {0};
    tx_data[0] = CMD_GET_FEATURE;
    tx_data[FEATURE_REG_INDEX] = reg;
    // perform transaction
    csel_select();
    int ret = nand_spi_write_read(tx_data, rx_data, FEATURE_TRANS_LEN, timeout);
    csel_deselect();

    // if good return, write data out
    if (SPI_RET_OK == ret) {
        *data_out = rx_data[FEATURE_DATA_INDEX];
        return SPI_NAND_RET_OK;
    }
    else {
        return SPI_NAND_RET_BAD_SPI;
    }
}

static int write_enable(uint32_t timeout)
{
    // setup data
    uint8_t cmd = CMD_WRITE_ENABLE;
    // perform transaction
    csel_select();
    int ret = nand_spi_write(&cmd, sizeof(cmd), timeout);
    csel_deselect();

    return (SPI_RET_OK == ret) ? SPI_NAND_RET_OK : SPI_NAND_RET_BAD_SPI;
}

/// @note Input validation is expected to be performed by caller.
static int page_read(row_address_t row, uint32_t timeout)
{
    // setup timeout tracking for second operation
    uint32_t start = sys_time_get_ms();

    // setup data for page read command (need to go from LSB -> MSB first on address)
    uint8_t tx_data[PAGE_READ_TRANS_LEN];
    tx_data[0] = CMD_PAGE_READ;
    tx_data[1] = row.whole >> 16;
    tx_data[2] = row.whole >> 8;
    tx_data[3] = row.whole;
    // perform transaction
    csel_select();
    int ret = nand_spi_write(tx_data, PAGE_READ_TRANS_LEN, timeout);
    csel_deselect();
    if (SPI_RET_OK != ret) return SPI_NAND_RET_BAD_SPI;

    // wait until that operation finishes
    feature_reg_status_t status;
    timeout -= sys_time_get_elapsed(start);
    ret = poll_for_oip_clear(&status, timeout);
    if (SPI_RET_OK != ret) return ret;

    // check ecc
    return get_ret_from_ecc_status(status);
}

/// @note Input validation is expected to be performed by caller.
static int read_from_cache(row_address_t row, column_address_t column, void *data_out, size_t read_len,
                           uint32_t timeout)
{
    // setup timeout tracking for second operation
    uint32_t start = sys_time_get_ms();

    // setup data for read from cache command (need to go from LSB -> MSB first on address)
    uint8_t tx_data[READ_FROM_CACHE_TRANS_LEN];

    tx_data[0] = CMD_READ_FROM_CACHE;
    uint8_t plane = get_plane(row);
    tx_data[1] = (plane << 4) | ((column >> 8) & 0xF);
    tx_data[2] = column;
    tx_data[3] = 0;
    // perform transaction
    csel_select();
    int ret = nand_spi_write(tx_data, READ_FROM_CACHE_TRANS_LEN, timeout);
    if (SPI_RET_OK == ret) {
        timeout -= sys_time_get_elapsed(start);
        ret = nand_spi_read(data_out, read_len, timeout);
    }
    csel_deselect();

    return (SPI_RET_OK == ret) ? SPI_NAND_RET_OK : SPI_NAND_RET_BAD_SPI;
}

/// @note Input validation is expected to be performed by caller.
static int program_load(row_address_t row, column_address_t column, const void *data_in, size_t write_len,
                        uint32_t timeout)
{
    // setup timeout tracking for second operation
    uint32_t start = sys_time_get_ms();

    // setup data for program load (need to go from LSB -> MSB first on address)
    uint8_t tx_data[PROGRAM_LOAD_TRANS_LEN];
    tx_data[0] = CMD_PROGRAM_LOAD;
    uint8_t plane = get_plane(row);
    tx_data[1] = (plane << 4) | ((column >> 8) & 0xF);
    tx_data[2] = column;
    // perform transaction
    csel_select();
    int ret = nand_spi_write(tx_data, PROGRAM_LOAD_TRANS_LEN, timeout);
    if (SPI_RET_OK == ret) {
        timeout -= sys_time_get_elapsed(start);
        ret = nand_spi_write(data_in, write_len, timeout);
    }
    csel_deselect();

    return (SPI_RET_OK == ret) ? SPI_NAND_RET_OK : SPI_NAND_RET_BAD_SPI;
}

static int program_load_random_data(row_address_t row, column_address_t column, void *data_in, size_t write_len,
                                    uint32_t timeout)
{
    // setup timeout tracking for second operation
    uint32_t start = sys_time_get_ms();

    // setup data for program load (need to go from LSB -> MSB first on address)
    uint8_t tx_data[PROGRAM_LOAD_RANDOM_DATA_TRANS_LEN];
    tx_data[0] = CMD_PROGRAM_LOAD_RANDOM_DATA;
    uint8_t plane = get_plane(row);
    tx_data[1] = (plane << 4) | ((column >> 8) & 0xF);
    tx_data[2] = column;
    // perform transaction
    csel_select();
    int ret = nand_spi_write(tx_data, PROGRAM_LOAD_TRANS_LEN, timeout);
    if (SPI_RET_OK == ret) {
        timeout -= sys_time_get_elapsed(start);
        ret = nand_spi_write(data_in, write_len, timeout);
    }
    csel_deselect();

    return (SPI_RET_OK == ret) ? SPI_NAND_RET_OK : SPI_NAND_RET_BAD_SPI;
}

/// @note Input validation is expected to be performed by caller.
static int program_execute(row_address_t row, uint32_t timeout)
{
    // setup timeout tracking for second operation
    uint32_t start = sys_time_get_ms();

    // setup data for program execute (need to go from LSB -> MSB first on address)
    uint8_t tx_data[PROGRAM_EXECUTE_TRANS_LEN];
    tx_data[0] = CMD_PROGRAM_EXECUTE;
    tx_data[1] = row.whole >> 16;
    tx_data[2] = row.whole >> 8;
    tx_data[3] = row.whole;
    // perform transaction
    csel_select();
    int ret = nand_spi_write(tx_data, PAGE_READ_TRANS_LEN, timeout);
    csel_deselect();
    if (SPI_RET_OK != ret) return SPI_NAND_RET_BAD_SPI;

    // wait until that operation finishes
    feature_reg_status_t status;
    timeout -= sys_time_get_elapsed(start);
    ret = poll_for_oip_clear(&status, timeout);

    if (SPI_NAND_RET_OK != ret) { // if polling failed, return that status
        return ret;
    }
    else if (status.P_FAIL) { // otherwise, check for P_FAIL
        return SPI_NAND_RET_P_FAIL;
    }
    else {
        return SPI_NAND_RET_OK;
    }
}

static int block_erase(row_address_t row, uint32_t timeout)
{
    // setup timeout tracking for second operation
    uint32_t start = sys_time_get_ms();

    // setup data for block erase command (need to go from LSB -> MSB first on address)
    uint8_t tx_data[BLOCK_ERASE_TRANS_LEN];
    tx_data[0] = CMD_BLOCK_ERASE;
    tx_data[1] = row.whole >> 16;
    tx_data[2] = row.whole >> 8;
    tx_data[3] = row.whole;
    // perform transaction
    csel_select();
    int ret = nand_spi_write(tx_data, BLOCK_ERASE_TRANS_LEN, timeout);
    csel_deselect();
    if (SPI_RET_OK != ret) return SPI_NAND_RET_BAD_SPI;

    // wait until that operation finishes
    feature_reg_status_t status;
    timeout -= sys_time_get_elapsed(start);
    ret = poll_for_oip_clear(&status, timeout);

    if (SPI_NAND_RET_OK != ret) { // if polling failed, return that status
        return ret;
    }
    else if (status.E_FAIL) { // otherwise, check for E_FAIL
        return SPI_NAND_RET_E_FAIL;
    }
    else {
        return SPI_NAND_RET_OK;
    }
}

static int unlock_all_blocks(void)
{
    feature_reg_block_lock_t unlock_all = {.whole = 0};
    return set_feature(FEATURE_REG_BLOCK_LOCK, unlock_all.whole, OP_TIMEOUT);
}

static int enable_ecc(void)
{
    feature_reg_configuration_t ecc_enable = {.whole = 0}; // we want to zero the other bits here
    ecc_enable.ECC_EN = 1;
    return set_feature(FEATURE_REG_CONFIGURATION, ecc_enable.whole, OP_TIMEOUT);
}

static int poll_for_oip_clear(feature_reg_status_t *status_out, uint32_t ms_timeout)
{
    uint32_t start_time = sys_time_get_ms();
    for (;;) {
        uint32_t get_feature_timeout = OP_TIMEOUT - sys_time_get_elapsed(start_time);
        int ret = get_feature(FEATURE_REG_STATUS, &status_out->whole, get_feature_timeout);
        // break on bad return
        if (SPI_NAND_RET_OK != ret) {
            return ret;
        }
        // check for OIP clear
        if (0 == status_out->OIP) {
            return SPI_NAND_RET_OK;
        }
        // check for timeout
        if (sys_time_is_elapsed(start_time, ms_timeout)) {
            return SPI_NAND_RET_TIMEOUT;
        }
    }
}

static bool validate_row_address(row_address_t row)
{
    if ((row.block > SPI_NAND_MAX_BLOCK_ADDRESS()) || (row.page > SPI_NAND_MAX_PAGE_ADDRESS)) {
        return false;
    }
    else {
        return true;
    }
}

static bool validate_column_address(column_address_t address)
{
    if (address >= (SPI_NAND_PAGE_SIZE + SPI_NAND_OOB_SIZE())) {
        return false;
    }
    else {
        return true;
    }
}

static int get_ret_from_ecc_status(feature_reg_status_t status)
{
    int ret;

    // map ECC status to return type
    switch (status.ECCS0_3) {
        case ECC_STATUS_NO_ERR:
        case ECC_STATUS_1_3_NO_REFRESH:
            ret = SPI_NAND_RET_OK;
            break;
        case ECC_STATUS_4_6_REFRESH:
        case ECC_STATUS_7_8_REFRESH:
            ret = SPI_NAND_RET_ECC_REFRESH;
            break;
        case ECC_STATUS_NOT_CORRECTED:
        default:
            ret = SPI_NAND_RET_ECC_ERR;
            break;
    }

    return ret;
}
