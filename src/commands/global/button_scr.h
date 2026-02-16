/**
 * @file button_scr.h
 * @brief Button script binding command interface.
 * @details Provides commands to bind scripts/macros to button presses.
 */

#pragma once
#include "pirate/button.h"

/**
 * @brief Handler for button script configuration command.
 * @param res  Command result structure
 */
void button_scr_handler(struct command_result* res);

extern const struct bp_command_def button_scr_def;

/**
 * @brief Execute script bound to button code.
 * @param button_code  Button event code
 * @return true if script executed
 */
bool button_exec(enum button_codes button_code);
