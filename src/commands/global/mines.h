// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file mines.h
 * @brief Mine Sweep — uncover a grid avoiding mines.
 */

#ifndef MINES_H
#define MINES_H

extern const struct bp_command_def mines_def;
void mines_handler(struct command_result* res);

#endif
