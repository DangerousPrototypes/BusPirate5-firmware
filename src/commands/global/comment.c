// Bus Pirate comment command
// Lines starting with # are treated as comments and silently ignored.
// This allows pasting script snippets that include comment lines from a text
// editor without the Bus Pirate reporting an error.

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "lib/bp_args/bp_cmd.h"

static const char* const usage[] = {
    "# [comment text]",
    "Comment line, ignored:%s # this is a comment",
};

const bp_command_def_t comment_def = {
    .name = "#",
    .description = T_HELP_CMD_COMMENT,
    .actions = NULL,
    .action_count = 0,
    .opts = NULL,
    .usage = usage,
    .usage_count = count_of(usage),
};

void comment_handler(struct command_result* res) {
    (void)res;
    // do nothing - the entire line is a comment
}
