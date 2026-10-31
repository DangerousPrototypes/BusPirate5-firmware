/**
 * @file cls.h
 * @brief Clear screen command interface.
 * @details Provides command to clear terminal display.
 */

/**
 * @brief Clear screen and redisplay statusbar.
 * @param res  Command result structure
 */
extern const struct bp_command_def cls_def;
void ui_display_clear(struct command_result* res);