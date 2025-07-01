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
    char file_name[13]; // file to read/write/verify
    bool verify_flag; // verify flag
    uint32_t start_address; // start address for read/write
    uint32_t user_bytes; // user specified number of bytes to read/write
    FIL file_handle;     // file handle
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