/**
 * @file nmea.h
 * @brief NMEA GPS sentence decoder command interface.
 * @details Provides command to decode NMEA-0183 GPS sentences.
 */

/**
 * @brief Decode NMEA GPS sentences.
 * @param res  Command result structure
 */
void nmea_decode_handler(struct command_result* res);

extern const struct bp_command_def nmea_decode_def;