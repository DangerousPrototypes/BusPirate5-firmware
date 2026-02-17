/**
 * @file bp_cmd.c
 * @brief Unified command definition, help, and argument query implementation.
 * @details Stateless query-pattern argument parsing. Each call re-scans
 *          the command line buffer using the command definition to correctly
 *          skip flags that consume values.
 */

#include "bp_cmd.h"
#include "lib/bp_number/bp_number.h"
#include "lib/bp_linenoise/ln_cmdreader.h"
#include "ui/ui_term.h"
#include "pirate.h"
#include <stdio.h>
#include <string.h>

/*
 * =============================================================================
 * Internal helpers
 * =============================================================================
 */

static const char *skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
    return p;
}

static const char *skip_tok(const char *p, const char *end) {
    while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
    return p;
}

static size_t tok_len(const char *p, const char *end) {
    return skip_tok(p, end) - p;
}

static bool tok_eq(const char *token, size_t len, const char *str) {
    size_t slen = strlen(str);
    if (len != slen) return false;
    return memcmp(token, str, len) == 0;
}

/**
 * @brief Find option descriptor by short name in command def.
 * @return Option pointer, or NULL if not found / no opts defined.
 */
static const bp_command_opt_t *find_opt_in_def(const bp_command_def_t *def, char c) {
    if (!def->opts) return NULL;
    for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++) {
        if (def->opts[i].short_name == c) {
            return &def->opts[i];
        }
    }
    return NULL;
}

/**
 * @brief Scan command line for a specific flag.
 * @details Uses def->opts to know which other flags consume values,
 *          ensuring correct skip-over behavior.
 * @param def       Command definition
 * @param flag      Short flag character to find (case-insensitive)
 * @param[out] val       Pointer to flag's value (NULL if flag-only)
 * @param[out] val_len   Length of value token
 * @return true if flag found
 */
static bool cmd_scan_flag(const bp_command_def_t *def, char flag,
                          const char **val, size_t *val_len) {
    const char *p = ln_cmdln_current();
    const char *end = p + ln_cmdln_remaining();

    // Skip command name (first token)
    p = skip_ws(p, end);
    p = skip_tok(p, end);

    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end || *p == '\0') break;

        // Check for flag: -X
        if (*p == '-' && p + 1 < end && p[1] != '-' &&
            p[1] != ' ' && p[1] != '\0') {
            char opt_char = p[1];
            p += 2; // past "-X"

            // Look up this flag to know if it eats a value
            const bp_command_opt_t *opt = find_opt_in_def(def, opt_char | 0x20);
            if (!opt) {
                // Also try exact case
                opt = find_opt_in_def(def, opt_char);
            }

            // Case-insensitive match for the requested flag
            bool match = ((opt_char | 0x20) == (flag | 0x20));

            if (opt && opt->arg_type != BP_ARG_NONE) {
                // This flag consumes a value
                p = skip_ws(p, end);
                if (p < end && *p != '-' && *p != '\0') {
                    const char *v = p;
                    size_t vl = tok_len(p, end);
                    p = v + vl;

                    if (match) {
                        *val = v;
                        *val_len = vl;
                        return true;
                    }
                } else {
                    // Flag expects value but none present
                    if (match) {
                        if (opt->arg_type == BP_ARG_REQUIRED) {
                            printf("option -%c requires an argument\r\n", flag);
                        }
                        *val = NULL;
                        *val_len = 0;
                        return true; // flag found, but missing value
                    }
                }
            } else {
                // Flag-only (BP_ARG_NONE or unknown)
                if (match) {
                    *val = NULL;
                    *val_len = 0;
                    return true;
                }
            }
            continue;
        }

        // Skip long option --name[=value] or --name value
        if (*p == '-' && p + 1 < end && p[1] == '-') {
            p += 2;
            p = skip_tok(p, end); // skip --name or --name=value
            // TODO: could match long names here too
            continue;
        }

        // Positional or action verb — skip it
        p = skip_tok(p, end);
    }

    *val = NULL;
    *val_len = 0;
    return false;
}

/*
 * =============================================================================
 * Action verb query
 * =============================================================================
 */

