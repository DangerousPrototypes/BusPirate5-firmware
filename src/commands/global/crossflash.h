// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

#ifndef CROSSFLASH_H
#define CROSSFLASH_H

extern const struct bp_command_def crossflash_def;
void crossflash_handler(struct command_result* res);

#endif
