/**
 * @file eeprom_base.h
 * @brief EEPROM base definitions and utilities.
 * @details Provides common EEPROM device definitions, HAL abstraction, and utility functions.
 */

#define EEPROM_DEBUG 0
#define EEPROM_ADDRESS_PAGE_SIZE 256 // size of the EEPROM address page in bytes

struct eeprom_hal_t;

struct eeprom_device_t {
    char name[9];
    uint32_t size_bytes;
    uint8_t address_bytes; 
    uint8_t block_select_bits;
    uint8_t block_select_offset;
    uint16_t page_bytes; 
    uint32_t max_speed_khz; //if max speed is <10MHz, then specify the speed in kHz
    const struct eeprom_hal_t *hal; // HAL for EEPROM operations
};

struct eeprom_info{
    const struct eeprom_device_t* device;
    //const struct eeprom_hal_t *hal; // HAL for EEPROM operations
    uint8_t device_address; // 7-bit address for the device
    uint32_t action;
    //uint32_t start_address; // start address for read/write
    //uint32_t user_bytes; // user specified number of bytes to read/write
    uint32_t protect_bits;
    uint32_t protect_wpen_bit;
    FIL file_handle;     // file handle
    char file_name[13]; // file to read/write/verify
    bool verify_flag; // verify flag
    bool protect_blocks_flag;
    bool protect_test_flag;
    bool protect_wpen_flag;
};

struct eeprom_hal_t {
    bool (*get_address)(struct eeprom_info *eeprom, uint32_t address, uint8_t *block_select_bits, uint8_t *address_array);
    bool (*read)(struct eeprom_info *eeprom, uint32_t address, uint32_t read_bytes, uint8_t *buf);
    bool (*write_page)(struct eeprom_info *eeprom, uint32_t address, uint8_t *buf, uint32_t page_write_size);
    bool (*is_write_protected)(struct eeprom_info *eeprom);
    bool (*probe_protect)(struct eeprom_info *eeprom); // probe the write protection status of the device
};

enum eeprom_read_action{
    EEPROM_READ_TO_FILE = 0,
    EEPROM_VERIFY_FILE,
    EEPROM_VERIFY_BUFFER
};

/**
 * @brief Display list of supported EEPROM devices.
 * @param eeprom_devices  Array of EEPROM device definitions
 * @param count           Number of devices in array
 */
void eeprom_display_devices(const struct eeprom_device_t *eeprom_devices, uint8_t count);

/**
 * @brief Get total number of address blocks in EEPROM.
 * @param eeprom  EEPROM information structure
 * @return        Total number of blocks
 */
uint32_t eeprom_get_address_blocks_total(struct eeprom_info *eeprom);

/**
 * @brief Get size of address block in EEPROM.
 * @param eeprom  EEPROM information structure
 * @return        Block size in bytes
 */
uint32_t eeprom_get_address_block_size(struct eeprom_info *eeprom);

/**
 * @brief Get block select bits and address array for EEPROM address.
 * @param eeprom             EEPROM information structure
 * @param address            Target address
 * @param block_select_bits  Output block select bits
 * @param address_array      Output address byte array
 * @return                   true on success
 */
bool eeprom_get_address(struct eeprom_info *eeprom, uint32_t address, uint8_t *block_select_bits, uint8_t *address_array);

/**
 * @brief Dump EEPROM contents to buffer.
 * @param eeprom    EEPROM information structure
 * @param buf       Output buffer
 * @param buf_size  Buffer size in bytes
 * @return          true on success
 */
struct bp_command_def;
bool eeprom_dump(const struct bp_command_def *def, struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size);

/**
 * @brief Write data to EEPROM.
 * @param eeprom          EEPROM information structure
 * @param buf             Data buffer
 * @param buf_size        Buffer size in bytes
 * @param write_from_buf  true to write from buffer, false from file
 * @return                true on success
 */
bool eeprom_write(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, bool write_from_buf);

/**
 * @brief Read data from EEPROM.
 * @param eeprom           EEPROM information structure
 * @param buf              Data buffer
 * @param buf_size         Buffer size in bytes
 * @param verify_buf       Verify buffer
 * @param verify_buf_size  Verify buffer size
 * @param action           Read action (file/verify/buffer)
 * @return                 true on success
 */
bool eeprom_read(struct eeprom_info *eeprom, char *buf, uint32_t buf_size, char *verify_buf, uint32_t verify_buf_size, enum eeprom_read_action action);

/**
 * @brief Erase EEPROM (write all 0xFF).
 * @param eeprom           EEPROM information structure
 * @param buf              Working buffer
 * @param buf_size         Buffer size
 * @param verify_buf       Verify buffer
 * @param verify_buf_size  Verify buffer size
 * @param verify           true to verify erase
 * @return                 true on success
 */
bool eeprom_action_erase(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size, bool verify);

/**
 * @brief Test EEPROM read/write functionality.
 * @param eeprom           EEPROM information structure
 * @param buf              Working buffer
 * @param buf_size         Buffer size
 * @param verify_buf       Verify buffer
 * @param verify_buf_size  Verify buffer size
 * @return                 true on success
 */
bool eeprom_action_test(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size);

/**
 * @brief Write file to EEPROM.
 * @param eeprom           EEPROM information structure
 * @param buf              Working buffer
 * @param buf_size         Buffer size
 * @param verify_buf       Verify buffer
 * @param verify_buf_size  Verify buffer size
 * @param verify           true to verify write
 * @return                 true on success
 */
bool eeprom_action_write(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size, bool verify);

/**
 * @brief Read EEPROM to file.
 * @param eeprom           EEPROM information structure
 * @param buf              Working buffer
 * @param buf_size         Buffer size
 * @param verify_buf       Verify buffer
 * @param verify_buf_size  Verify buffer size
 * @param verify           true to verify read
 * @return                 true on success
 */
bool eeprom_action_read(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size, bool verify);

/**
 * @brief Verify EEPROM contents against file.
 * @param eeprom           EEPROM information structure
 * @param buf              Working buffer
 * @param buf_size         Buffer size
 * @param verify_buf       Verify buffer
 * @param verify_buf_size  Verify buffer size
 * @return                 true on success
 */
bool eeprom_action_verify(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size);

/**
 * @brief Confirm destructive action with user.
 * @return true if user confirms, false if aborted
 */
bool eeprom_confirm_action(void);