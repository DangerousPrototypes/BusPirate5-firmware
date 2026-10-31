/**
 * @file txtest.h
 * @brief TX pipeline saturation test command.
 * @details Blasts known patterns through layers of the USB TX pipeline
 *          to detect byte-drop bugs. Hidden debug command.
 */

#ifndef TXTEST_H
#define TXTEST_H

extern const struct bp_command_def txtest_def;

void txtest_handler(struct command_result* res);

#endif /* TXTEST_H */
