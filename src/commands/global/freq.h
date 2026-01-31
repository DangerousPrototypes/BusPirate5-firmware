/**
 * @file freq.h
 * @brief Frequency measurement command interface (f/F commands).
 * @details Provides commands for measuring signal frequency and duty cycle
 *          on I/O pins using PWM hardware in counter mode.
 */

/**
 * @brief Handler for continuous frequency measurement (F).
 * @param res  Command result structure
 */
void freq_cont(struct command_result* res);

/**
 * @brief Handler for single frequency measurement (f).
 * @param res  Command result structure
 */
void freq_single(struct command_result* res);

/**
 * @brief Measure frequency on specified pin.
 * @param pin      I/O pin number (0-7)
 * @param refresh  true for continuous updates, false for single measurement
 * @return 1 on success, 0 on failure
 */
uint32_t freq_measure(int32_t pin, int refresh);

/**
 * @brief Interactive menu to disable frequency measurement.
 * @return 1 on success, 0 on failure
 */
uint32_t freq_configure_disable(void);

/**
 * @brief Interactive menu to enable continuous frequency measurement.
 * @return 1 on success, 0 on failure
 */
uint32_t freq_configure_enable(void);

/**
 * @brief Measure signal period on GPIO pin.
 * @param gpio  GPIO pin number
 * @return Period in microseconds
 */
float freq_measure_period(uint gpio);

/**
 * @brief Measure duty cycle on GPIO pin.
 * @param gpio  GPIO pin number
 * @return Duty cycle as percentage (0.0 to 100.0)
 */
float freq_measure_duty_cycle(uint gpio);

/**
 * @brief Convert frequency to human-readable format.
 * @param[in] freq_hz_value          Frequency in Hz
 * @param[out] freq_friendly_value   Scaled value for display
 * @param[out] freq_friendly_units   Unit index (0=Hz, 1=kHz, 2=MHz)
 */
void freq_display_hz(float* freq_hz_value, float* freq_friendly_value, uint8_t* freq_friendly_units);

/**
 * @brief Convert period to human-readable format.
 * @param[in] freq_ns_value           Period in nanoseconds
 * @param[out] period_friendly_value  Scaled value for display
 * @param[out] period_friendly_units  Unit index (0=ns, 1=us, 2=ms)
 */
void freq_display_ns(float* freq_ns_value, float* period_friendly_value, uint8_t* period_friendly_units);

/**
 * @brief IRQ handler for period measurement.
 */
void freq_measure_period_irq(void);