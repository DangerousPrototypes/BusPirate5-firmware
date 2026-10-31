/**
 * @file jtag.h
 * @brief JTAG mode interface.
 * @details Provides JTAG protocol mode for debugging and programming.
 */

uint32_t jtag_setup(void);
uint32_t jtag_setup_exc(void);
void jtag_cleanup(void);
bool jtag_preflight_sanity_check(void);
void jtag_pins(void);
void jtag_settings(void);
void jtag_help(void);
uint32_t jtag_get_speed(void);


extern const struct _mode_command_struct jtag_commands[];
extern const uint32_t jtag_commands_count;
