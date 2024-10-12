void hwuart_open(struct _bytecode* result, struct _bytecode* next);      // start
void hwuart_open_read(struct _bytecode* result, struct _bytecode* next); // start with read
void hwuart_close(struct _bytecode* result, struct _bytecode* next);     // stop
void hwuart_write(struct _bytecode* result, struct _bytecode* next);
void hwuart_read(struct _bytecode* result, struct _bytecode* next);
void hwuart_macro(uint32_t macro);
uint32_t hwuart_setup(void);
uint32_t hwuart_setup_exc(void);
void hwuart_cleanup(void);
void hwuart_pins(void);
void hwuart_settings(void);
void hwuart_printerror(void);
void hwuart_help(void);
void hwuart_periodic(void);
void hwuart_wait_done(void);
uint32_t hwuart_get_speed(void);

typedef struct _uart_mode_config {
    uint32_t baudrate;
    uint32_t baudrate_actual;
    uint32_t data_bits;
    uint32_t stop_bits;
    uint32_t parity;
    uint32_t blocking;
    bool async_print;
    uint32_t flow_control;
    uint32_t invert;
} _uart_mode_config;

extern const struct _command_struct hwuart_commands[];
extern const uint32_t hwuart_commands_count;