void hw2wire_start(bytecode_t *result, bytecode_t *next);
void hw2wire_start_alt(bytecode_t *result, bytecode_t *next);
void hw2wire_stop(bytecode_t *result, bytecode_t *next);
void hw2wire_stop_alt(bytecode_t *result, bytecode_t *next);
void hw2wire_write(bytecode_t *result, bytecode_t *next);
void hw2wire_read(bytecode_t *result, bytecode_t *next);
void hw2wire_tick_clock(bytecode_t *result, bytecode_t *next);
void hw2wire_set_clk_high(bytecode_t *result, bytecode_t *next);
void hw2wire_set_clk_low(bytecode_t *result, bytecode_t *next);
void hw2wire_set_dat_high(bytecode_t *result, bytecode_t *next);
void hw2wire_set_dat_low(bytecode_t *result, bytecode_t *next);
void hw2wire_read_bit(bytecode_t *result, bytecode_t *next);
void hw2wire_macro(uint32_t macro);
uint32_t hw2wire_setup(void);
uint32_t hw2wire_setup_exc(void);
void hw2wire_cleanup(void);
//void hw2wire_pins(void);
void hw2wire_settings(void);
void hw2wire_printI2Cflags(void);
void hw2wire_help(void);

typedef struct _hw2wire_mode_config{
	uint32_t baudrate;
	uint32_t baudrate_actual;
	uint32_t data_bits;
	bool ack_pending;
	bool read;
	bool start_sent;
} _hw2wire_mode_config;

extern const struct _command_struct hw2wire_commands[];
extern const uint32_t hw2wire_commands_count;
extern struct _hw2wire_mode_config hw2wire_mode_config;
