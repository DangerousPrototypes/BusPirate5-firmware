/**
 * @file bug.h
 * @brief Hardware bug detection and workaround command interface.
 * @details Provides commands to detect and work around hardware errata.
 */

/**
 * @brief Handler for bug detection command.
 * @param res  Command result structure
 */
void bug_handler(struct command_result* res);

/**
 * @brief Check if E9 erratum seems fixed.
 * @param pullup   Pullup configuration
 * @param bio_pin  I/O pin number
 * @param verbose  Enable verbose output
 * @return true if erratum appears fixed
 */
bool bug_e9_seems_fixed(bool pullup, uint8_t bio_pin, bool verbose);