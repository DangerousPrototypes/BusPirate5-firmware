uint32_t ps2_setup(void);
uint32_t ps2_setup_exc(void);
void ps2_cleanup(void);
bool ps2_preflight_sanity_check(void);
void ps2_pins(void);
void ps2_settings(void);
void ps2_help(void);
uint32_t ps2_get_speed(void);
void ps2_host_start(struct _bytecode *result, struct _bytecode *next);
void ps2_host_stop(struct _bytecode *result, struct _bytecode *next);
void ps2_host_write(struct _bytecode *result, struct _bytecode *next);
void ps2_host_read(struct _bytecode *result, struct _bytecode *next);


extern const struct _mode_command_struct ps2_commands[];
extern const uint32_t ps2_commands_count;
