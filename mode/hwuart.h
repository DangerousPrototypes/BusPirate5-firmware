void hwuart_open(bytecode_t *result, bytecode_t *next);				// start
void hwuart_open_read(bytecode_t *result, bytecode_t *next);				// start with read
void hwuart_close(bytecode_t *result, bytecode_t *next);				// stop
void hwuart_write(bytecode_t *result, bytecode_t *next);
void hwuart_read(bytecode_t *result, bytecode_t *next);
void hwuart_macro(uint32_t macro);
uint32_t hwuart_setup(void);
uint32_t hwuart_setup_exc(void);
void hwuart_cleanup(void);
void hwuart_pins(void);
void hwuart_settings(void);
void hwuart_printerror(void);
void hwuart_help(void);
void hwuart_periodic(void);

typedef struct _uart_mode_config{
	uint32_t baudrate;
	uint32_t baudrate_actual;
	uint32_t data_bits;
	uint32_t stop_bits; 
	uint32_t parity;
    uint32_t blocking;
	bool async_print;
}_uart_mode_config;

extern const struct _command_struct hwuart_commands[];
extern const uint32_t hwuart_commands_count;