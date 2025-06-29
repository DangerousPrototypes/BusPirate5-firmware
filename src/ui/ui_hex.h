struct hex_config_t {
    bool highlight; // flag to highlight the address
    uint32_t highlight_start; // start address to highlight
    uint32_t highlight_end;   // end address to highlight
    uint32_t highlight_color; // color to highlight the address
};


bool ui_hex_get_args(uint32_t max_size_bytes, uint32_t *start_address, uint32_t *read_bytes);
void ui_hex_align(uint32_t start_address, uint32_t read_bytes, uint32_t max_bytes, uint32_t *aligned_start, uint32_t *aligned_end, uint32_t *total_read_bytes);
void ui_hex_header(uint32_t aligned_start, uint32_t aligned_end, uint32_t total_read_bytes);
void ui_hex_row(uint32_t address, uint8_t *buf, uint32_t buf_size, struct hex_config_t *config);

#define UI_HEX_HELP_START T_HELP_EEPROM_START_FLAG
#define UI_HEX_HELP_BYTES T_HELP_EEPROM_BYTES_FLAG