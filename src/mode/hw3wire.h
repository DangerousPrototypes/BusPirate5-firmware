/**
 * @file hw3wire.h
 * @brief 3-wire PIO protocol mode interface.
 * @details Provides 3-wire protocol mode using PIO state machines.
 */

void hw3wire_start(struct _bytecode* result, struct _bytecode* next);
void hw3wire_start_alt(struct _bytecode* result, struct _bytecode* next);
void hw3wire_stop(struct _bytecode* result, struct _bytecode* next);
void hw3wire_stop_alt(struct _bytecode* result, struct _bytecode* next);
void hw3wire_write(struct _bytecode* result, struct _bytecode* next);
void hw3wire_read(struct _bytecode* result, struct _bytecode* next);
void hw3wire_tick_clock(struct _bytecode* result, struct _bytecode* next);
void hw3wire_set_clk_high(struct _bytecode* result, struct _bytecode* next);
void hw3wire_set_clk_low(struct _bytecode* result, struct _bytecode* next);
void hw3wire_set_dat_high(struct _bytecode* result, struct _bytecode* next);
void hw3wire_set_dat_low(struct _bytecode* result, struct _bytecode* next);
void hw3wire_read_bit(struct _bytecode* result, struct _bytecode* next);
void hw3wire_macro(uint32_t macro);
uint32_t hw3wire_setup(void);
uint32_t hw3wire_setup_exc(void);
void hw3wire_cleanup(void);
void hw3wire_set_cs(uint8_t cs);
void hw3wire_settings(void);
void hw3wire_printI2Cflags(void);
void hw3wire_help(void);
uint32_t hw3wire_get_speed(void);
bool hw3wire_preflight_sanity_check(void);
bool bpio_hw3wire_configure(bpio_mode_configuration_t *bpio_mode_config);

typedef struct _hw3wire_mode_config {
    uint32_t baudrate;
    // uint32_t baudrate_actual;
    uint32_t cs_idle;
    bool read_with_write;
} _hw3wire_mode_config;

extern const struct _mode_command_struct hw3wire_commands[];
extern const uint32_t hw3wire_commands_count;
//extern struct _hw3wire_mode_config hw3wire_mode_config;
extern const struct bp_command_def hw3wire_setup_def;
