/**
 * @file sniff.h
 * @brief SPI bus sniffer command interface.
 * @details Provides command to monitor SPI bus traffic.
 */

/**
 * @brief Sniff SPI bus traffic.
 * @param res  Command result structure
 */
void sniff_handler(struct command_result* res);