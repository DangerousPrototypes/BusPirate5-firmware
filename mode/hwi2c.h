
void hwi2c_start(struct _bytecode *result, struct _bytecode *next);
void hwi2c_stop(struct _bytecode *result, struct _bytecode *next);
void hwi2c_write(struct _bytecode *result, struct _bytecode *next);
void hwi2c_read(struct _bytecode *result, struct _bytecode *next);
void hwi2c_macro(uint32_t macro);
uint32_t hwi2c_setup(void);
uint32_t hwi2c_setup_exc(void);
void hwi2c_cleanup(void);
//void hwi2c_pins(void);
void hwi2c_settings(void);
void hwi2c_printI2Cflags(void);
void hwi2c_help(void);
uint8_t hwi2c_checkshort(void);
uint32_t hwi2c_get_speed(void);

typedef struct _i2c_mode_config{
	uint32_t baudrate;
	//uint32_t baudrate_actual;
	uint32_t data_bits;
	bool ack_pending;
	bool read;
	bool start_sent;
} _i2c_mode_config;


extern const struct _command_struct hwi2c_commands[];
extern const uint32_t hwi2c_commands_count;