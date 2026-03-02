// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT
#ifndef WIRETRACE_H
#define WIRETRACE_H

extern const struct bp_command_def wiretrace_def;
void wiretrace_handler(struct command_result* res);

#endif
