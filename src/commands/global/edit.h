/**
 * @file edit.h
 * @brief Embedded text editor command interface.
 * @details Phase 0: Minimal kilo-inspired editor for testing VT100 alternate
 *          screen, keyboard input, and BIG_BUFFER text storage.
 */

void edit_handler(struct command_result* res);
extern const struct bp_command_def edit_def;
