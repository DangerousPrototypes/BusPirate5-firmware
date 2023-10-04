uint32_t HWUSART_send(uint32_t d);
uint32_t HWUSART_read(void);
void HWUSART_macro(uint32_t macro);
uint32_t HWUSART_setup(void);
uint32_t HWUSART_setup_exc(void);
void HWUSART_cleanup(void);
void HWUSART_pins(void);
void HWUSART_settings(void);
void HWUSART_printerror(void);
void HWUSART_help(void);
void HWUSART_open(void);				// start
void HWUSART_open_read(void);				// start with read
void HWUSART_close(void);				// stop
uint32_t HWUSART_periodic(void);

typedef struct _uart_mode_config{
	uint32_t baudrate;
	uint32_t baudrate_actual;
	uint32_t data_bits;
	uint32_t stop_bits; 
	uint32_t parity;
    uint32_t blocking;
	bool async_print;
}_uart_mode_config;