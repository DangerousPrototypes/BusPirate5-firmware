/**
 * @file a_auxio.h
 * @brief Auxiliary I/O control commands (a/A/@ commands).
 * @details Provides direct control of individual I/O pins as auxiliary inputs/outputs,
 *          separate from current protocol mode.
 */

extern const struct bp_command_def auxio_high_def;
extern const struct bp_command_def auxio_low_def;
extern const struct bp_command_def auxio_input_def;

/**
 * @brief Handler for aux pin high command (A).
 * @param res  Command result structure
 * @note Syntax: A \<pin\> - Set pin as output high
 */
void auxio_high_handler(struct command_result* res);

/**
 * @brief Handler for aux pin low command (a).
 * @param res  Command result structure
 * @note Syntax: a \<pin\> - Set pin as output low
 */
void auxio_low_handler(struct command_result* res);

/**
 * @brief Handler for aux pin input/read command (@).
 * @param res  Command result structure
 * @note Syntax: @ \<pin\> - Set pin as input and read value
 */
void auxio_input_handler(struct command_result* res);
