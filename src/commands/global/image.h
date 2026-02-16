/**
 * @file image.h
 * @brief Image display command interface.
 * @details Provides command to display images on LCD screen.
 */

/**
 * @brief Handler for image display command.
 * @param res  Command result structure
 */
void image_handler(struct command_result* res);
extern const struct bp_command_def image_def;