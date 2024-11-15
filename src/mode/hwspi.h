void spi_start(struct _bytecode* result, struct _bytecode* next);
void spi_startr(struct _bytecode* result, struct _bytecode* next);
void spi_stop(struct _bytecode* result, struct _bytecode* next);
void spi_stopr(struct _bytecode* result, struct _bytecode* next);
void spi_write(struct _bytecode* result, struct _bytecode* next);
void spi_read(struct _bytecode* result, struct _bytecode* next);
void spi_macro(uint32_t macro);
uint32_t spi_setup(void);
uint32_t spi_binmode_get_config_length(void);
uint32_t spi_binmode_setup(uint8_t* config);
uint32_t spi_setup_exc(void);
void spi_cleanup(void);
void spi_pins(void);
void spi_settings(void);
void spi_printSPIflags(void);
void spi_help(void);
uint32_t spi_get_speed(void);

// special for binmode and lcd
void spi_setcpol(uint32_t val);
void spi_setcpha(uint32_t val);
void spi_setbr(uint32_t val);
void spi_setdff(uint32_t val);
void spi_setlsbfirst(uint32_t val);
void spi_set_cs_idle(uint32_t val);
void spi_set_cs(uint8_t cs);
uint8_t spi_xfer(const uint8_t out);

uint32_t spi_read_simple(void);
void spi_write_simple(uint32_t data);

typedef struct _spi_mode_config {
    uint32_t baudrate;
    uint32_t baudrate_actual;
    uint32_t data_bits;
    uint32_t clock_polarity;
    uint32_t clock_phase;
    uint32_t cs_idle;
    uint32_t dff;
    bool read_with_write;
    bool binmode;
} _spi_mode_config;

extern const struct _mode_command_struct hwspi_commands[];
extern const uint32_t hwspi_commands_count;
