/**
 * @file demos.h
 * @brief 1-Wire sensor demonstration commands interface.
 * @details Provides demo commands for 1-Wire temperature sensors.
 */

/**
 * @brief Demo DS18B20 temperature sensor.
 * @param res  Command result structure
 */
void onewire_test_ds18b20_conversion(struct command_result* res);

extern const struct bp_command_def ds18b20_def;