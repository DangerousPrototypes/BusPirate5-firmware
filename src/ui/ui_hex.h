void ui_hex_align(uint32_t start_address, uint32_t read_bytes, uint32_t max_bytes, uint32_t *aligned_start, uint32_t *aligned_end, uint32_t *total_read_bytes);
void ui_hex_header(uint32_t aligned_start, uint32_t aligned_end, uint32_t total_read_bytes);
void ui_hex_row(uint32_t address, uint8_t *buf, uint32_t buf_size);