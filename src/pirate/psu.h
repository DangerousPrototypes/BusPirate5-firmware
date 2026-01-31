/**
 * @file psu.h
 * @brief Power supply unit (PSU) control and monitoring interface.
 * @details Provides voltage regulation (0.8V to 5V) and current limiting (0-500mA)
 *          with overcurrent protection, undervoltage monitoring, and backflow detection.
 *          Supports both PWM and I2C DAC control depending on hardware revision.
 */

/**
 * @brief PSU operation result codes.
 */
enum psu_errors {
    PSU_OK = 0,                    ///< PSU operation successful
    PSU_ERROR_FUSE_TRIPPED,        ///< Current limit fuse tripped (overcurrent)
    PSU_ERROR_VOUT_LOW,            ///< Output voltage below undervoltage threshold
    PSU_ERROR_BACKFLOW             ///< Backflow detected from VOUT to VREG
};

    //uint8_t psu; // psu (0=off, 1=on)
    //uint32_t psu_voltage;   // psu voltage output setting in decimal * 10000
    //bool psu_current_limit_en;
    //uint32_t psu_current_limit; // psu current limit in decimal * 10000
    //bool psu_current_error;     // psu over current limit fuse tripped
    //bool psu_undervoltage_error;
    //bool psu_error;             // error, usually with the dac
    //bool psu_irq_en;

/**
 * @brief PSU status and configuration structure.
 * @details Tracks all PSU parameters including requested vs actual voltages/currents,
 *          DAC values, limit thresholds, and error states.
 */
struct psu_status_t {
    float voltage_requested_float;       ///< User-requested voltage (volts)
    float voltage_actual_float;          ///< Actual achievable voltage after rounding (volts)
    uint32_t voltage_actual_int;         ///< Actual voltage in scaled integer format (mV * 10)
    uint16_t voltage_dac_value;          ///< DAC/PWM value for voltage control (inverted for VREG margining)
    float current_requested_float;       ///< User-requested current limit (mA)
    float current_actual_float;          ///< Actual achievable current after rounding (mA)
    uint32_t current_actual_int;         ///< Actual current in scaled integer format (mA * 10000)
    uint16_t current_dac_value;          ///< DAC/PWM value for current limit control
    uint8_t undervoltage_percent;        ///< Undervoltage threshold as percentage below target
    uint32_t undervoltage_limit_int;     ///< Undervoltage limit for display (mV)
    uint16_t undervoltage_limit_adc;     ///< Undervoltage limit as raw ADC counts for fast comparison
    bool current_limit_override;         ///< true if current limiting is disabled
    bool undervoltage_limit_override;    ///< true if undervoltage monitoring is disabled
    bool enabled;                        ///< true if PSU is currently enabled
    bool error_overcurrent;              ///< true if overcurrent fault detected
    bool error_undervoltage;             ///< true if undervoltage fault detected
    bool error_pending;                  ///< true if an error occurred and needs user attention
};

/**
 * @brief Global PSU status and configuration instance.
 */
extern struct psu_status_t psu_status;

/**
 * @brief Initialize PSU hardware and reset status.
 * @note Must be called before any other PSU operations.
 */
void psu_init(void);

/**
 * @brief Enable PSU with specified voltage, current limit, and protection settings.
 * @param volts                   Requested output voltage (0.8V to 5.0V)
 * @param current                 Current limit in mA (0 to 500mA)
 * @param current_limit_enabled   false to disable current limiting, true to enforce
 * @param voltage_lag_percent     Undervoltage threshold percentage (1-100, 100=disabled)
 * @return PSU_OK on success, or PSU_ERROR_* code on failure
 * @warning Temporarily engages current override during startup to prevent inrush trips.
 */
uint32_t psu_enable(float volts, float current, bool current_limit_enabled, uint8_t voltage_lag_percent);

/**
 * @brief Disable PSU and reset hardware to safe state.
 */
void psu_disable(void);

/**
 * @brief Read all PSU measurements in a single ADC sweep.
 * @param[out] vout   VOUT voltage measurement
 * @param[out] isense Current sense measurement (mA)
 * @param[out] vreg   VREG voltage measurement
 * @param[out] fuse   Fuse status (true=OK, false=blown)
 */
void psu_measure(uint32_t* vout, uint32_t* isense, uint32_t* vreg, bool* fuse);

/**
 * @brief Measure VOUT voltage.
 * @return VOUT voltage in scaled format
 */
uint32_t psu_measure_vout(void);

/**
 * @brief Measure VREG voltage.
 * @return VREG voltage in scaled format
 */
uint32_t psu_measure_vreg(void);

/**
 * @brief Measure output current.
 * @return Current in milliamps (mA)
 */
uint32_t psu_measure_current(void);

/**
 * @brief Check current fuse/limit status.
 * @return true if fuse OK, false if blown or overcurrent detected
 */
bool psu_fuse_ok(void);

/**
 * @brief Check if VOUT is above undervoltage limit.
 * @return true if voltage OK, false if undervoltage detected
 */
bool psu_vout_ok(void);

/**
 * @brief Enable or disable VREG connection to output.
 * @param enable  true to connect VREG, false to disconnect
 */
void psu_vreg_enable(bool enable);

/**
 * @brief Override current limiting circuitry.
 * @param enable  true to disable current limiting, false to enable
 * @warning Disabling current limit can damage hardware if load shorts.
 */
void psu_current_limit_override(bool enable);

/**
 * @brief Poll PSU sensors and handle fuse/undervoltage errors.
 * @return true if error detected and PSU disabled, false otherwise
 * @note Automatically disables PSU if error detected while enabled.
 */
bool psu_poll_fuse_vout_error(void);

/**
 * @brief Clear the pending error flag.
 * @note Call after user acknowledges error to allow PSU re-enable.
 */
void psu_clear_error_flag(void);
