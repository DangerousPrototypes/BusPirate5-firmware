
void hwi2c_start(struct _bytecode* result, struct _bytecode* next);
void hwi2c_stop(struct _bytecode* result, struct _bytecode* next);
void hwi2c_write(struct _bytecode* result, struct _bytecode* next);
void hwi2c_read(struct _bytecode* result, struct _bytecode* next);
void hwi2c_macro(uint32_t macro);
uint32_t hwi2c_setup(void);
bool hwi2c_configure(void);
uint32_t hwi2c_setup_exc(void);
void hwi2c_cleanup(void);
// void hwi2c_pins(void);
void hwi2c_settings(void);
void hwi2c_printI2Cflags(void);
void hwi2c_help(void);
uint8_t hwi2c_checkshort(void);
uint32_t hwi2c_get_speed(void);
void hwi2c_set_speed(uint32_t speed_hz);
void hwi2c_set_databits(uint32_t bits);
bool hwi2c_preflight_sanity_check(void);
bool bpio_hwi2c_configure(bpio_mode_configuration_t *bpio_mode_config);

typedef struct _i2c_mode_config {
    uint32_t baudrate;
    // uint32_t baudrate_actual;
    uint32_t data_bits;
    bool clock_stretch;
    bool ack_pending;
    bool read;
    bool start_sent;
} _i2c_mode_config;

extern const struct _mode_command_struct hwi2c_commands[];
extern const uint32_t hwi2c_commands_count;
