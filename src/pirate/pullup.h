/**
 * @file pullup.h
 * @brief Programmable pullup/pulldown resistor control interface.
 * @details Controls I2C-based resistor networks (MCP23017 expanders) that provide
 *          selectable pullup/pulldown resistors for each I/O pin. Resistor values
 *          can be combined in parallel to achieve standard values from 1.3K to 1M ohms.
 */

/**
 * @brief Available pullup/pulldown resistor values.
 * @details Values achieved by parallel combinations of 2.2K, 4.7K, 10K, and 1M resistors.
 */
enum {
    PULLX_OFF=0,  ///< No pullup/pulldown
    PULLX_1K3,    ///< 1.3K ohms (2.2K || 4.7K || 10K)
    PULLX_1K5,    ///< 1.5K ohms (2.2K || 4.7K)
    PULLX_1K8,    ///< 1.8K ohms (2.2K || 10K)
    PULLX_2K2,    ///< 2.2K ohms
    PULLX_3K2,    ///< 3.2K ohms (4.7K || 10K)
    PULLX_4K7,    ///< 4.7K ohms
    PULLX_10K,    ///< 10K ohms
    PULLX_1M,     ///< 1M ohms
};

/**
 * @brief Pullup/pulldown resistor configuration.
 * @details Describes which parallel resistors to enable for a given value.
 */
struct pullx_options_t {
    uint8_t pull;            ///< Resistor value from pullup enum
    const char name[5];      ///< Human-readable name (e.g., "10K")
    uint8_t resistors;       ///< Bitfield of parallel resistors to enable
};

/**
 * @brief Lookup table of all pullup/pulldown configurations.
 */
extern const struct pullx_options_t pullx_options[9];

/**
 * @brief Current pullup/pulldown value for each of 8 I/O pins.
 */
extern uint8_t pullx_value[8];

/**
 * @brief Pullup/pulldown direction mask (0=pulldown, 1=pullup).
 */
extern uint8_t pullx_direction;

/**
 * @brief Initialize pullup/pulldown hardware (I2C expanders).
 * @note Must be called before any other pullup operations.
 */
void pullup_init(void);

/**
 * @brief Enable pullup/pulldown resistors globally.
 */
void pullup_enable(void);

/**
 * @brief Disable pullup/pulldown resistors globally.
 */
void pullup_disable(void);

/**
 * @brief Low-level test function to set raw resistor and direction masks.
 * @param resistor_mask  16-bit mask of resistors to enable
 * @param direction_mask 16-bit direction mask (0=low, 1=high)
 * @warning Debug/test function, prefer pullx_set_pin() for normal use.
 */
void pullx_set_all_test(uint16_t resistor_mask, uint16_t direction_mask);

/**
 * @brief Set all pins to the same pullup/pulldown value.
 * @param pull     Resistor value from pullup enum
 * @param pull_up  true for pullup, false for pulldown
 */
void pullx_set_all_update(uint8_t pull, bool pull_up);

/**
 * @brief Set pullup/pulldown for a single pin.
 * @param pin      Pin number (0-7)
 * @param pull     Resistor value from pullup enum
 * @param pull_up  true for pullup, false for pulldown
 */
void pullx_set_pin(uint8_t pin, uint8_t pull, bool pull_up);

/**
 * @brief Apply pending pullup/pulldown changes to hardware.
 * @return true on success, false on I2C error
 * @note Changes made by pullx_set_pin() are cached until pullx_update() is called.
 */
bool pullx_update(void);

/**
 * @brief Read current pullup/pulldown configuration for a pin.
 * @param pin       Pin number (0-7)
 * @param[out] pull Resistor value (pullup enum)
 * @param[out] pull_up true if pullup, false if pulldown
 */
void pullx_get_pin(uint8_t pin, uint8_t *pull, bool *pull_up);