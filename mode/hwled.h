
void HWLED_start(void);
void HWLED_stop(void);
uint32_t HWLED_send(uint32_t d);
uint32_t HWLED_read(void);
void HWLED_macro(uint32_t macro);
uint32_t HWLED_setup(void);
uint32_t HWLED_setup_exc(void);
void HWLED_cleanup(void);
//void HWLED_pins(void);
void HWLED_settings(void);
void HWLED_printI2Cflags(void);
void HWLED_help(void);

typedef struct _led_mode_config{
	uint32_t num_leds;
	uint32_t device;
} _led_mode_config;