/**
 * @file comment.c
 * @brief Comment line handler — skips lines starting with '#'.
 * @details Lines beginning with '#' are treated as comments and ignored.
 *          This is useful when pasting script content from a text editor that
 *          includes comment lines.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "lib/bp_args/bp_cmd.h"

const bp_command_def_t comment_def = {
    .name = "#",
    .description = 0x00,
    .actions = NULL,
    .action_count = 0,
    .opts = NULL,
    .usage = NULL,
    .usage_count = 0,
};

void comment_handler(struct command_result* res) {
    // Lines starting with '#' are comments — do nothing.
    (void)res;
}
