struct hex_config_t {
    uint32_t start_address; // start address for the hex dump
    uint32_t requested_bytes; // user requested number of bytes to read
    uint32_t max_size_bytes; // maximum size of the device in bytes
    uint32_t _aligned_start; // aligned start address for the hex dump
    uint32_t _aligned_end;   // aligned end address for the hex dump
    uint32_t _total_read_bytes; // total number of bytes read
    bool quiet; // disable address and ASCII dump
};


bool ui_hex_get_args(uint32_t max_size_bytes, uint32_t *start_address, uint32_t *read_bytes);
void ui_hex_align(uint32_t start_address, uint32_t read_bytes, uint32_t max_bytes, uint32_t *aligned_start, uint32_t *aligned_end, uint32_t *total_read_bytes);
void ui_hex_header(uint32_t aligned_start, uint32_t aligned_end, uint32_t total_read_bytes);
void ui_hex_row(uint32_t address, uint8_t *buf, uint32_t buf_size, struct hex_config_t *config);


bool ui_hex_get_args_config(struct hex_config_t *config);
void ui_hex_align_config(struct hex_config_t *config);
void ui_hex_header_config(struct hex_config_t *config);
void ui_hex_row_config(struct hex_config_t *config, uint32_t address, uint8_t *buf, uint32_t buf_size);

#define UI_HEX_HELP_START T_HELP_EEPROM_START_FLAG
#define UI_HEX_HELP_BYTES T_HELP_EEPROM_BYTES_FLAG
#define UI_HEX_HELP_QUIET T_HELP_DISK_HEX_QUIET