/**
 * @file infrared.h
 * @brief Infrared protocol mode interface.
 * @details Provides infrared communication mode for remote control protocols.
 */

void infrared_write(struct _bytecode* result, struct _bytecode* next);
void infrared_read(struct _bytecode* result, struct _bytecode* next);
void infrared_start(struct _bytecode* result, struct _bytecode* next);
void infrared_stop(struct _bytecode* result, struct _bytecode* next);
void infrared_startr(void);
void infrared_stopr(void);
void infrared_macro(uint32_t macro);
void infrared_periodic(void);
uint32_t infrared_setup(void);
uint32_t infrared_setup_exc(void);
void infrared_cleanup(void);
bool infrared_preflight_sanity_check(void);
void infrared_settings(void);
const char* infrared_pins(void);
void infrared_help(void);
void infrared_wait_idle(void);

uint32_t infrared_get_speed(void);

//for pausing the PIO programs while other programs use the hardware
void infrared_setup_resume(void);
void infrared_cleanup_temp(void);

extern const struct _mode_command_struct infrared_commands[];
extern const uint32_t infrared_commands_count;

typedef struct _infrared_mode_config {
    uint32_t rx_sensor;
    uint32_t protocol;
    uint32_t tx_freq;
} _infrared_mode_config;
