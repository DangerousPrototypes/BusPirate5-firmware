/**
 * @file ui_const.h
 * @brief UI string constants and enumerations.
 * @details Provides constant arrays for display strings:
 *          frequency units, bit orders, display formats, pin states.
 */

/**
 * @brief Input/output mode translation keys.
 */
static const int ui_const_input_output[] = { T_INPUT, T_OUTPUT };

/**
 * @brief Frequency/period measurement units.
 */
enum ui_const_freq_measures {
    freq_ns,      ///< Nanoseconds
    freq_us,      ///< Microseconds
    freq_ms,      ///< Milliseconds
    freq_hz,      ///< Hertz
    freq_khz,     ///< Kilohertz
    freq_mhz,     ///< Megahertz
    freq_percent  ///< Percentage (duty cycle)
};

/**
 * @brief Frequency unit slugs (lowercase for user input).
 */
static const char ui_const_freq_slugs[][4] = { "ns", "us", "ms", "hz", "khz", "mhz", "%" };

/**
 * @brief Frequency unit labels (for display).
 */
static const char ui_const_freq_labels[][4] = { "ns", "us", "ms", "Hz", "kHz", "MHz", "%" };

/**
 * @brief Short frequency labels (for constrained spaces like toolbar).
 */
static const char ui_const_freq_labels_short[][2] = { "n", "u", "m", "H", "K", "M", "%" };

/**
 * @brief Bit order options.
 */
static const char ui_const_bit_orders[][4] = { "MSB", "LSB" };

/**
 * @brief Pin state strings.
 */
static const char ui_const_pin_states[][6] = { "OFF", "ON", "GND", "PWM", "FREQ", "ERR" };

/**
 * @brief Display format enumeration.
 */
enum ui_const_display_formats_enum {
    df_auto,   ///< Auto-detect format
    df_hex,    ///< Hexadecimal
    df_dec,    ///< Decimal
    df_bin,    ///< Binary
    df_ascii   ///< ASCII
};

/**
 * @brief Display format strings.
 */
static const char ui_const_display_formats[][6] = { "Auto",
                                                    "HEX",
                                                    "DEC",
                                                    "BIN",
                                                    "ASCII" };

/**
 * @brief Hexadecimal digit characters.
 */
static const char ascii_hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };