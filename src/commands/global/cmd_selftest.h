/**
 * @file cmd_selftest.h
 * @brief Hardware self-test command interface.
 * @details Provides command to run hardware validation tests.
 */

/**
 * @brief Handler for self-test command.
 * @param res  Command result structure
 */
void cmd_selftest_handler(struct command_result* res);

/**
 * @brief Run hardware self-test.
 * @note Tests PSU, pullups, ADC, and other hardware subsystems
 */
void cmd_selftest(void);