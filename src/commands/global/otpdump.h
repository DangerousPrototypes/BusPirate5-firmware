/**
 * @file otpdump.h
 * @brief OTP (One-Time Programmable) memory dump command interface.
 * @details Provides command to display RP2040/RP2350 OTP memory contents.
 */

#pragma once

/**
 * @brief Handler for OTP dump command.
 * @param res  Command result structure
 */
void otpdump_handler(struct command_result* res);