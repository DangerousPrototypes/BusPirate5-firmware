
void hwled_start(struct _bytecode *result, struct _bytecode *next);
void hwled_stop(struct _bytecode *result, struct _bytecode *next);
void hwled_write(struct _bytecode *result, struct _bytecode *next);
void hwled_read(struct _bytecode *result, struct _bytecode *next);
void hwled_macro(uint32_t macro);
uint32_t hwled_setup(void);
uint32_t hwled_setup_exc(void);
void hwled_cleanup(void);
//void hwled_pins(void);
void hwled_settings(void);
void hwled_printI2Cflags(void);
void hwled_help(void);

typedef struct _led_mode_config{
	uint32_t num_leds;
	uint32_t device;
} _led_mode_config;