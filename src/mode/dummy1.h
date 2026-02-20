
void dummy1_write(struct _bytecode* result, struct _bytecode* next);
void dummy1_read(struct _bytecode* result, struct _bytecode* next);
void dummy1_start(struct _bytecode* result, struct _bytecode* next);
void dummy1_stop(struct _bytecode* result, struct _bytecode* next);

// full duplex start/stop — signature matches _mode struct
void dummy1_startr(struct _bytecode* result, struct _bytecode* next);
void dummy1_stopr(struct _bytecode* result, struct _bytecode* next);

// passes the number in (1) = 1 for mode based macros
void dummy1_macro(uint32_t macro);

// a periodic service call for doing things async.
void dummy1_periodic(void);

// setup functions
uint32_t dummy1_setup(void);
uint32_t dummy1_setup_exc(void);
void dummy1_cleanup(void);

// displayed in the 'i' command
void dummy1_settings(void);

// bitwise handlers — signature must match _mode struct
void dummy1_clkh(struct _bytecode* result, struct _bytecode* next);
void dummy1_clkl(struct _bytecode* result, struct _bytecode* next);
void dummy1_dath(struct _bytecode* result, struct _bytecode* next);
void dummy1_datl(struct _bytecode* result, struct _bytecode* next);
void dummy1_dats(struct _bytecode* result, struct _bytecode* next);
void dummy1_clk(struct _bytecode* result, struct _bytecode* next);
void dummy1_bitr(struct _bytecode* result, struct _bytecode* next);

void dummy1_help(void);

extern const struct _mode_command_struct dummy1_commands[];
extern const uint32_t dummy1_commands_count;
extern const struct bp_command_def dummy1_setup_def;
