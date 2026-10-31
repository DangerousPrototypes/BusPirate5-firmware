/**
 * @file hwi2c.h
 * @brief Hardware I2C mode interface.
 * @details Implements I2C master mode using PIO-based bit-banging with optional
 *          clock stretching support. Includes device scanning, sensor demos,
 *          EEPROM access, and DDR SPD reading capabilities.
 */

/**
 * @brief I2C mode configuration structure.
 */
typedef struct _i2c_mode_config {
    uint32_t baudrate;       ///< I2C clock frequency in Hz
    uint32_t data_bits;      ///< Data bits (typically 8)
    bool clock_stretch;      ///< Enable clock stretching support
    bool ack_pending;        ///< ACK expected from slave
    bool read;               ///< Current operation is read
    bool start_sent;         ///< START condition already sent
} _i2c_mode_config;

/**
 * @brief Issue I2C START condition.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hwi2c_start(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Issue I2C STOP condition.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hwi2c_stop(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Write data to I2C bus.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hwi2c_write(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Read data from I2C bus.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hwi2c_read(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Execute I2C macro command.
 * @param macro  Macro number to execute
 */
void hwi2c_macro(uint32_t macro);

/**
 * @brief Interactive I2C mode setup.
 * @return 1 on success, 0 on failure
 */
uint32_t hwi2c_setup(void);

/**
 * @brief Configure I2C mode parameters.
 * @return true on success, false on failure
 */
bool hwi2c_configure(void);

/**
 * @brief Execute I2C mode setup.
 * @return 1 on success, 0 on failure
 */
uint32_t hwi2c_setup_exc(void);

/**
 * @brief Cleanup and disable I2C mode.
 */
void hwi2c_cleanup(void);

/**
 * @brief Display current I2C settings.
 */
void hwi2c_settings(void);

/**
 * @brief Display I2C configuration flags.
 */
void hwi2c_printI2Cflags(void);

/**
 * @brief Display I2C mode help.
 */
void hwi2c_help(void);

/**
 * @brief Check for short circuit on I2C bus.
 * @return Error code (0=OK)
 */
uint8_t hwi2c_checkshort(void);

/**
 * @brief Get current I2C speed.
 * @return Speed in Hz
 */
uint32_t hwi2c_get_speed(void);

/**
 * @brief Set I2C speed.
 * @param speed_hz  Speed in Hz
 */
void hwi2c_set_speed(uint32_t speed_hz);

/**
 * @brief Set I2C data bits.
 * @param bits  Data bits (typically 8)
 */
void hwi2c_set_databits(uint32_t bits);

/**
 * @brief Perform I2C mode sanity checks.
 * @return true if all checks pass, false otherwise
 */
bool hwi2c_preflight_sanity_check(void);

/**
 * @brief Configure I2C mode for binary protocol.
 * @param bpio_mode_config  Binary protocol configuration
 * @return true on success, false on failure
 */
bool bpio_hwi2c_configure(bpio_mode_configuration_t *bpio_mode_config);

extern struct _i2c_mode_config i2c_mode_config;
extern const struct _mode_command_struct hwi2c_commands[];
extern const uint32_t hwi2c_commands_count;
extern const struct bp_command_def i2c_setup_def;
