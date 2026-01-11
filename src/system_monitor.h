bool monitor(void);
void monitor_init(void);
void monitor_reset(void);
void monitor_force_update(void);
bool monitor_get_voltage_char(uint8_t pin, uint8_t digit, char* c);
bool monitor_get_voltage_ptr(uint8_t pin, char** c);
bool monitor_get_current_ptr(char** c);
bool monitor_get_current_char(uint8_t digit, char* c);

void monitor_clear_voltage(void);
void monitor_clear_current(void);
bool monitor_voltage_changed(void);
bool monitor_current_changed(void);
