/**
 * @file hiz.h
 * @brief High-impedance (HiZ) safe mode interface.
 * @details HiZ mode is the default safe mode where all I/O pins are set to
 *          high-impedance inputs, power supply is disabled, and pull-ups are off.
 *          This mode prevents accidental damage to connected circuits.
 */

/**
 * @brief Get pin label string for HiZ mode.
 * @return String of dashes indicating no active pins
 */
const char* hiz_pins(void);

/**
 * @brief Get error message for commands in HiZ mode.
 * @return Error message indicating no effect in HiZ mode
 */
const char* hiz_error(void);

/**
 * @brief Cleanup function (no-op for HiZ mode).
 */
void hiz_cleanup(void);

/**
 * @brief Setup function for HiZ mode.
 * @return Always returns 1 (success)
 */
uint32_t hiz_setup(void);

/**
 * @brief Execute HiZ mode setup - disable all peripherals.
 * @details Disables PSU, pull-ups, PWM, and sets all pins to safe state.
 * @return Always returns 1 (success)
 */
uint32_t hiz_setup_exec(void);

/**
 * @brief Display HiZ mode help information.
 */
void hiz_help(void);

/**
 * @brief Configure HiZ mode for binary protocol.
 * @param bpio_mode_config  Binary protocol configuration (unused)
 * @return Always returns true
 */
bool bpio_hiz_configure(bpio_mode_configuration_t *bpio_mode_config);

/**
 * @brief Number of mode-specific commands in HiZ mode.
 */
extern const uint32_t hiz_commands_count;

/**
 * @brief Array of mode-specific commands (empty for HiZ).
 */
extern const struct _mode_command_struct hiz_commands[];
