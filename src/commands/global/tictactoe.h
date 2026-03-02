// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file tictactoe.h
 * @brief Tic-Tac-Toe — 3x3 grid vs CPU with minimax AI.
 */

#ifndef TICTACTOE_H
#define TICTACTOE_H

extern const struct bp_command_def tictactoe_def;
void tictactoe_handler(struct command_result* res);

#endif