bool bp_cmd_get_action(const bp_command_def_t *def, uint32_t *action) {
    if (!def->actions || def->action_count == 0) return false;

    const char *p = ln_cmdln_current();
    const char *end = p + ln_cmdln_remaining();

    // Skip command name
    p = skip_ws(p, end);
    p = skip_tok(p, end);

    // Walk tokens looking for a verb (skip flags and their values)
    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end || *p == '\0') break;

        // Skip flags
        if (*p == '-') {
            if (p + 1 < end && p[1] != '-' && p[1] != ' ' && p[1] != '\0') {
                char opt_char = p[1];
                p += 2;
                // Check if this flag eats a value
                const bp_command_opt_t *opt = find_opt_in_def(def, opt_char | 0x20);
                if (!opt) opt = find_opt_in_def(def, opt_char);
                if (opt && opt->arg_type != BP_ARG_NONE) {
                    p = skip_ws(p, end);
                    p = skip_tok(p, end); // skip value
                }
                continue;
            }
            // Long option
            p += 2;
            p = skip_tok(p, end);
            continue;
        }

        // This is a positional token — try to match as action verb
        size_t tl = tok_len(p, end);
        for (uint32_t i = 0; i < def->action_count; i++) {
            if (tok_eq(p, tl, def->actions[i].verb)) {
                *action = def->actions[i].action;
                return true;
            }
        }

        // Not a recognized verb — skip (could be positional arg)
        p += tl;
    }

    return false;
}

/*
 * =============================================================================
 * Flag query API
 * =============================================================================
 */

bool bp_cmd_find_flag(const bp_command_def_t *def, char flag) {
    const char *val;
    size_t val_len;
    return cmd_scan_flag(def, flag, &val, &val_len);
}

bool bp_cmd_get_uint32(const bp_command_def_t *def, char flag, uint32_t *value) {
    const char *val;
    size_t val_len;
    if (!cmd_scan_flag(def, flag, &val, &val_len)) return false;
    if (!val || val_len == 0) return false;

    const char *p = val;
    bp_num_format_t fmt;
    if (bp_num_u32(&p, value, &fmt)) {
        return p <= val + val_len;
    }
    return false;
}

bool bp_cmd_get_string(const bp_command_def_t *def, char flag, char *buf, size_t maxlen) {
    const char *val;
    size_t val_len;
    if (maxlen > 0) buf[0] = '\0';
    if (!cmd_scan_flag(def, flag, &val, &val_len)) return false;
    if (!val || val_len == 0) return false;

    size_t copy = (val_len < maxlen - 1) ? val_len : maxlen - 1;
    memcpy(buf, val, copy);
    buf[copy] = '\0';
    return true;
}

bool bp_cmd_get_float(const bp_command_def_t *def, char flag, float *value) {
    const char *val;
    size_t val_len;
    if (!cmd_scan_flag(def, flag, &val, &val_len)) return false;
    if (!val || val_len == 0) return false;

    const char *p = val;
    if (bp_num_float(&p, value)) {
        return p <= val + val_len;
    }
    return false;
}

bool bp_cmd_get_int32(const bp_command_def_t *def, char flag, int32_t *value) {
    const char *val;
    size_t val_len;
    if (!cmd_scan_flag(def, flag, &val, &val_len)) return false;
    if (!val || val_len == 0) return false;

    const char *p = val;
    bp_num_format_t fmt;
    if (bp_num_i32(&p, value, &fmt)) {
        return p <= val + val_len;
    }
    return false;
}

/*
 * =============================================================================
 * Positional argument query
 * =============================================================================
 */

/**
 * @brief Internal: find the Nth positional (non-flag) token.
 * @details Position 0 = command name. Position 1 = first non-flag token
 *          after the command. Flags and their consumed values are skipped
 *          using the def's option descriptors.
 * @param def       Command definition
 * @param pos       Desired position index
 * @param[out] tok  Pointer to start of token (NOT null-terminated)
 * @param[out] tlen Length of token
 * @return true if token found at requested position
 */
