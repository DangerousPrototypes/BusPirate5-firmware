
void HWI2C_start(struct _bytecode *result, struct _bytecode *next);
void HWI2C_stop(struct _bytecode *result, struct _bytecode *next);
void HWI2C_write(struct _bytecode *result, struct _bytecode *next);
void HWI2C_read(struct _bytecode *result, struct _bytecode *next);
void HWI2C_macro(uint32_t macro);
uint32_t HWI2C_setup(void);
uint32_t HWI2C_setup_exc(void);
void HWI2C_cleanup(void);
//void HWI2C_pins(void);
void HWI2C_settings(void);
void HWI2C_printI2Cflags(void);
void HWI2C_help(void);

typedef struct _i2c_mode_config{
	uint32_t baudrate;
	uint32_t baudrate_actual;
	uint32_t data_bits;
	bool ack_pending;
	bool read;
	bool start_sent;
} _i2c_mode_config;

