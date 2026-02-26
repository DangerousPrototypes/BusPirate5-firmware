/**
 * @file hexedit.h
 * @brief Embedded hex editor command interface.
 * @details Port of krpors/hx hex editor for Bus Pirate VT100 terminal.
 */

void hexedit_handler(struct command_result* res);
extern const struct bp_command_def hexedit_def;
