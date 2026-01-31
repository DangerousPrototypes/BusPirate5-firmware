/**
 * @file v_adc.h
 * @brief ADC voltage measurement command interface (v/V commands).
 * @details Provides commands for measuring voltages on I/O pins using
 *          the analog multiplexer and 12-bit ADC.
 */

/**
 * @brief Handler for single voltage measurement (v).
 * @param res  Command result structure
 * @note Syntax: v [pin] - measures specified pin or all pins if omitted
 */
void adc_measure_single(struct command_result* res);

/**
 * @brief Handler for continuous voltage measurement (V).
 * @param res  Command result structure
 * @note Syntax: V [pin] - continuously measures specified pin or all pins
 */
void adc_measure_cont(struct command_result* res);
