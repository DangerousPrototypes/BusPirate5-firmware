
void i2s_write(struct _bytecode* result, struct _bytecode* next);
void i2s_read(struct _bytecode* result, struct _bytecode* next);
void i2s_start(struct _bytecode* result, struct _bytecode* next);
void i2s_stop(struct _bytecode* result, struct _bytecode* next);

// full duplex commands not currently implemented
void i2s_startr(struct _bytecode* result, struct _bytecode* next);
void i2s_stopr(struct _bytecode* result, struct _bytecode* next);

// a periodic service call for doing things async.
void i2s_periodic(void);

// setup functions
uint32_t i2s_setup(void);
uint32_t i2s_setup_exc(void);
void i2s_cleanup(void);

// displayed in the 'i' command
void i2s_settings(void);

const char* i2s_pins(void);

// old bitwise commands not currently needed because we don't bitbang anymore
void i2s_clkh(struct _bytecode* result, struct _bytecode* next);
void i2s_clkl(struct _bytecode* result, struct _bytecode* next);
void i2s_dath(struct _bytecode* result, struct _bytecode* next);
void i2s_datl(struct _bytecode* result, struct _bytecode* next);
void i2s_dats(struct _bytecode* result, struct _bytecode* next);
void i2s_clk(struct _bytecode* result, struct _bytecode* next);
void i2s_bitr(struct _bytecode* result, struct _bytecode* next);

void i2s_help(void);

extern const struct _mode_command_struct i2s_commands[];
extern const uint32_t i2s_commands_count;
