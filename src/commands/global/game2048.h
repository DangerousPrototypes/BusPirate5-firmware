// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file game2048.h
 * @brief 2048 — slide numbered tiles on a 4x4 grid.
 */

#ifndef GAME2048_H
#define GAME2048_H

extern const struct bp_command_def game2048_def;
void game2048_handler(struct command_result* res);

#endif
