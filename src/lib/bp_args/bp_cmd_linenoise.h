/**
 * @file bp_cmd_linenoise.h
 * @brief Linenoise glue — wires bp_cmd hints/completion to linenoise callbacks.
 * @details Call bp_cmd_linenoise_init() once after linenoise is initialised.
 *          The callbacks walk both the global commands[] array and the current
 *          mode's mode_commands[] array, collecting every entry whose .def
 *          pointer is non-NULL, and delegate to bp_cmd_hint() / bp_cmd_completion().
 *
 *          Commands that have NOT yet been migrated to bp_cmd (def == NULL)
 *          are silently skipped — no regressions.
 */

#ifndef BP_CMD_LINENOISE_H
#define BP_CMD_LINENOISE_H

/**
 * @brief Register bp_cmd-driven hints and completion with linenoise.
 * @details Calls linenoiseSetHintsCallback() and linenoiseSetCompletionCallback().
 *          Safe to call multiple times (idempotent).
 */
void bp_cmd_linenoise_init(void);

#endif // BP_CMD_LINENOISE_H
