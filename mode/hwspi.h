void spi_start(void);
void spi_startr(void);
void spi_start_post(void);
void spi_stop(void);
void spi_stopr(void); 
void spi_stop_post(void);
uint32_t spi_send(uint32_t d);
uint32_t spi_read(void);
void spi_macro(uint32_t macro);
uint32_t spi_setup(void);
uint32_t spi_setup_exc(void);
void spi_cleanup(void);
void spi_pins(void);
void spi_settings(void);
void spi_printSPIflags(void);
void spi_help(void);

// special for binmode and lcd
void spi_setcpol(uint32_t val);
void spi_setcpha(uint32_t val);
void spi_setbr(uint32_t val);
void spi_setdff(uint32_t val);
void spi_setlsbfirst(uint32_t val);
void spi_set_cs_idle(uint32_t val);
void spi_set_cs(uint8_t cs);
uint8_t spi_xfer(const uint8_t out);

typedef struct _spi_mode_config{
	uint32_t baudrate;
	uint32_t baudrate_actual;
	uint32_t data_bits;
	uint32_t clock_polarity; 
	uint32_t clock_phase;
	uint32_t cs_idle; 
	uint32_t dff; 
}_spi_mode_config;
