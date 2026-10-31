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
#include "pirate.h"
#include "ui/ui_term.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
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
 * @brief Case-insensitive token equality check.
 */
static bool tok_eq_ci(const char *token, size_t len, const char *str) {
    size_t slen = strlen(str);
    if (len != slen) return false;
    for (size_t i = 0; i < len; i++) {
        if ((token[i] | 0x20) != (str[i] | 0x20)) return false;
    }
    return true;
}

/**
 * @brief Case-insensitive prefix match: does token start with prefix?
 */
static bool tok_prefix_ci(const char *token, size_t tok_len,
                          const char *full, size_t full_len) {
    if (tok_len >= full_len) return false;
    for (size_t i = 0; i < tok_len; i++) {
        if ((token[i] | 0x20) != (full[i] | 0x20)) return false;
    }
    return true;
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
 * @brief Find option descriptor by long name in command def.
 * @param def       Command definition
 * @param name      Start of long name (without --)
 * @param name_len  Length of the name (may not be null-terminated)
 * @return Option pointer, or NULL if not found / no opts defined.
 */
static const bp_command_opt_t *find_opt_by_long_name(const bp_command_def_t *def,
                                                     const char *name, size_t name_len) {
    if (!def->opts) return NULL;
    for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++) {
        if (def->opts[i].long_name &&
            strlen(def->opts[i].long_name) == name_len &&
            memcmp(def->opts[i].long_name, name, name_len) == 0) {
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

        // Long option --name[=value] or --name value
        if (*p == '-' && p + 1 < end && p[1] == '-') {
            p += 2;
            const char *name_start = p;
            // Find end of name (stop at '=' or whitespace)
            while (p < end && *p != '=' && *p != ' ' && *p != '\t' && *p != '\0') p++;
            size_t name_len = p - name_start;

            const bp_command_opt_t *opt = find_opt_by_long_name(def, name_start, name_len);

            // Check if this long name matches the requested short flag
            bool match = (opt && opt->short_name &&
                         ((opt->short_name | 0x20) == (flag | 0x20)));

            if (opt && opt->arg_type != BP_ARG_NONE) {
                // This flag consumes a value — check for =value or next token
                if (p < end && *p == '=') {
                    p++; // skip '='
                    const char *v = p;
                    size_t vl = tok_len(p, end);
                    p = v + vl;
                    if (match) {
                        *val = v;
                        *val_len = vl;
                        return true;
                    }
                } else {
                    // Value is next token
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
                        if (match) {
                            if (opt->arg_type == BP_ARG_REQUIRED) {
                                printf("option --%.*s requires an argument\r\n",
                                       (int)name_len, name_start);
                            }
                            *val = NULL;
                            *val_len = 0;
                            return true;
                        }
                    }
                }
            } else {
                // Flag-only or skip to end of token
                if (*p == '=') p = skip_tok(p, end); // skip any =junk
                else p = skip_ws(p, end);
                if (match) {
                    *val = NULL;
                    *val_len = 0;
                    return true;
                }
            }
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
    bool has_actions = (def->actions && def->action_count > 0);
    bool has_delegate = (def->action_delegate != NULL);
    if (!has_actions && !has_delegate) return false;

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
            const char *nm = p;
            while (p < end && *p != '=' && *p != ' ' && *p != '\t' && *p != '\0') p++;
            size_t nm_len = p - nm;
            const bp_command_opt_t *opt2 = find_opt_by_long_name(def, nm, nm_len);
            if (p < end && *p == '=') {
                p = skip_tok(p, end);
            } else if (opt2 && opt2->arg_type != BP_ARG_NONE) {
                p = skip_ws(p, end);
                if (p < end && *p != '-' && *p != '\0') {
                    p = skip_tok(p, end);
                }
            }
            continue;
        }

        // This is a positional token — try to match as action verb
        size_t tl = tok_len(p, end);
        if (has_delegate) {
            if (def->action_delegate->match(p, tl, action)) return true;
        } else {
            for (uint32_t i = 0; i < def->action_count; i++) {
                if (tok_eq(p, tl, def->actions[i].verb)) {
                    *action = def->actions[i].action;
                    return true;
                }
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

bool bp_cmd_has_help_flag(void) {
    const char *p = ln_cmdln_current();
    const char *end = p + ln_cmdln_remaining();

    // Skip command name (first token)
    p = skip_ws(p, end);
    p = skip_tok(p, end);

    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end || *p == '\0') break;

        if (*p == '-') {
            size_t tl = tok_len(p, end);
            // Match "-h" (exactly 2 chars)
            if (tl == 2 && (p[1] | 0x20) == 'h') {
                return true;
            }
            // Match "--help" (exactly 6 chars)
            if (tl == 6 && memcmp(p, "--help", 6) == 0) {
                return true;
            }
        }
        p = skip_tok(p, end);
    }
    return false;
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

        // Skip long flags: --name[=value] or --name value
        if (*p == '-' && p + 1 < end && p[1] == '-') {
            p += 2;
            const char *nm = p;
            while (p < end && *p != '=' && *p != ' ' && *p != '\t' && *p != '\0') p++;
            size_t nm_len = p - nm;
            const bp_command_opt_t *opt = find_opt_by_long_name(def, nm, nm_len);
            if (p < end && *p == '=') {
                p = skip_tok(p, end); // skip =value
            } else if (opt && opt->arg_type != BP_ARG_NONE) {
                p = skip_ws(p, end);
                if (p < end && *p != '-' && *p != '\0') {
                    p = skip_tok(p, end); // skip flag's value
                }
            }
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
 * Constraint validation (internal)
 * =============================================================================
 */

/**
 * @brief Validate a value against a constraint.
 * @return true if value is within range or no constraint.
 */
static bool val_check(const bp_val_constraint_t *c, const void *val) {
    if (!c || c->type == BP_VAL_NONE) return true;
    switch (c->type) {
        case BP_VAL_FLOAT: {
            float v = *(const float *)val;
            return (v >= c->f.min && v <= c->f.max);
        }
        case BP_VAL_UINT32: {
            uint32_t v = *(const uint32_t *)val;
            return (v >= c->u.min && v <= c->u.max);
        }
        case BP_VAL_INT32: {
            int32_t v = *(const int32_t *)val;
            return (v >= c->i.min && v <= c->i.max);
        }
        case BP_VAL_CHOICE: {
            uint32_t v = *(const uint32_t *)val;
            for (uint32_t i = 0; i < c->choice.count; i++) {
                if (c->choice.choices[i].value == v) return true;
            }
            return false;
        }
        default: return true;
    }
}

/**
 * @brief Print a consistent range-error message from constraint data.
 * @param c      Constraint with min/max
 * @param label  Human name for the value (e.g. "Current"), NULL = "Value"
 * @param units  Unit suffix (e.g. "mA"), NULL = none
 * @param out    Pointer to the rejected value (type matches c->type), NULL = don't show
 */
static void val_print_range_error(const bp_val_constraint_t *c,
                                  const char *label, const char *units,
                                  const void *out) {
    if (!c) return;
    if (!label) label = "Value";
    if (!units) units = "";
    switch (c->type) {
        case BP_VAL_FLOAT:
            printf("%s%s out of range", ui_term_color_warning(), label);
            if (out) printf(": %s%1.2f%s", ui_term_color_reset(), *(const float *)out, ui_term_color_warning());
            printf(", expected %1.2f%s to %1.2f%s%s\r\n",
                   c->f.min, units, c->f.max, units, ui_term_color_reset());
            break;
        case BP_VAL_UINT32:
            printf("%s%s out of range", ui_term_color_warning(), label);
            if (out) printf(": %s%lu%s", ui_term_color_reset(), (unsigned long)*(const uint32_t *)out, ui_term_color_warning());
            printf(", expected %lu%s to %lu%s%s\r\n",
                   (unsigned long)c->u.min, units, (unsigned long)c->u.max, units, ui_term_color_reset());
            break;
        case BP_VAL_INT32:
            printf("%s%s out of range", ui_term_color_warning(), label);
            if (out) printf(": %s%ld%s", ui_term_color_reset(), (long)*(const int32_t *)out, ui_term_color_warning());
            printf(", expected %ld%s to %ld%s%s\r\n",
                   (long)c->i.min, units, (long)c->i.max, units, ui_term_color_reset());
            break;
        case BP_VAL_CHOICE:
            printf("%s%s: unknown value%s\r\n", ui_term_color_warning(), label, ui_term_color_reset());
            printf("  expected: ");
            for (uint32_t i = 0; i < c->choice.count; i++) {
                if (i) printf(", ");
                printf("%s", c->choice.choices[i].name);
            }
            printf("\r\n");
            break;
        default:
            break;
    }
}

/**
 * @brief Find the default choice index (0-based) for display purposes.
 */
static uint32_t val_choice_default_index(const bp_val_constraint_t *c) {
    for (uint32_t i = 0; i < c->choice.count; i++) {
        if (c->choice.choices[i].value == c->choice.def) return i;
    }
    return 0;
}

/**
 * @brief Self-contained interactive prompt loop driven by a constraint.
 * @details Prints description + default, loops until valid input or exit.
 *          Uses ui_parse_get_float/uint32 directly (no ui_prompt_float dependency).
 *          For BP_VAL_CHOICE: prints numbered menu, user picks by number.
 */
static bp_cmd_status_t val_prompt_loop(const bp_val_constraint_t *c, void *out) {
    prompt_result result;

    while (true) {
        // For choice type, print the numbered menu first
        if (c->type == BP_VAL_CHOICE) {
            uint32_t def_idx = val_choice_default_index(c);
            printf("\r\n");
            for (uint32_t i = 0; i < c->choice.count; i++) {
                const char *display = c->choice.choices[i].label
                    ? GET_T(c->choice.choices[i].label)
                    : c->choice.choices[i].name;
                printf(" %lu. %s%s%s%s\r\n",
                       (unsigned long)(i + 1),
                       ui_term_color_info(),
                       display,
                       (i == def_idx) ? "*" : "",
                       ui_term_color_reset());
            }
        }

        // Print prompt: "x to exit (default) >"
        // CHOICE already printed \r\n before menu; others need it here
        if (c->type != BP_VAL_CHOICE) {
            printf("\r\n");
            if (c->hint) {
                printf(" %s%s%s\r\n", ui_term_color_info(), GET_T(c->hint), ui_term_color_reset());
            }
        }
        printf("%sx to exit ", ui_term_color_prompt());
        switch (c->type) {
            case BP_VAL_FLOAT:
                printf("(%1.2f)", c->f.def);
                break;
            case BP_VAL_UINT32:
                printf("(%lu)", (unsigned long)c->u.def);
                break;
            case BP_VAL_INT32:
                printf("(%ld)", (long)c->i.def);
                break;
            case BP_VAL_CHOICE:
                printf("(%lu)", (unsigned long)(val_choice_default_index(c) + 1));
                break;
            default: break;
        }
        printf(" >%s \x03", ui_term_color_reset());

        if (!ui_prompt_user_input()) {
            return BP_CMD_EXIT;
        }

        // Parse based on constraint type
        switch (c->type) {
            case BP_VAL_FLOAT:
                ui_parse_get_float(&result, (float *)out);
                break;
            case BP_VAL_UINT32:
                ui_parse_get_uint32(&result, (uint32_t *)out);
                break;
            case BP_VAL_INT32: {
                // int32 via uint32 parse then cast
                uint32_t tmp;
                ui_parse_get_uint32(&result, &tmp);
                if (result.success) *(int32_t *)out = (int32_t)tmp;
                break;
            }
            case BP_VAL_CHOICE: {
                // Interactive: user picks by 1-based number
                uint32_t pick;
                ui_parse_get_uint32(&result, &pick);
                if (result.success && pick >= 1 && pick <= c->choice.count) {
                    *(uint32_t *)out = c->choice.choices[pick - 1].value;
                } else if (result.success) {
                    result.success = false; // out of range
                }
                break;
            }
            default:
                return BP_CMD_MISSING;
        }

        printf("\r\n");

        if (result.exit) {
            return BP_CMD_EXIT;
        }

        // Enter with no value = accept default
        if (result.no_value) {
            switch (c->type) {
                case BP_VAL_FLOAT:  *(float *)out    = c->f.def; break;
                case BP_VAL_UINT32: *(uint32_t *)out = c->u.def; break;
                case BP_VAL_INT32:  *(int32_t *)out  = c->i.def; break;
                case BP_VAL_CHOICE: *(uint32_t *)out = c->choice.def; break;
                default: break;
            }
            return BP_CMD_OK;
        }

        if (result.success && val_check(c, out)) {
            return BP_CMD_OK;
        }

        // Invalid — print range and loop
        val_print_range_error(c, NULL, NULL, out);
    }
}

/*
 * =============================================================================
 * Constraint-aware argument resolution
 * =============================================================================
 */

/**
 * @brief Match a raw token against a choice table (case-insensitive).
 * @details Tries name first, then alias. Writes the choice's .value to *out.
 * @return true if matched
 */
static bool val_match_choice(const bp_val_constraint_t *c,
                             const char *tok, size_t tok_len,
                             uint32_t *out) {
    for (uint32_t i = 0; i < c->choice.count; i++) {
        const bp_val_choice_t *ch = &c->choice.choices[i];
        // Match full name
        if (ch->name && strlen(ch->name) == tok_len) {
            bool match = true;
            for (size_t j = 0; j < tok_len; j++) {
                if ((tok[j] | 0x20) != (ch->name[j] | 0x20)) { match = false; break; }
            }
            if (match) { *out = ch->value; return true; }
        }
        // Match alias
        if (ch->alias && strlen(ch->alias) == tok_len) {
            bool match = true;
            for (size_t j = 0; j < tok_len; j++) {
                if ((tok[j] | 0x20) != (ch->alias[j] | 0x20)) { match = false; break; }
            }
            if (match) { *out = ch->value; return true; }
        }
    }
    return false;
}

bp_cmd_status_t bp_cmd_positional(const bp_command_def_t *def,
                                  uint32_t pos, void *out) {
    // Look up constraint from def's positionals array (1-based index)
    const bp_val_constraint_t *c = NULL;
    if (def->positionals && pos > 0 && pos <= def->positional_count) {
        c = def->positionals[pos - 1].constraint;
    }
    if (!c || c->type == BP_VAL_NONE) {
        return BP_CMD_MISSING; // no constraint = can't know the type
    }

    // Parse from command line using type-appropriate getter
    bool found = false;
    switch (c->type) {
        case BP_VAL_FLOAT:
            found = bp_cmd_get_positional_float(def, pos, (float *)out);
            break;
        case BP_VAL_UINT32:
            found = bp_cmd_get_positional_uint32(def, pos, (uint32_t *)out);
            break;
        case BP_VAL_INT32:
            found = bp_cmd_get_positional_int32(def, pos, (int32_t *)out);
            break;
        case BP_VAL_CHOICE: {
            // Try string match first, then numeric
            const char *tok; size_t tlen;
            if (cmd_get_positional_token(def, pos, &tok, &tlen)) {
                if (val_match_choice(c, tok, tlen, (uint32_t *)out)) {
                    found = true;
                } else {
                    // Try numeric (1-based pick like interactive menu)
                    uint32_t pick;
                    const char *p = tok;
                    bp_num_format_t fmt;
                    if (bp_num_u32(&p, &pick, &fmt) && pick >= 1 && pick <= c->choice.count) {
                        *(uint32_t *)out = c->choice.choices[pick - 1].value;
                        found = true;
                    } else {
                        // Token present but unrecognized — invalid
                        val_print_range_error(c, def->positionals[pos - 1].name, NULL, NULL);
                        return BP_CMD_INVALID;
                    }
                }
            }
            break;
        }
        default:
            return BP_CMD_MISSING;
    }

    if (!found) {
        // Not supplied — write default and return MISSING
        switch (c->type) {
            case BP_VAL_FLOAT:  *(float *)out    = c->f.def; break;
            case BP_VAL_UINT32: *(uint32_t *)out = c->u.def; break;
            case BP_VAL_INT32:  *(int32_t *)out  = c->i.def; break;
            case BP_VAL_CHOICE: *(uint32_t *)out = c->choice.def; break;
            default: break;
        }
        return BP_CMD_MISSING;
    }

    // Validate against constraint
    if (val_check(c, out)) {
        return BP_CMD_OK;
    }
    const bp_command_positional_t *p = &def->positionals[pos - 1];
    val_print_range_error(c, p->name, NULL, out);
    return BP_CMD_INVALID;
}

bp_cmd_status_t bp_cmd_flag(const bp_command_def_t *def,
                            char flag, void *out) {
    // Find the option descriptor to get its constraint
    const bp_val_constraint_t *c = NULL;
    const bp_command_opt_t *opt = NULL;
    if (def->opts) {
        for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++) {
            if (def->opts[i].short_name == flag) {
                opt = &def->opts[i];
                c = opt->constraint;
                break;
            }
        }
    }
    if (!c || c->type == BP_VAL_NONE) {
        return BP_CMD_MISSING;
    }

    // Parse flag value from command line
    bool found = false;
    switch (c->type) {
        case BP_VAL_FLOAT:
            found = bp_cmd_get_float(def, flag, (float *)out);
            break;
        case BP_VAL_UINT32:
            found = bp_cmd_get_uint32(def, flag, (uint32_t *)out);
            break;
        case BP_VAL_INT32:
            found = bp_cmd_get_int32(def, flag, (int32_t *)out);
            break;
        case BP_VAL_CHOICE: {
            // Get raw string token from flag value
            const char *val;
            size_t val_len;
            if (cmd_scan_flag(def, flag, &val, &val_len) && val && val_len > 0) {
                if (val_match_choice(c, val, val_len, (uint32_t *)out)) {
                    found = true;
                } else {
                    // Token present but unrecognized
                    val_print_range_error(c, opt ? opt->long_name : NULL, NULL, NULL);
                    return BP_CMD_INVALID;
                }
            }
            break;
        }
        default:
            return BP_CMD_MISSING;
    }

    if (!found) {
        // Not supplied — write default and return MISSING
        switch (c->type) {
            case BP_VAL_FLOAT:  *(float *)out    = c->f.def; break;
            case BP_VAL_UINT32: *(uint32_t *)out = c->u.def; break;
            case BP_VAL_INT32:  *(int32_t *)out  = c->i.def; break;
            case BP_VAL_CHOICE: *(uint32_t *)out = c->choice.def; break;
            default: break;
        }
        return BP_CMD_MISSING;
    }

    // Validate
    if (val_check(c, out)) {
        return BP_CMD_OK;
    }
    val_print_range_error(c, opt ? opt->long_name : NULL, NULL, out);
    return BP_CMD_INVALID;
}

bp_cmd_status_t bp_cmd_prompt(const bp_val_constraint_t *con, void *out) {
    if (!con || con->type == BP_VAL_NONE) {
        return BP_CMD_EXIT;
    }

    // Print description from constraint prompt key
    if (con->prompt) {
        printf("%s%s%s", ui_term_color_info(), GET_T(con->prompt), ui_term_color_reset());
    }

    return val_prompt_loop(con, out);
}

/*
 * =============================================================================
 * Destructive-action confirmation
 * =============================================================================
 */

bool bp_cmd_confirm(const bp_command_def_t *def, const char *message) {
    // Skip prompt if -y flag is present
    if (def && bp_cmd_find_flag(def, 'y')) {
        return true;
    }

    // Self-contained y/n prompt loop (no ui_prompt_bool dependency)
    prompt_result result;
    bool value;
    printf("%s", message);
    while (true) {
        printf("\r\n%sy/n >%s \x03", ui_term_color_prompt(), ui_term_color_reset());

        if (!ui_prompt_user_input()) {
            return false;
        }

        ui_parse_get_bool(&result, &value);
        printf("\r\n");

        if (result.exit) {
            return false;
        }
        if (result.success) {
            return value;
        }
        // Invalid input — loop
    }
}

bp_yn_result_t bp_cmd_yes_no_exit(const char *message) {
    prompt_result result;
    bool value;
    printf("%s", message);
    while (true) {
        printf("\r\n%sy/n, x to exit (Y) >%s \x03",
               ui_term_color_prompt(), ui_term_color_reset());

        if (!ui_prompt_user_input()) {
            return BP_YN_EXIT;
        }

        ui_parse_get_bool(&result, &value);
        printf("\r\n");

        if (result.exit) {
            return BP_YN_EXIT;
        }
        if (result.no_value) {
            return BP_YN_YES; // enter = default yes
        }
        if (result.success) {
            return value ? BP_YN_YES : BP_YN_NO;
        }
        // Invalid input — loop
    }
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
    #define HELP_INDENT "  "

    // Synopsis (first line of usage[])
    if (def->usage && def->usage_count > 0) {
        printf("usage:\r\n");
        printf("%s%s", HELP_INDENT, ui_term_color_info());
        printf(def->usage[0], ui_term_color_reset());
        printf("%s\r\n", ui_term_color_reset());
    }

    // Section heading from description
    if (def->description) {
        printf("\r\n%s%s%s\r\n",
               ui_term_color_info(),
               GET_T(def->description),
               ui_term_color_reset());
    }

    // Action verbs
    if (def->action_delegate) {
        printf("\r\nactions:\r\n");
        for (uint32_t i = 0; ; i++) {
            const char *verb = def->action_delegate->verb_at(i);
            if (!verb) break;
            int len = strlen(verb);
            printf("%s%s%s%s",
                   HELP_INDENT,
                   ui_term_color_prompt(),
                   verb,
                   ui_term_color_reset());
            if (len < HELP_COL_WIDTH) {
                printf("%*s", HELP_COL_WIDTH - len, "");
            } else {
                printf(" ");
            }
            printf("\r\n");
        }
    } else if (def->actions && def->action_count > 0) {
        printf("\r\nactions:\r\n");
        for (uint32_t i = 0; i < def->action_count; i++) {
            int len = strlen(def->actions[i].verb);
            printf("%s%s%s%s",
                   HELP_INDENT,
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

    // Positional arguments
    if (def->positionals && def->positional_count > 0) {
        printf("\r\npositional arguments:\r\n");
        for (uint32_t i = 0; i < def->positional_count; i++) {
            const bp_command_positional_t *pa = &def->positionals[i];
            if (!pa->name) break;

            char arg_str[48];
            int pos;
            if (pa->required) {
                pos = snprintf(arg_str, sizeof(arg_str), "<%s>", pa->name);
            } else {
                pos = snprintf(arg_str, sizeof(arg_str), "[%s]", pa->name);
            }

            printf("%s%s%s%s",
                   HELP_INDENT,
                   ui_term_color_prompt(),
                   arg_str,
                   ui_term_color_reset());
            if (pos < HELP_COL_WIDTH) {
                printf("%*s", HELP_COL_WIDTH - pos, "");
            } else {
                printf(" ");
            }
            printf("%s%s%s\r\n",
                   ui_term_color_info(),
                   pa->description ? GET_T(pa->description) : "",
                   ui_term_color_reset());
        }
    }

    // Flags
    {
        bool header_printed = false;
        if (def->opts) {
            printf("\r\noptions:\r\n");
            header_printed = true;
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
                    if (o->arg_type == BP_ARG_REQUIRED) {
                        pos += snprintf(flag_str + pos, sizeof(flag_str) - pos, " <%s>", o->arg_hint);
                    } else {
                        pos += snprintf(flag_str + pos, sizeof(flag_str) - pos, " [%s]", o->arg_hint);
                    }
                }

                printf("%s%s%s%s",
                       HELP_INDENT,
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
        // Always show the universal -h/--help flag
        if (!header_printed) printf("\r\noptions:\r\n");
        printf("%s%s-h, --help      %s%*s%s%s%s\r\n",
               HELP_INDENT,
               ui_term_color_prompt(),
               ui_term_color_reset(),
               0, "",
               ui_term_color_info(),
               GET_T(T_HELP_FLAG),
               ui_term_color_reset());
    }

    // Examples (usage[] lines 1+)
    if (def->usage && def->usage_count > 1) {
        printf("\r\nexamples:\r\n");
        for (uint32_t i = 1; i < def->usage_count; i++) {
            printf("%s%s", HELP_INDENT, ui_term_color_info());
            printf(def->usage[i], ui_term_color_reset());
            printf("\r\n");
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
 * Linenoise hints and completion
 * =============================================================================
 */

// Shared scratch buffer for hint text (static — returned to linenoise)
static char hint_buf[64];

/* ── Shared helpers for hint and completion ─────────────────────────── */

/**
 * @brief Unified verb access — works with delegate or static actions array.
 * @return Verb string at index, NULL past the end.
 */
static const char *def_verb_at(const bp_command_def_t *def, uint32_t index) {
    if (def->action_delegate) return def->action_delegate->verb_at(index);
    if (def->actions && index < def->action_count) return def->actions[index].verb;
    return NULL;
}

/**
 * @brief Resolve effective def by checking for a completed delegate verb.
 * @details If the first non-flag token after the command is a complete verb
 *          with a sub-definition, return that sub-def.
 */
static const bp_command_def_t *resolve_eff_def(
        const bp_command_def_t *def, const char *after_cmd, const char *end) {
    if (!def->action_delegate || !def->action_delegate->def_for_verb) return def;
    const char *fa = skip_ws(after_cmd, end);
    if (fa >= end || *fa == '-') return def;
    size_t fa_len = tok_len(fa, end);
    if (fa + fa_len >= end) return def;   /* still typing verb */
    uint32_t act;
    if (def->action_delegate->match(fa, fa_len, &act)) {
        const bp_command_def_t *sub = def->action_delegate->def_for_verb(act);
        if (sub) return sub;
    }
    return def;
}

/**
 * @brief Format hint: prefix text + optional " <arg_hint>" into hint_buf.
 */
static const char *hint_fmt(const char *prefix, const char *arg_hint) {
    if (arg_hint)
        snprintf(hint_buf, sizeof(hint_buf), "%s <%s>", prefix, arg_hint);
    else
        snprintf(hint_buf, sizeof(hint_buf), "%s", prefix);
    return hint_buf;
}

/**
 * @brief Format a positional hint " <name>" or " [name]" into hint_buf.
 */
static const char *hint_positional(const bp_command_positional_t *pa) {
    const char *h = pa->hint ? pa->hint : pa->name;
    snprintf(hint_buf, sizeof(hint_buf), pa->required ? " <%s>" : " [%s]", h);
    return hint_buf;
}

/** @brief Find first opt with a short_name, or NULL. */
static const bp_command_opt_t *first_short_opt(const bp_command_def_t *def) {
    if (!def->opts) return NULL;
    for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++)
        if (def->opts[i].short_name) return &def->opts[i];
    return NULL;
}

/** @brief Find first opt with a long_name, or NULL. */
static const bp_command_opt_t *first_long_opt(const bp_command_def_t *def) {
    if (!def->opts) return NULL;
    for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++)
        if (def->opts[i].long_name) return &def->opts[i];
    return NULL;
}

/**
 * @brief Match a command name from a defs array.
 * @return Matching def, or NULL.
 */
static const bp_command_def_t *match_cmd(const char *tok, size_t tlen,
                                         const bp_command_def_t *const *defs,
                                         size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (defs[i] && defs[i]->name && tok_eq(tok, tlen, defs[i]->name))
            return defs[i];
    }
    return NULL;
}

/**
 * @brief Parse command token from input line.
 * @details Skips leading whitespace, extracts the first token.
 * @param[out] cmd_start  Start of command token
 * @param[out] cmd_len    Length of command token
 * @param[out] rest       Pointer past the command token
 * @return false if no token found
 */
static bool parse_cmd_token(const char *buf, const char *end,
                            const char **cmd_start, size_t *cmd_len,
                            const char **rest) {
    const char *p = buf;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    *cmd_start = p;
    while (p < end && *p != ' ' && *p != '\t') p++;
    *cmd_len = p - *cmd_start;
    *rest = p;
    return *cmd_len > 0;
}

/**
 * @brief Find the last token in a line segment.
 * @return Pointer to the start of the last token.
 */
static const char *find_last_tok(const char *p, const char *end) {
    const char *last = p;
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p >= end) break;
        last = p;
        while (p < end && *p != ' ' && *p != '\t') p++;
    }
    return last;
}

/*
 * Completion buffer pool.  In BP_EMBEDDED mode linenoiseAddCompletion()
 * stores the pointer we give it — it does NOT copy the string.  So each
 * completion needs its own persistent buffer.  We keep a small static
 * ring re-used each time the completion callback is invoked (linenoise
 * rebuilds the table from scratch on every key).
 */
#define COMP_SLOTS  8
#define COMP_BUF_SZ 64
static char comp_pool[COMP_SLOTS][COMP_BUF_SZ];
static int  comp_next;

/**
 * @brief Add a completion: line[0..pfx_len) + suffix.
 */
static void comp_add(const char *line, size_t pfx_len, const char *suffix,
                     bp_cmd_add_completion_fn add_fn, void *ud) {
    if (comp_next >= COMP_SLOTS) return;
    size_t slen = strlen(suffix);
    if (pfx_len + slen >= COMP_BUF_SZ) return;
    char *cb = comp_pool[comp_next++];
    memcpy(cb, line, pfx_len);
    memcpy(cb + pfx_len, suffix, slen + 1);
    add_fn(cb, ud);
}

/* ── Hint callback ─────────────────────────────────────────────────── */

const char *bp_cmd_hint(const char *buf, size_t len,
                        const bp_command_def_t *const *defs, size_t count) {
    if (!buf || len == 0 || !defs) return NULL;

    const char *end = buf + len;
    const char *cmd_start, *rest;
    size_t cmd_len;
    if (!parse_cmd_token(buf, end, &cmd_start, &cmd_len, &rest)) return NULL;

    /* ── Match command ── */
    const bp_command_def_t *def = match_cmd(cmd_start, cmd_len, defs, count);
    if (!def) {
        /* Partial command match — only while still typing the first token */
        if (rest >= end) {
            for (size_t i = 0; i < count; i++) {
                if (defs[i] && defs[i]->name &&
                    cmd_len < strlen(defs[i]->name) &&
                    memcmp(cmd_start, defs[i]->name, cmd_len) == 0)
                    return hint_fmt(defs[i]->name + cmd_len, NULL);
            }
        }
        return NULL;
    }

    /* ── Nothing after command → suggest first verb or positional ── */
    const char *after = skip_ws(rest, end);
    if (after >= end) {
        const char *first = def_verb_at(def, 0);
        if (first) { snprintf(hint_buf, sizeof(hint_buf), " %s", first); return hint_buf; }
        if (def->positionals && def->positional_count > 0)
            return hint_positional(&def->positionals[0]);
        return NULL;
    }

    /* ── Find last token + resolve effective def ── */
    const char *last_tok = find_last_tok(after, end);
    const bp_command_def_t *eff_def = resolve_eff_def(def, rest, end);
    size_t tl = tok_len(last_tok, end);

    /* ── Bare "-" → suggest first short flag ── */
    if (*last_tok == '-' && tl == 1) {
        const bp_command_opt_t *opt = first_short_opt(eff_def);
        if (opt) { char c[2] = { opt->short_name, '\0' }; return hint_fmt(c, opt->arg_hint); }
        return hint_fmt("h", NULL);
    }

    /* ── Long flag: "--" or "--xxx" ── */
    if (last_tok[0] == '-' && tl >= 2 && last_tok[1] == '-') {
        const char *typed = last_tok + 2;
        size_t typed_len = tl - 2;

        if (typed_len == 0) {
            const bp_command_opt_t *opt = first_long_opt(eff_def);
            return opt ? hint_fmt(opt->long_name, opt->arg_hint) : hint_fmt("help", NULL);
        }
        /* Partial long flag */
        if (eff_def->opts) {
            for (int i = 0; eff_def->opts[i].long_name || eff_def->opts[i].short_name; i++) {
                const char *ln = eff_def->opts[i].long_name;
                if (ln && typed_len < strlen(ln) &&
                    memcmp(typed, ln, typed_len) == 0)
                    return hint_fmt(ln + typed_len, eff_def->opts[i].arg_hint);
            }
        }
        /* Exact-match long flag → suggest its arg_hint */
        if (eff_def->opts) {
            const bp_command_opt_t *opt = find_opt_by_long_name(eff_def, typed, typed_len);
            if (opt && opt->arg_hint) return hint_fmt("", opt->arg_hint);
        }
        if (typed_len < 4 && memcmp(typed, "help", typed_len) == 0)
            return hint_fmt("help" + typed_len, NULL);
        return NULL;
    }

    /* ── Complete short flag "-f" → suggest arg_hint ── */
    if (*last_tok == '-' && tl == 2 && last_tok[1] != '-') {
        const bp_command_opt_t *opt = find_opt_in_def(eff_def, last_tok[1] | 0x20);
        if (!opt) opt = find_opt_in_def(eff_def, last_tok[1]);
        if (opt && opt->arg_hint) return hint_fmt("", opt->arg_hint);
        return NULL;
    }

    /* ── Partial action verb match ── */
    if (*last_tok != '-') {
        for (uint32_t i = 0; ; i++) {
            const char *verb = def_verb_at(def, i);
            if (!verb) break;
            if (tok_prefix_ci(last_tok, tl, verb, strlen(verb)))
                return hint_fmt(verb + tl, NULL);
        }
    }

    /* ── After resolved sub-def verb, suggest first sub-def flag ── */
    if (eff_def != def && eff_def->opts &&
        len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t')) {
        const bp_command_opt_t *opt = first_short_opt(eff_def);
        if (opt) {
            char pfx[5];
            snprintf(pfx, sizeof(pfx), " -%c", opt->short_name);
            return hint_fmt(pfx, opt->arg_hint);
        }
    }

    /* ── Positional argument hint ── */
    if (def->positionals && def->positional_count > 0 &&
        !(last_tok < end && *last_tok == '-')) {
        uint32_t pos_count = 0;
        const char *sc = skip_ws(buf, end);
        sc = skip_tok(sc, end);  /* skip command name */

        while (sc < end) {
            sc = skip_ws(sc, end);
            if (sc >= end || *sc == '\0') break;

            if (*sc == '-') {
                if (sc + 1 < end && sc[1] == '-') {
                    sc += 2;
                    const char *nm = sc;
                    while (sc < end && *sc != '=' && *sc != ' ' &&
                           *sc != '\t' && *sc != '\0') sc++;
                    const bp_command_opt_t *opt = find_opt_by_long_name(def, nm, sc - nm);
                    if (*sc == '=') sc = skip_tok(sc, end);
                    else if (opt && opt->arg_type != BP_ARG_NONE) {
                        sc = skip_ws(sc, end);
                        if (sc < end && *sc != '-') sc = skip_tok(sc, end);
                    }
                } else if (sc + 1 < end && sc[1] != ' ' && sc[1] != '\0') {
                    char oc = sc[1]; sc += 2;
                    const bp_command_opt_t *opt = find_opt_in_def(def, oc | 0x20);
                    if (!opt) opt = find_opt_in_def(def, oc);
                    if (opt && opt->arg_type != BP_ARG_NONE) {
                        sc = skip_ws(sc, end);
                        if (sc < end && *sc != '-') sc = skip_tok(sc, end);
                    }
                } else {
                    sc = skip_tok(sc, end);
                }
                continue;
            }
            pos_count++;
            sc = skip_tok(sc, end);
        }

        bool at_new = (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t'));
        if (at_new && pos_count < def->positional_count)
            return hint_positional(&def->positionals[pos_count]);
    }

    return NULL;
}

/* ── Completion callback ───────────────────────────────────────────── */

void bp_cmd_completion(const char *buf, size_t len,
                       const bp_command_def_t *const *defs, size_t count,
                       bp_cmd_add_completion_fn add_completion, void *userdata) {
    if (!buf || len == 0 || !defs || !add_completion) return;
    comp_next = 0;  /* reset pool each invocation */

    const char *end = buf + len;
    const char *cmd_start, *rest;
    size_t cmd_len;
    if (!parse_cmd_token(buf, end, &cmd_start, &cmd_len, &rest)) return;

    /* ── Still typing command name → complete command names ── */
    if (rest >= end) {
        for (size_t i = 0; i < count; i++) {
            if (defs[i] && defs[i]->name &&
                cmd_len <= strlen(defs[i]->name) &&
                memcmp(cmd_start, defs[i]->name, cmd_len) == 0)
                comp_add(buf, 0, defs[i]->name, add_completion, userdata);
        }
        return;
    }

    /* ── Find matching command ── */
    const bp_command_def_t *def = match_cmd(cmd_start, cmd_len, defs, count);
    if (!def) return;

    /* ── Find last token ── */
    const char *after = skip_ws(rest, end);
    const char *last_tok = find_last_tok(after, end);
    size_t tl = (last_tok < end) ? tok_len(last_tok, end) : 0;
    bool last_is_flag = (last_tok < end && *last_tok == '-');
    size_t pfx_len = last_tok - buf;

    const bp_command_def_t *eff_def = resolve_eff_def(def, rest, end);

    /* ── Complete action verbs ── */
    if (!last_is_flag) {
        for (uint32_t i = 0; ; i++) {
            const char *verb = def_verb_at(def, i);
            if (!verb) break;
            if (tl <= strlen(verb) && tok_prefix_ci(last_tok, tl, verb, strlen(verb) + 1))
                comp_add(buf, pfx_len, verb, add_completion, userdata);
        }
    }

    /* ── Complete flag names ── */
    if (last_is_flag) {
        if (tl >= 2 && last_tok[1] == '-') {
            /* Long flag completion: --name */
            const char *typed = last_tok + 2;
            size_t typed_len = tl - 2;
            if (eff_def->opts) {
                for (int i = 0; eff_def->opts[i].long_name || eff_def->opts[i].short_name; i++) {
                    const char *ln = eff_def->opts[i].long_name;
                    if (ln && typed_len <= strlen(ln) &&
                        memcmp(typed, ln, typed_len) == 0) {
                        char full[COMP_BUF_SZ];
                        snprintf(full, sizeof(full), "--%s", ln);
                        comp_add(buf, pfx_len, full, add_completion, userdata);
                    }
                }
            }
            if (typed_len <= 4 && memcmp(typed, "help", typed_len) == 0)
                comp_add(buf, pfx_len, "--help", add_completion, userdata);
        } else if (tl <= 2) {
            /* Short flag completion: -x */
            if (eff_def->opts) {
                for (int i = 0; eff_def->opts[i].long_name || eff_def->opts[i].short_name; i++) {
                    if (eff_def->opts[i].short_name) {
                        char fs[4] = { '-', eff_def->opts[i].short_name, '\0' };
                        if (tl == 1 || (tl == 2 && last_tok[1] == eff_def->opts[i].short_name))
                            comp_add(buf, pfx_len, fs, add_completion, userdata);
                    }
                }
            }
            if (tl == 1 || (tl == 2 && (last_tok[1] | 0x20) == 'h'))
                comp_add(buf, pfx_len, "-h", add_completion, userdata);
        }
    }

    /* Fallback: echo input so linenoise doesn't insert a literal tab */
    if (comp_next == 0) {
        char *cb = comp_pool[comp_next++];
        size_t cl = (len < COMP_BUF_SZ - 1) ? len : COMP_BUF_SZ - 1;
        memcpy(cb, buf, cl);
        cb[cl] = '\0';
        add_completion(cb, userdata);
    }
}
