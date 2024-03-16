void hwusart_open(struct _bytecode *result, struct _bytecode *next);				// start
void hwusart_open_read(struct _bytecode *result, struct _bytecode *next);				// start with read
void hwusart_close(struct _bytecode *result, struct _bytecode *next);				// stop
void hwusart_write(struct _bytecode *result, struct _bytecode *next);
void hwusart_read(struct _bytecode *result, struct _bytecode *next);
void hwusart_macro(uint32_t macro);
uint32_t hwusart_setup(void);
uint32_t hwusart_setup_exc(void);
void hwusart_cleanup(void);
void hwusart_pins(void);
void hwusart_settings(void);
void hwusart_printerror(void);
void hwusart_help(void);
void hwusart_periodic(void);

typedef struct _uart_mode_config{
	uint32_t baudrate;
	uint32_t baudrate_actual;
	uint32_t data_bits;
	uint32_t stop_bits; 
	uint32_t parity;
    uint32_t blocking;
	bool async_print;
}_uart_mode_config;