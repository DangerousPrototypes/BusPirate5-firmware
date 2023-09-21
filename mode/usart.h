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

typedef struct _uart_mode_config{
	uint32_t baudrate;
	uint32_t baudrate_actual;
	uint32_t data_bits;
	uint32_t stop_bits; 
	uint32_t parity;
    uint32_t blocking;
}_uart_mode_config;