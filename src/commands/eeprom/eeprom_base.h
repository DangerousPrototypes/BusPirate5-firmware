#define EEPROM_DEBUG 0
#define EEPROM_ADDRESS_PAGE_SIZE 256 // size of the EEPROM address page in bytes

struct eeprom_device_t {
    char name[9];
    uint32_t size_bytes;
    uint8_t address_bytes; 
    uint8_t block_select_bits;
    uint8_t block_select_offset;
    uint16_t page_bytes; 
    uint32_t max_speed_khz; //if max speed is <10MHz, then specify the speed in kHz
};

struct eeprom_hal_t;

struct eeprom_info{
    const struct eeprom_device_t* device;
    const struct eeprom_hal_t *hal; // HAL for EEPROM operations
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
    bool (*write_page)(struct eeprom_info *eeprom, uint32_t address, uint8_t *buf);
    bool (*write_protection_blocks)(struct eeprom_info *eeprom, uint8_t block_select_bits, uint8_t *_array, uint8_t reg);
};

enum eeprom_read_action{
    EEPROM_READ_TO_FILE = 0,
    EEPROM_VERIFY_FILE,
    EEPROM_VERIFY_BUFFER
};

void eeprom_display_devices(const struct eeprom_device_t *eeprom_devices, uint8_t count);
uint32_t eeprom_get_address_blocks_total(struct eeprom_info *eeprom);
uint32_t eeprom_get_address_block_size(struct eeprom_info *eeprom);
bool eeprom_get_address(struct eeprom_info *eeprom, uint32_t address, uint8_t *block_select_bits, uint8_t *address_array);
bool eeprom_dump(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size);
bool eeprom_write(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, bool write_from_buf);
bool eeprom_read(struct eeprom_info *eeprom, char *buf, uint32_t buf_size, char *verify_buf, uint32_t verify_buf_size, enum eeprom_read_action action);

// action functions, high level
bool eeprom_action_erase(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size, bool verify);
bool eeprom_action_test(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size);
bool eeprom_action_write(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size, bool verify);
bool eeprom_action_read(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size, bool verify);
bool eeprom_action_verify(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size);