static bool cmd_get_positional_token(const bp_command_def_t *def, uint32_t pos,
                                     const char **tok, size_t *tlen) {
    const char *p = ln_cmdln_current();
    const char *end = p + ln_cmdln_remaining();
    uint32_t cur_pos = 0;

    // Position 0 is the command name itself
    p = skip_ws(p, end);
    if (p >= end || *p == '\0') return false;

    if (pos == 0) {
        *tok = p;
        *tlen = tok_len(p, end);
        return *tlen > 0;
    }

    // Skip command name
    p = skip_tok(p, end);

    // Walk remaining tokens
    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end || *p == '\0') break;

        // Skip short flags: -X [value]
        if (*p == '-' && p + 1 < end && p[1] != '-' &&
            p[1] != ' ' && p[1] != '\0') {
            char opt_char = p[1];
            p += 2;
            // Check if this flag consumes a value
            const bp_command_opt_t *opt = find_opt_in_def(def, opt_char | 0x20);
            if (!opt) opt = find_opt_in_def(def, opt_char);
            if (opt && opt->arg_type != BP_ARG_NONE) {
                p = skip_ws(p, end);
                if (p < end && *p != '-' && *p != '\0') {
                    p = skip_tok(p, end); // skip flag's value
                }
            }
            continue;
        }

        // Skip long flags: --name[=value]
        if (*p == '-' && p + 1 < end && p[1] == '-') {
            p += 2;
            p = skip_tok(p, end);
            continue;
        }

        // This is a positional token
        cur_pos++;
        if (cur_pos == pos) {
            *tok = p;
            *tlen = tok_len(p, end);
            return *tlen > 0;
        }
        p = skip_tok(p, end);
    }

    return false;
}

bool bp_cmd_get_positional_string(const bp_command_def_t *def, uint32_t pos,
                                  char *buf, size_t maxlen) {
    if (maxlen > 0) buf[0] = '\0';

    const char *tok;
    size_t tlen;
    if (!cmd_get_positional_token(def, pos, &tok, &tlen)) return false;

    // Reject tokens that look like flags
    if (tlen > 0 && tok[0] == '-') return false;

    size_t copy = (tlen < maxlen - 1) ? tlen : maxlen - 1;
    memcpy(buf, tok, copy);
    buf[copy] = '\0';
    return true;
}

bool bp_cmd_get_positional_uint32(const bp_command_def_t *def, uint32_t pos,
                                  uint32_t *value) {
    const char *tok;
    size_t tlen;
    if (!cmd_get_positional_token(def, pos, &tok, &tlen)) return false;

    const char *p = tok;
    bp_num_format_t fmt;
    if (bp_num_u32(&p, value, &fmt)) {
        return p <= tok + tlen;
    }
    return false;
}

bool bp_cmd_get_positional_int32(const bp_command_def_t *def, uint32_t pos,
                                 int32_t *value) {
    const char *tok;
    size_t tlen;
    if (!cmd_get_positional_token(def, pos, &tok, &tlen)) return false;

    const char *p = tok;
    bp_num_format_t fmt;
    if (bp_num_i32(&p, value, &fmt)) {
        return p <= tok + tlen;
    }
    return false;
}

bool bp_cmd_get_positional_float(const bp_command_def_t *def, uint32_t pos,
                                 float *value) {
    const char *tok;
    size_t tlen;
    if (!cmd_get_positional_token(def, pos, &tok, &tlen)) return false;

    const char *p = tok;
    if (bp_num_float(&p, value)) {
        return p <= tok + tlen;
    }
    return false;
}

bool bp_cmd_get_remainder(const bp_command_def_t *def,
                          const char **out, size_t *len) {
    (void)def; // reserved for future use
    const char *p = ln_cmdln_current();
    const char *end = p + ln_cmdln_remaining();

    // Skip leading whitespace
    p = skip_ws(p, end);
    if (p >= end || *p == '\0') return false;

    // Skip command name (position 0)
    p = skip_tok(p, end);

    // Skip whitespace after command name
    p = skip_ws(p, end);
    if (p >= end || *p == '\0') return false;

    *out = p;
    *len = end - p;
    return true;
}

/*
 * =============================================================================
 * Help display
 * =============================================================================
 */

/**
 * @brief Display help for a command using its unified definition.
 * @details Prints usage examples, then actions with descriptions,
 *          then flags with short/long names and arg hints.
 */
