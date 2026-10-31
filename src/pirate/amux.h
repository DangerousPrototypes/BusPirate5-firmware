/**
 * @file amux.h
 * @brief Analog multiplexer (AMUX) interface and ADC control
 * 
 * Declares functions for analog multiplexer control and ADC measurements.
 * 
 * @author Bus Pirate Project  
 * @date 2024-2026
 */

/**
 * @brief Initialize analog multiplexer and ADC
 */
void amux_init(void);

/**
 * @brief Select AMUX input channel
 * @param channel Channel number (platform-specific)
 * @return true if successful, false if scope running
 * @note Only affects 4067 analog mux, not current sense
 */
bool amux_select_input(uint16_t channel);

/**
 * @brief Select AMUX input using BIO pin number
 * @param bio BIO pin number (0-7)
 * @return true if successful, false if scope running
 */
bool amux_select_bio(uint8_t bio);

/**
 * @brief Read voltage from AMUX channel
 * @param channel Channel number
 * @return 12-bit ADC value (0-4095)
 */
uint32_t amux_read(uint8_t channel);

/**
 * @brief Read from currently selected AMUX channel
 * @return 12-bit ADC value (0-4095)
 */
uint32_t amux_read_present_channel(void);

/**
 * @brief Read voltage from BIO pin
 * @param bio BIO pin number (0-7)
 * @return 12-bit ADC value (0-4095)
 */
uint32_t amux_read_bio(uint8_t bio);

/**
 * @brief Read current sense (separate ADC channel)
 * @return 12-bit ADC value (0-4095)
 * @note Voltage not divided by 2, uses full ADC range
 */
uint32_t amux_read_current(void);

/**
 * @brief Read all AMUX channels and current sense
 * 
 * Sweeps all channels and stores results in global arrays:
 * - hw_adc_raw[]: Raw ADC values
 * - hw_adc_voltage[]: Converted voltages
 */
void amux_sweep(void);

/**
 * @brief Control ADC busy/lock state
 * @param enable true to acquire lock, false to release
 */
void adc_busy_wait(bool enable);

/**
 * @brief Flag to reset averaging
 * 
 * Set to true to reset averaging of all channels and start with current value.
 * Useful when expecting step-change of inputs.
 */
extern bool reset_adc_average;

/** Number of samples for averaging (power of 2 for efficient division) */
#define ADC_AVG_TIMES 64

/**
 * @brief Calculate average from sum
 * 
 * @param avgsum Sum of ADC_AVG_TIMES samples
 * @return Averaged value with correct rounding
 * 
 * @note Uses power-of-2 division for efficiency
 * @note Adds ADC_AVG_TIMES/2 before division for proper rounding
 */
inline uint32_t get_adc_average(uint32_t avgsum)
{
    // add ADC_AVG_TIMES/2 before division for correct rounding instead of cutting of decimals
    return ((avgsum+(ADC_AVG_TIMES/2))/ADC_AVG_TIMES);
}
