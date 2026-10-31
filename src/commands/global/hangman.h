// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file hangman.h
 * @brief Hangman — guess the word letter-by-letter, EE/protocol themed.
 */

#ifndef HANGMAN_H
#define HANGMAN_H

extern const struct bp_command_def hangman_def;
void hangman_handler(struct command_result* res);

#endif
