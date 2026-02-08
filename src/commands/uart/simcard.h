/**
 * @file simcard.h
 * @brief SIM card ISO7816 command interface.
 * @details Provides command to interact with SIM cards over ISO7816 protocol.
 */

/**
 * @brief SIM card ISO7816 interface.
 * @param res  Command result structure
 */
void simcard_handler(struct command_result* res);