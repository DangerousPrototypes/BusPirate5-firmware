void hw2wire_start(struct _bytecode *result, struct _bytecode *next);
void hw2wire_stop(struct _bytecode *result, struct _bytecode *next);
void hw2wire_write(struct _bytecode *result, struct _bytecode *next);
void hw2wire_read(struct _bytecode *result, struct _bytecode *next);
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

