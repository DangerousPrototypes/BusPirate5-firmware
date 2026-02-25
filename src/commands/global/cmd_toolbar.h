/**
 * @file cmd_toolbar.h
 * @brief Toolbar debug/test command interface.
 * @details Development command for testing the toolbar registry system.
 *          Creates dummy toolbars, lists registered toolbars, etc.
 */

#pragma once

struct command_result;
struct bp_command_def;

extern const struct bp_command_def toolbar_cmd_def;
void toolbar_cmd_handler(struct command_result* res);
