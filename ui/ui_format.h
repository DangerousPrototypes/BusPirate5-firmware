void ui_format_print_number(uint32_t d);
uint32_t ui_format_bitorder(uint32_t d);
void ui_format_print_number_2(struct command_attributes *attributes, uint32_t *value);
void ui_format_print_number_3(uint32_t value, uint32_t num_bits, uint32_t display_format);
uint32_t ui_format_bitorder_manual(uint32_t *d, uint8_t num_bits, bool bit_order);
uint32_t ui_format_lsb(uint32_t d, uint8_t num_bits);