/**
 * @file flat.h
 * @brief FlatBuffers protocol command interface.
 * @details Provides command to use FlatBuffers binary protocol.
 */

extern const struct bp_command_def flat_def;

/**
 * @brief Handler for FlatBuffers protocol command.
 * @param res  Command result structure
 */
void flat_handler(struct command_result* res) ;