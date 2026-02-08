/**
 * @file pwm.h
 * @brief PWM (Pulse Width Modulation) generation command interface (g/G commands).
 * @details Provides commands for generating PWM signals on I/O pins using
 *          RP2040/RP2350 hardware PWM peripheral.
 */

/**
 * @brief Handler for PWM enable command (G).
 * @param res  Command result structure
 * @note Syntax: G to enable with interactive menu
 */
void pwm_configure_enable(struct command_result* res);

/**
 * @brief Handler for PWM disable command (g).
 * @param res  Command result structure
 */
void pwm_configure_disable(struct command_result* res);

/**
 * @brief Calculate PWM parameters for desired frequency.
 * @param[in,out] freq_hz_value  Desired frequency in Hz
 * @param[out] pwm_hz_actual     Actual achievable frequency
 * @param[out] pwm_ns_actual     Actual period in nanoseconds
 * @param[out] pwm_divider       Clock divider value
 * @param[out] pwm_top           PWM counter wrap value
 * @return 0 on success, error code on failure
 */
uint8_t pwm_freq_find(
    float* freq_hz_value, float* pwm_hz_actual, float* pwm_ns_actual, uint32_t* pwm_divider, uint32_t* pwm_top);
