void ui_info_print_info(struct command_attributes *attributes, struct command_response *response);
void ui_info_print_help(struct command_attributes *attributes, struct command_response *response);

void ui_info_print_toolbar(void);
void ui_info_print_pin_names(void);
void ui_info_print_pin_labels(void);
void ui_info_print_pin_voltage(bool refresh);
void ui_info_print_error(uint32_t error);

//void ui_info_print_pin_states(int refresh);