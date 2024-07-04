
void dummy1_write(bytecode_t *result, bytecode_t *next);
void dummy1_read(bytecode_t *result, bytecode_t *next);
void dummy1_start(bytecode_t *result, bytecode_t *next);
void dummy1_stop(bytecode_t *result, bytecode_t *next);

// full duplex commands not currently implemented
void dummy1_startr(void);
void dummy1_stopr(void);

// passes the number in (1) = 1 for mode based macros
void dummy1_macro(uint32_t macro);

// a periodic service call for doing things async.
uint32_t dummy1_periodic(void);

// setup functions
uint32_t dummy1_setup(void);
uint32_t dummy1_setup_exc(void);
void dummy1_cleanup(void);

// displayed in the 'i' command
void dummy1_settings(void);

const char * dummy1_pins(void);

// old bitwise commands not currently needed because we don't bitbang anymore
void dummy1_clkh(void);
void dummy1_clkl(void);
void dummy1_dath(void);
void dummy1_datl(void);
uint32_t dummy1_dats(void);
void dummy1_clk(void);
uint32_t dummy1_bitr(void);

void dummy1_help(void);

extern const struct _command_struct dummy1_commands[];
extern const uint32_t dummy1_commands_count;
