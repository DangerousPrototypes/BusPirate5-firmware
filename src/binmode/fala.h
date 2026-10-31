/**
 * @file fala.h
 * @brief Follow-Along Logic Analyzer interface.
 * @details Provides automatic logic analyzer capture during protocol operations.
 */

#ifndef FALA_H
#define FALA_H

/**
 * @brief FALA configuration structure.
 */
typedef struct {
    uint32_t base_frequency;         /**< Base sampling frequency */
    uint32_t oversample;             /**< Oversampling rate */
    uint32_t actual_sample_frequency; /**< Actual sampling frequency */
    uint8_t debug_level;             /**< Debug verbosity level */
} FalaConfig;

extern FalaConfig fala_config;

/**
 * @brief Set FALA sampling frequency.
 * @param freq  Frequency in Hz
 */
void fala_set_freq(uint32_t freq);

/**
 * @brief Set FALA oversampling rate.
 * @param oversample_rate  Oversampling factor
 */
void fala_set_oversample(uint32_t oversample_rate);

/**
 * @brief Set FALA trigger configuration.
 * @param trigger_pin    Trigger pin number
 * @param trigger_level  Trigger level (0=low, 1=high)
 */
void fala_set_triggers(uint8_t trigger_pin, uint8_t trigger_level);

/**
 * @brief Start FALA capture.
 */
void fala_start(void);

/**
 * @brief Stop FALA capture.
 */
void fala_stop(void);

/**
 * @brief Print FALA capture results.
 */
void fala_print_result(void);

/**
 * @brief FALA start hook.
 */
void fala_start_hook(void);

/**
 * @brief FALA stop hook.
 */
void fala_stop_hook(void);

/**
 * @brief FALA notify hook.
 */
void fala_notify_hook(void);

/**
 * @brief Register FALA notification callback.
 * @param hook  Callback function
 * @return      true on success
 */
bool fala_notify_register(void (*hook)());

/**
 * @brief Unregister FALA notification callback.
 * @param hook  Callback function
 */
void fala_notify_unregister(void (*hook)());

/**
 * @brief FALA mode change hook.
 */
void fala_mode_change_hook(void);

#endif // FALA_H