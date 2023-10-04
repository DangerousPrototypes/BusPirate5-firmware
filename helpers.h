void helpers_selftest(struct command_attributes *attributes, struct command_response *response);


void helpers_delay_us(struct command_attributes *attributes, struct command_response *response);
void helpers_delay_ms(struct command_attributes *attributes, struct command_response *response);
void helpers_bit_order_msb(struct command_attributes *attributes, struct command_response *response);
void helpers_bit_order_lsb(struct command_attributes *attributes, struct command_response *response);
void helpers_show_int_formats(struct command_attributes *attributes, struct command_response *response);
void helpers_show_int_inverse(struct command_attributes *attributes, struct command_response *response);

//mode commands
void helpers_mode_periodic();
void helpers_mode_macro(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_start(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_stop(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_start_with_read(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_stop_with_read(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_clock_high(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_clock_low(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_clock_tick(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_data_high(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_data_low(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_data_s(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_read_bit(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_help(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_read(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_write(struct command_attributes *attributes, struct command_response *response);
void helpers_mode_write_string(struct command_attributes *attributes, struct command_response *response);
void helpers_numbits(struct command_attributes *attributes, struct command_response *response);


// debug hook
void helpers_debug(struct command_attributes *attributes, struct command_response *response);
