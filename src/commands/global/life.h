// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file life.h
 * @brief Conway's Game of Life — fullscreen cellular automaton.
 *
 * Type "life" at the Bus Pirate prompt to launch.
 * Press 'q' to quit, SPACE to pause/resume, 'r' to randomize,
 * 'c' to clear, '+'/'-' to adjust speed.
 */

#ifndef LIFE_H
#define LIFE_H

extern const struct bp_command_def life_def;
void life_handler(struct command_result* res);

#endif
