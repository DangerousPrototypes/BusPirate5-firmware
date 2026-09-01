uint32_t ps2_setup(void);
uint32_t ps2_setup_exc(void);
void ps2_cleanup(void);
bool ps2_preflight_sanity_check(void);
void ps2_pins(void);
void ps2_settings(void);
void ps2_help(void);
uint32_t ps2_get_speed(void);


extern const struct _mode_command_struct ps2_commands[];
extern const uint32_t ps2_commands_count;
