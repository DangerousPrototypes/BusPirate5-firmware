/**
 * @file scan.h
 * @brief 1-Wire ROM search command interface.
 * @details Provides command to scan 1-Wire bus for device ROMs.
 */

/**
 * @brief Test 1-Wire ROM search algorithm.
 * @param res  Command result structure
 */
void onewire_test_romsearch(struct command_result* res);

extern const struct bp_command_def scan_1wire_def;