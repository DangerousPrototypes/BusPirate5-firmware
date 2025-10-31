const char* hiz_pins(void);
const char* hiz_error(void);
void hiz_cleanup(void);
uint32_t hiz_setup(void);
uint32_t hiz_setup_exec(void);
void hiz_help(void);
bool bpio_hiz_configure(bpio_mode_configuration_t *bpio_mode_config);

extern const uint32_t hiz_commands_count;
extern const struct _mode_command_struct hiz_commands[];
