
void hwled_start(bytecode_t *result, bytecode_t *next);
void hwled_stop(bytecode_t *result, bytecode_t *next);
void hwled_write(bytecode_t *result, bytecode_t *next);
void hwled_read(bytecode_t *result, bytecode_t *next);
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

extern const struct _command_struct hwled_commands[];
extern const uint32_t hwled_commands_count;