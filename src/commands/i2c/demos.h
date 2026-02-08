/**
 * @file demos.h
 * @brief I2C sensor demonstration commands interface.
 * @details Provides demo commands for common I2C sensors.
 */

/**
 * @brief Demo TSL2561 light sensor.
 * @param res  Command result structure
 */
void demo_tsl2561(struct command_result* res);

/**
 * @brief Demo MS5611 barometric pressure sensor.
 * @param res  Command result structure
 */
void demo_ms5611(struct command_result* res);

/**
 * @brief Demo SI7021 humidity/temperature sensor.
 * @param res  Command result structure
 */
void demo_si7021(struct command_result* res);

/**
 * @brief Demo TCS34725 color sensor.
 * @param res  Command result structure
 */
void demo_tcs34725(struct command_result* res);

/**
 * @brief Demo SHT3x humidity/temperature sensor.
 * @param res  Command result structure
 */
void demo_sht3x(struct command_result* res);

/**
 * @brief Demo SHT4x humidity/temperature sensor.
 * @param res  Command result structure
 */
void demo_sht4x(struct command_result* res);