void bp_cmd_help_show(const bp_command_def_t *def) {
    // Column alignment: left column target width (characters).
    // If the label fits within this, pad to the column with spaces.
    // If it overflows, just use a single space before the description.
    #define HELP_COL_WIDTH 16

    // Usage examples
    if (def->usage && def->usage_count > 0) {
        printf("usage:\r\n");
        for (uint32_t i = 0; i < def->usage_count; i++) {
            printf("%s", ui_term_color_info());
            printf(def->usage[i], ui_term_color_reset());
            printf("\r\n");
        }
    }

    // Section heading from description
    if (def->description) {
        printf("\r\n%s%s%s\r\n",
               ui_term_color_info(),
               GET_T(def->description),
               ui_term_color_reset());
    }

    // Action verbs
    if (def->actions && def->action_count > 0) {
        for (uint32_t i = 0; i < def->action_count; i++) {
            int len = strlen(def->actions[i].verb);
            printf("%s%s%s",
                   ui_term_color_prompt(),
                   def->actions[i].verb,
                   ui_term_color_reset());
            // Pad or space
            if (len < HELP_COL_WIDTH) {
                printf("%*s", HELP_COL_WIDTH - len, "");
            } else {
                printf(" ");
            }
            printf("%s%s%s\r\n",
                   ui_term_color_info(),
                   def->actions[i].description ? GET_T(def->actions[i].description) : "",
                   ui_term_color_reset());
        }
    }

    // Flags
    if (def->opts) {
        for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++) {
            const bp_command_opt_t *o = &def->opts[i];
            char flag_str[48];
            int pos = 0;

            // Build flag display: "-f, --file <file>"
            if (o->short_name) {
                pos += snprintf(flag_str + pos, sizeof(flag_str) - pos, "-%c", o->short_name);
            }
            if (o->long_name) {
                if (pos > 0) pos += snprintf(flag_str + pos, sizeof(flag_str) - pos, ", ");
                pos += snprintf(flag_str + pos, sizeof(flag_str) - pos, "--%s", o->long_name);
            }
            if (o->arg_hint) {
                pos += snprintf(flag_str + pos, sizeof(flag_str) - pos, " %s", o->arg_hint);
            }

            printf("%s%s%s",
                   ui_term_color_prompt(),
                   flag_str,
                   ui_term_color_reset());
            // Pad or space
            if (pos < HELP_COL_WIDTH) {
                printf("%*s", HELP_COL_WIDTH - pos, "");
            } else {
                printf(" ");
            }
            printf("%s%s%s\r\n",
                   ui_term_color_info(),
                   o->description ? GET_T(o->description) : "",
                   ui_term_color_reset());
        }
    }
}

bool bp_cmd_help_check(const bp_command_def_t *def, bool help_flag) {
    if (help_flag) {
        bp_cmd_help_show(def);
        return true;
    }
    return false;
}

/*
 * =============================================================================
 * Linenoise hints and completion (Phase 3 stubs)
 * =============================================================================
 */

// Shared scratch buffer for hint text (static — returned to linenoise)
static char hint_buf[64];

const char *bp_cmd_hint(const char *buf, size_t len,
                        const bp_command_def_t *const *defs, size_t count) {
    if (!buf || len == 0 || !defs) return NULL;

    // Find which command matches
    const char *p = buf;
    const char *end = buf + len;

    // Skip leading whitespace
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    const char *cmd_start = p;
    while (p < end && *p != ' ' && *p != '\t') p++;
    size_t cmd_len = p - cmd_start;
    if (cmd_len == 0) return NULL;

    // Match command name
    const bp_command_def_t *def = NULL;
    for (size_t i = 0; i < count; i++) {
        if (defs[i] && defs[i]->name && tok_eq(cmd_start, cmd_len, defs[i]->name)) {
            def = defs[i];
            break;
        }
    }
    if (!def) {
        // Partial command match — suggest first matching command name
        for (size_t i = 0; i < count; i++) {
            if (defs[i] && defs[i]->name &&
                cmd_len < strlen(defs[i]->name) &&
                memcmp(cmd_start, defs[i]->name, cmd_len) == 0) {
                snprintf(hint_buf, sizeof(hint_buf), "%s", defs[i]->name + cmd_len);
                return hint_buf;
            }
        }
        return NULL;
    }

    // Command matched — check what the user is currently typing
    while (p < end && (*p == ' ' || *p == '\t')) p++;

    if (p >= end) {
        // Nothing after command — suggest first action verb if available
        if (def->actions && def->action_count > 0) {
            snprintf(hint_buf, sizeof(hint_buf), " %s", def->actions[0].verb);
            return hint_buf;
        }
        return NULL;
    }

    // Check if user is typing after a flag that needs a value
    // Find last token
    const char *last_tok = p;
    const char *prev_tok = NULL;
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p >= end) break;
        prev_tok = last_tok;
        last_tok = p;
        while (p < end && *p != ' ' && *p != '\t') p++;
    }

    // If last token is "-", suggest first available flag
    if (last_tok && *last_tok == '-' && tok_len(last_tok, end) == 1 && def->opts) {
        for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++) {
            if (def->opts[i].short_name) {
                if (def->opts[i].arg_hint) {
                    snprintf(hint_buf, sizeof(hint_buf), "%c %s",
                             def->opts[i].short_name, def->opts[i].arg_hint);
                } else {
                    snprintf(hint_buf, sizeof(hint_buf), "%c", def->opts[i].short_name);
                }
                return hint_buf;
            }
        }
    }

    // If last token is a complete flag like "-f", suggest its arg_hint
    if (last_tok && *last_tok == '-' && tok_len(last_tok, end) == 2) {
        char fc = last_tok[1];
        const bp_command_opt_t *opt = find_opt_in_def(def, fc | 0x20);
        if (!opt) opt = find_opt_in_def(def, fc);
        if (opt && opt->arg_hint) {
            snprintf(hint_buf, sizeof(hint_buf), " %s", opt->arg_hint);
            return hint_buf;
        }
    }

    // Partial action verb match
    if (last_tok && *last_tok != '-' && def->actions) {
        size_t tl = tok_len(last_tok, end);
        for (uint32_t i = 0; i < def->action_count; i++) {
            size_t vl = strlen(def->actions[i].verb);
            if (tl < vl && memcmp(last_tok, def->actions[i].verb, tl) == 0) {
                snprintf(hint_buf, sizeof(hint_buf), "%s", def->actions[i].verb + tl);
                return hint_buf;
            }
        }
    }

    return NULL;
}

