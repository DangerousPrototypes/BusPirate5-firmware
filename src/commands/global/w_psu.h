/**
 * @file w_psu.h
 * @brief Power supply (PSU) control command interface (w/W commands).
 * @details Provides commands to enable/disable the programmable power supply
 *          with voltage, current limiting, and undervoltage protection.
 */

extern const struct bp_command_def psucmd_enable_def;
extern const struct bp_command_def psucmd_disable_def;

/**
 * @brief Initialize PSU command subsystem.
 * @return true on success, false on failure
 */
bool psucmd_init(void);

/**
 * @brief Handler for PSU enable command (W).
 * @param res  Command result structure
 * @note Syntax: W \<volts\> \<mA\> [-u \<percent\>]
 */
void psucmd_enable_handler(struct command_result* res);

/**
 * @brief Enable PSU with specified parameters.
 * @param volts                   Output voltage (0.8V to 5.0V)
 * @param current                 Current limit in mA (0-500mA)
 * @param current_limit_override  true to disable current limiting
 * @param undervoltage_percent    Undervoltage threshold percent (1-100, 100=disabled)
 * @return PSU_OK on success, or PSU_ERROR_* code on failure
 */
uint32_t psucmd_enable(float volts, float current, bool current_limit_override, uint8_t undervoltage_percent);

/**
 * @brief Handler for PSU disable command (w).
 * @param res  Command result structure
 */
void psucmd_disable_handler(struct command_result* res);

/**
 * @brief Disable PSU and reset to safe state.
 */
void psucmd_disable(void);

/**
 * @brief IRQ callback for PSU fault conditions.
 * @note Called when overcurrent or undervoltage fault occurs.
 */
void psucmd_irq_callback(void);

/**
 * @brief Display and clear PSU error status.
 */
void psucmd_show_clear_error(void);