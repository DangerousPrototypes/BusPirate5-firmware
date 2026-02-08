/**
 * @file dio.h
 * @brief Digital I/O mode interface.
 * @details Provides direct digital I/O pin control mode.
 */

void dio_write(struct _bytecode* result, struct _bytecode* next);
void dio_read(struct _bytecode* result, struct _bytecode* next);
void dio_start(struct _bytecode* result, struct _bytecode* next);
void dio_stop(struct _bytecode* result, struct _bytecode* next);
void dio_startr(void);
void dio_stopr(void);
void dio_macro(uint32_t macro);
uint32_t dio_periodic(void);
uint32_t dio_setup(void);
uint32_t dio_setup_exc(void);
void dio_cleanup(void);
bool dio_preflight_sanity_check(void);
void dio_settings(void);
uint32_t dio_get_speed(void);
const char* dio_pins(void);
void dio_clkh(void);
void dio_clkl(void);
void dio_dath(void);
void dio_datl(void);
uint32_t dio_dats(void);
void dio_clk(void);
uint32_t dio_bitr(void);

void dio_help(void);

extern const struct _mode_command_struct dio_commands[];
extern const uint32_t dio_commands_count;
