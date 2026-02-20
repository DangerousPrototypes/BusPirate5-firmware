/**
 * @file i2s.h
 * @brief I2S audio protocol mode interface.
 * @details Provides I2S mode for digital audio communication.
 */

void i2s_write(struct _bytecode* result, struct _bytecode* next);
void i2s_read(struct _bytecode* result, struct _bytecode* next);
void i2s_start(struct _bytecode* result, struct _bytecode* next);
void i2s_stop(struct _bytecode* result, struct _bytecode* next);
void i2s_startr(struct _bytecode* result, struct _bytecode* next);
void i2s_stopr(struct _bytecode* result, struct _bytecode* next);
void i2s_periodic(void);
uint32_t i2s_setup(void);
uint32_t i2s_setup_exc(void);
void i2s_cleanup(void);
void i2s_settings(void);
const char* i2s_pins(void);
void i2s_clkh(struct _bytecode* result, struct _bytecode* next);
void i2s_clkl(struct _bytecode* result, struct _bytecode* next);
void i2s_dath(struct _bytecode* result, struct _bytecode* next);
void i2s_datl(struct _bytecode* result, struct _bytecode* next);
void i2s_dats(struct _bytecode* result, struct _bytecode* next);
void i2s_clk(struct _bytecode* result, struct _bytecode* next);
void i2s_bitr(struct _bytecode* result, struct _bytecode* next);

void i2s_help(void);

extern const struct _mode_command_struct i2s_commands[];
extern const uint32_t i2s_commands_count;
typedef struct _i2s_mode_config {
    uint32_t freq;
    uint32_t bits;
} _i2s_mode_config;
extern struct _i2s_mode_config i2s_mode_config;
extern struct _pio_config i2s_pio_config_out,  i2s_pio_config_in;
extern const struct bp_command_def i2s_setup_def;