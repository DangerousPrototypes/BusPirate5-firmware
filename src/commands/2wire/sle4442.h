/**
 * @file sle4442.h
 * @brief SLE4442 smart card command interface.
 * @details Provides command for SLE4442 memory card access.
 */

/**
 * @brief SLE4442 smart card handler.
 * @param res  Command result structure
 */
void sle4442(struct command_result* res);

extern const struct bp_command_def sle4442_def;
