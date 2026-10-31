// Hash/comment command for Bus Pirate 5/6/7
//
// The '#' command silently ignores the rest of the line.
// This lets users paste in annotated command sequences from a text editor
// or webpage without the Bus Pirate complaining about unknown commands.
//
// Usage:  # anything here is ignored
//
// Registration (commands.c):
//   { .command="#", .allow_hiz=true, .func=&hash_handler,
//     .def=&hash_def, .description_text=0x00, .category=CMD_CAT_HIDDEN }

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "lib/bp_args/bp_cmd.h"    // bp_command_def_t
#include "system_config.h"
#include "hash.h"

static const char* const usage[] = {
    "# [comment text]\r\n\tIgnore the rest of the line (comment).",
};

const bp_command_def_t hash_def = {
    .name = "#",
    .description = 0x00,
    .actions = NULL,
    .action_count = 0,
    .opts = NULL,
    .usage = usage,
    .usage_count = count_of(usage),
};

void hash_handler(struct command_result* res) {
    // Do nothing — the entire line is treated as a comment.
    (void)res;
}