void bp_cmd_completion(const char *buf, size_t len,
                       const bp_command_def_t *const *defs, size_t count,
                       bp_cmd_add_completion_fn add_completion, void *userdata) {
    if (!buf || len == 0 || !defs || !add_completion) return;

    const char *p = buf;
    const char *end = buf + len;

    // Skip whitespace
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    const char *cmd_start = p;
    while (p < end && *p != ' ' && *p != '\t') p++;
    size_t cmd_len = p - cmd_start;
    if (cmd_len == 0) return;

    // Check if we're still typing the command name
    if (p >= end) {
        // Complete command names
        for (size_t i = 0; i < count; i++) {
            if (defs[i] && defs[i]->name &&
                cmd_len <= strlen(defs[i]->name) &&
                memcmp(cmd_start, defs[i]->name, cmd_len) == 0) {
                add_completion(defs[i]->name, userdata);
            }
        }
        return;
    }

    // Find matching command
    const bp_command_def_t *def = NULL;
    for (size_t i = 0; i < count; i++) {
        if (defs[i] && defs[i]->name && tok_eq(cmd_start, cmd_len, defs[i]->name)) {
            def = defs[i];
            break;
        }
    }
    if (!def) return;

    // Find last token for completion context
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    const char *last_tok = p;
    const char *scan = p;
    while (scan < end) {
        while (scan < end && (*scan == ' ' || *scan == '\t')) scan++;
        if (scan >= end) break;
        last_tok = scan;
        while (scan < end && *scan != ' ' && *scan != '\t') scan++;
    }

    size_t tl = tok_len(last_tok, end);
    static char comp_buf[64];

    // Complete action verbs
    if (last_tok && *last_tok != '-' && def->actions) {
        for (uint32_t i = 0; i < def->action_count; i++) {
            if (tl <= strlen(def->actions[i].verb) &&
                memcmp(last_tok, def->actions[i].verb, tl) == 0) {
                // Build full line up to last_tok, then append completed verb
                size_t prefix_len = last_tok - buf;
                if (prefix_len + strlen(def->actions[i].verb) < sizeof(comp_buf)) {
                    memcpy(comp_buf, buf, prefix_len);
                    strcpy(comp_buf + prefix_len, def->actions[i].verb);
                    add_completion(comp_buf, userdata);
                }
            }
        }
    }

    // Complete flag names
    if (last_tok && *last_tok == '-' && tl <= 2 && def->opts) {
        for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++) {
            if (def->opts[i].short_name) {
                char flag_str[4] = { '-', def->opts[i].short_name, '\0' };
                if (tl == 1 || (tl == 2 && last_tok[1] == def->opts[i].short_name)) {
                    size_t prefix_len = last_tok - buf;
                    if (prefix_len + 2 < sizeof(comp_buf)) {
                        memcpy(comp_buf, buf, prefix_len);
                        strcpy(comp_buf + prefix_len, flag_str);
                        add_completion(comp_buf, userdata);
                    }
                }
            }
        }
    }
}
