/*
 * arduboy.h — Arduboy emulator mode for Bus Pirate
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef MODE_ARDUBOY_H
#define MODE_ARDUBOY_H

#include <stdint.h>

/* ── Mode dispatch interface (matches _mode struct in modes.h) ── */

uint32_t arduboy_setup(void);
uint32_t arduboy_setup_exec(void);
void arduboy_cleanup(void);
void arduboy_help(void);

extern const uint32_t arduboy_commands_count;
extern const struct _mode_command_struct arduboy_commands[];

#endif /* MODE_ARDUBOY_H */
