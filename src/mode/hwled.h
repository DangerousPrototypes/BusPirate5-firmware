
void hwled_start(struct _bytecode* result, struct _bytecode* next);
void hwled_stop(struct _bytecode* result, struct _bytecode* next);
void hwled_write(struct _bytecode* result, struct _bytecode* next);
void hwled_read(struct _bytecode* result, struct _bytecode* next);
void hwled_macro(uint32_t macro);
uint32_t hwled_setup(void);
uint32_t hwled_setup_exc(void);
void hwled_cleanup(void);
// void hwled_pins(void);
void hwled_settings(void);
void hwled_printI2Cflags(void);
void hwled_help(void);
uint32_t hwled_get_speed(void);
void hwled_wait_idle(void);
bool hwled_preflight_sanity_check(void);
bool hwled_bpio_configure(bpio_mode_configuration_t *bpio_mode_config);

enum M_LED_DEVICE_TYPE {
    M_LED_WS2812,
    M_LED_APA102,
    M_LED_WS2812_ONBOARD
};

typedef struct _led_mode_config {
    uint32_t num_leds;
    uint32_t device;
    uint32_t baudrate;
} led_mode_config_t;

extern led_mode_config_t hwled_mode_config;
extern struct _pio_config pio_config;


extern const struct _mode_command_struct hwled_commands[];
extern const uint32_t hwled_commands_count;