/**
 * @file hw2wire.h
 * @brief 2-wire PIO protocol mode interface.
 * @details Provides 2-wire protocol mode using PIO state machines.
 */

void hw2wire_start(struct _bytecode* result, struct _bytecode* next);
void hw2wire_start_alt(struct _bytecode* result, struct _bytecode* next);
void hw2wire_stop(struct _bytecode* result, struct _bytecode* next);
void hw2wire_stop_alt(struct _bytecode* result, struct _bytecode* next);
void hw2wire_write(struct _bytecode* result, struct _bytecode* next);
void hw2wire_read(struct _bytecode* result, struct _bytecode* next);
void hw2wire_tick_clock(struct _bytecode* result, struct _bytecode* next);
void hw2wire_set_clk_high(struct _bytecode* result, struct _bytecode* next);
void hw2wire_set_clk_low(struct _bytecode* result, struct _bytecode* next);
void hw2wire_set_dat_high(struct _bytecode* result, struct _bytecode* next);
void hw2wire_set_dat_low(struct _bytecode* result, struct _bytecode* next);
void hw2wire_read_bit(struct _bytecode* result, struct _bytecode* next);
void hw2wire_macro(uint32_t macro);
uint32_t hw2wire_setup(void);
uint32_t hw2wire_setup_exc(void);
void hw2wire_cleanup(void);
void hw2wire_settings(void);
void hw2wire_printI2Cflags(void);
void hw2wire_help(void);
uint32_t hw2wire_get_speed(void);
bool hw2wire_preflight_sanity_check(void);
bool bpio_hw2wire_configure(bpio_mode_configuration_t *bpio_mode_config);

typedef struct _hw2wire_mode_config {
    uint32_t baudrate;
    // uint32_t baudrate_actual;
    uint32_t data_bits;
    bool ack_pending;
    bool read;
    bool start_sent;
} _hw2wire_mode_config;

extern const struct _mode_command_struct hw2wire_commands[];
extern const uint32_t hw2wire_commands_count;
extern struct _hw2wire_mode_config hw2wire_mode_config;
