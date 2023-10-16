
void HWI2C_start(struct _bytecode_result *result);
void HWI2C_stop(void);
void HWI2C_start_post(void);
void HWI2C_stop_post(void);
uint32_t HWI2C_write(uint32_t d);
uint32_t HWI2C_read(uint8_t next_command);
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

