
void hwi2c_start(bytecode_t *result, bytecode_t *next);
void hwi2c_stop(bytecode_t *result, bytecode_t *next);
void hwi2c_write(bytecode_t *result, bytecode_t *next);
void hwi2c_read(bytecode_t *result, bytecode_t *next);
void hwi2c_macro(uint32_t macro);
uint32_t hwi2c_setup(void);
uint32_t hwi2c_setup_exc(void);
void hwi2c_cleanup(void);
//void hwi2c_pins(void);
void hwi2c_settings(void);
void hwi2c_printI2Cflags(void);
void hwi2c_help(void);
uint8_t hwi2c_checkshort(void);

typedef struct _i2c_mode_config{
	uint32_t baudrate;
	uint32_t baudrate_actual;
	uint32_t data_bits;
	bool ack_pending;
	bool read;
	bool start_sent;
} _i2c_mode_config;


extern const struct _command_struct hwi2c_commands[];
extern const uint32_t hwi2c_commands_count;