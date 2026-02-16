/**
 * @file bp_cmd_linenoise.c
 * @brief Linenoise glue — bridges bp_cmd hint/completion to linenoise callbacks.
 * @details Implements the two linenoise callback signatures:
 *            char *hints_cb(const char *buf, int *color, int *bold)
 *            void  comp_cb(const char *buf, linenoiseCompletions *lc)
 *
 *          Each callback dynamically builds a flat array of const bp_command_def_t*
 *          from the global commands[] and the current mode's mode_commands[],
 *          then delegates to bp_cmd_hint() / bp_cmd_completion().
 *
 *          Only entries with a non-NULL .def pointer participate — legacy
 *          commands are transparently skipped.
 */

#include "bp_cmd_linenoise.h"
#include "bp_cmd.h"
#include "lib/bp_linenoise/linenoise.h"
#include "pirate.h"
#include "command_struct.h"
#include "commands.h"
#include "bytecode.h"   /* struct _bytecode, needed by modes.h */
#include "modes.h"
#include "system_config.h"

#include <stddef.h>
#include <string.h>

/*
 * Upper bound on the number of defs we collect per callback invocation.
 * Global commands + largest mode command set.  If a mode has more than
 * this many migrated commands some will simply be silently skipped —
 * a safe, low-cost tradeoff vs. malloc on an RP2040.
 */
#define MAX_COLLECTED_DEFS 48

/* ── helpers ────────────────────────────────────────────────────────── */

/**
 * @brief Collect non-NULL .def pointers from global + current-mode commands.
 * @param[out] defs   Caller-provided array of at least MAX_COLLECTED_DEFS
 * @return            Number of defs collected
 */
static size_t collect_defs(const bp_command_def_t *defs[]) {
    size_t n = 0;

    /* Global commands */
    for (uint32_t i = 0; i < commands_count && n < MAX_COLLECTED_DEFS; i++) {
        if (commands[i].def) {
            defs[n++] = commands[i].def;
        }
    }

    /* Current mode commands */
    if (modes[system_config.mode].mode_commands_count) {
        uint32_t mc = *modes[system_config.mode].mode_commands_count;
        const struct _mode_command_struct *mc_arr = modes[system_config.mode].mode_commands;
        for (uint32_t i = 0; i < mc && n < MAX_COLLECTED_DEFS; i++) {
            if (mc_arr[i].def) {
                defs[n++] = mc_arr[i].def;
            }
        }
    }

    return n;
}

/* ── linenoise hints callback ──────────────────────────────────────── */

/**
 * @brief Hints callback matching linenoiseHintsCallback signature.
 * @details Called by linenoise on every keystroke to generate ghost text
 *          displayed to the right of the cursor.
 * @param buf    Current line buffer (null-terminated)
 * @param color  ANSI color code to set (output, -1 = default)
 * @param bold   Bold attribute (output, 0 = normal, 1 = bold)
 * @return Static hint string, or NULL if no hint
 */
static char *bp_cmd_ln_hints(const char *buf, int *color, int *bold) {
    if (!buf || buf[0] == '\0') return NULL;

    const bp_command_def_t *defs[MAX_COLLECTED_DEFS];
    size_t count = collect_defs(defs);
    if (count == 0) return NULL;

    size_t len = strlen(buf);
    const char *hint = bp_cmd_hint(buf, len, defs, count);
    if (!hint) return NULL;

    /* Dim grey ghost text */
    *color = 90;
    *bold  = 0;

    /* bp_cmd_hint returns a static buffer — safe to return directly.
     * Cast away const: linenoise prototype is char*, but the static
     * buffer is ours and linenoise does not modify it. */
    return (char *)hint;
}

/* ── linenoise completion callback ─────────────────────────────────── */

/**
 * @brief Adapter: bridges bp_cmd_add_completion_fn to linenoiseAddCompletion.
 */
static void add_completion_adapter(const char *text, void *userdata) {
    linenoiseCompletions *lc = (linenoiseCompletions *)userdata;
    linenoiseAddCompletion(lc, text);
}

/**
 * @brief Completion callback matching linenoiseCompletionCallback signature.
 * @details Called by linenoise when the user presses Tab.
 * @param buf  Current line buffer (null-terminated)
 * @param lc   Completions table to populate via linenoiseAddCompletion()
 */
static void bp_cmd_ln_completion(const char *buf, linenoiseCompletions *lc) {
    if (!buf || buf[0] == '\0') return;

    const bp_command_def_t *defs[MAX_COLLECTED_DEFS];
    size_t count = collect_defs(defs);
    if (count == 0) return;

    size_t len = strlen(buf);
    bp_cmd_completion(buf, len, defs, count, add_completion_adapter, lc);
}

/* ── public init ───────────────────────────────────────────────────── */

void bp_cmd_linenoise_init(void) {
    linenoiseSetHintsCallback(bp_cmd_ln_hints);
    linenoiseSetCompletionCallback(bp_cmd_ln_completion);
    /* No free-hints callback needed — bp_cmd_hint returns a static buffer. */
}
