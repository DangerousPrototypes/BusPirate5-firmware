void hwhduart_open(struct _bytecode* result, struct _bytecode* next); // start
void hwhduart_open_read(struct _bytecode* result, struct _bytecode* next);
void hwhduart_stop_alt(struct _bytecode* result, struct _bytecode* next);
void hwhduart_start_alt(struct _bytecode* result, struct _bytecode* next); // start with read
void hwhduart_close(struct _bytecode* result, struct _bytecode* next);     // stop
void hwhduart_write(struct _bytecode* result, struct _bytecode* next);
void hwhduart_read(struct _bytecode* result, struct _bytecode* next);
void hwhduart_macro(uint32_t macro);
uint32_t hwhduart_setup(void);
uint32_t hwhduart_setup_exc(void);
void hwhduart_cleanup(void);
void hwhduart_pins(void);
void hwhduart_settings(void);
void hwhduart_printerror(void);
void hwhduart_help(void);
void hwhduart_periodic(void);
uint32_t hwhduart_get_speed(void);

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
extern const struct _mode_command_struct hwhduart_commands[];
extern const uint32_t hwhduart_commands_count;