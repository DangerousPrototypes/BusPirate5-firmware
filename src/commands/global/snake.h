// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file snake.h
 * @brief Snake — classic terminal snake game.
 */

#ifndef SNAKE_H
#define SNAKE_H

extern const struct bp_command_def snake_def;
void snake_handler(struct command_result* res);

#endif
