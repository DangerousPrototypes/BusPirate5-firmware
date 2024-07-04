void hwhduart_open(bytecode_t *result, bytecode_t *next);				// start
void hwhduart_open_read(bytecode_t *result, bytecode_t *next);
void hwhduart_stop_alt(bytecode_t *result, bytecode_t *next);
void hwhduart_start_alt(bytecode_t *result, bytecode_t *next);				// start with read
void hwhduart_close(bytecode_t *result, bytecode_t *next);				// stop
void hwhduart_write(bytecode_t *result, bytecode_t *next);
void hwhduart_read(bytecode_t *result, bytecode_t *next);
void hwhduart_macro(uint32_t macro);
uint32_t hwhduart_setup(void);
uint32_t hwhduart_setup_exc(void);
void hwhduart_cleanup(void);
void hwhduart_pins(void);
void hwhduart_settings(void);
void hwhduart_printerror(void);
void hwhduart_help(void);
void hwhduart_periodic(void);

/*
typedef struct _uart_mode_config{
	uint32_t baudrate;
	uint32_t baudrate_actual;
	uint32_t data_bits;
	uint32_t stop_bits; 
	uint32_t parity;
    uint32_t blocking;
	bool async_print;
}_uart_mode_config;
*/
extern const struct _command_struct hwhduart_commands[];
extern const uint32_t hwhduart_commands_count;