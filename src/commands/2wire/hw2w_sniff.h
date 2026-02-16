/**
 * @file hw2w_sniff.h
 * @brief 2-wire bus sniffer command interface.
 * @details Provides command to monitor 2-wire bus traffic.
 */

/**
 * @brief Sniff 2-wire bus traffic.
 * @param res  Command result structure
 */
void hw2w_sniff(struct command_result* res);

extern const struct bp_command_def hw2w_sniff_def;