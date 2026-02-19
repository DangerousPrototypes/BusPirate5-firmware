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

    // Map def index to command-line position.
    // positional_start: 0 or 1 = right after command name (default).
    //                   2 = skip one extra token (e.g. "m uart <here>").
    uint32_t start = def->positional_start ? def->positional_start : 1;
    uint32_t cmdline_pos = pos + start - 1;

    // Parse from command line using type-appropriate getter
    bool found = false;
    switch (c->type) {
        case BP_VAL_FLOAT:
            found = bp_cmd_get_positional_float(def, cmdline_pos, (float *)out);
            break;
        case BP_VAL_UINT32:
            found = bp_cmd_get_positional_uint32(def, cmdline_pos, (uint32_t *)out);
            break;
        case BP_VAL_INT32:
            found = bp_cmd_get_positional_int32(def, cmdline_pos, (int32_t *)out);
            break;
        case BP_VAL_CHOICE: {
            // Try string match first, then numeric
            const char *tok; size_t tlen;
            if (cmd_get_positional_token(def, cmdline_pos, &tok, &tlen)) {
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
    if (def->actions && def->action_count > 0) {
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
    if (def->opts) {
        printf("\r\noptions:\r\n");
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
        // Partial command match — only if still typing the first token
        if (p >= end) {
            for (size_t i = 0; i < count; i++) {
                if (defs[i] && defs[i]->name &&
                    cmd_len < strlen(defs[i]->name) &&
                    memcmp(cmd_start, defs[i]->name, cmd_len) == 0) {
                    snprintf(hint_buf, sizeof(hint_buf), "%s", defs[i]->name + cmd_len);
                    return hint_buf;
                }
            }
        }
        return NULL;
    }

    // Command matched — check what the user is currently typing
    while (p < end && (*p == ' ' || *p == '\t')) p++;

    if (p >= end) {
        // Nothing after command — suggest first action verb or positional
        if (def->actions && def->action_count > 0) {
            snprintf(hint_buf, sizeof(hint_buf), " %s", def->actions[0].verb);
            return hint_buf;
        }
        if (def->positionals && def->positional_count > 0) {
            const bp_command_positional_t *pa = &def->positionals[0];
            const char *h = pa->hint ? pa->hint : pa->name;
            if (pa->required) {
                snprintf(hint_buf, sizeof(hint_buf), " <%s>", h);
            } else {
                snprintf(hint_buf, sizeof(hint_buf), " [%s]", h);
            }
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
                    snprintf(hint_buf, sizeof(hint_buf), "%c <%s>",
                             def->opts[i].short_name, def->opts[i].arg_hint);
                } else {
                    snprintf(hint_buf, sizeof(hint_buf), "%c", def->opts[i].short_name);
                }
                return hint_buf;
            }
        }
    }

    // If last token is "--", suggest first available long flag
    if (last_tok && last_tok[0] == '-' && tok_len(last_tok, end) >= 2 &&
        last_tok[1] == '-' && def->opts) {
        size_t tl = tok_len(last_tok, end);
        const char *typed_name = last_tok + 2;
        size_t typed_len = tl - 2;

        if (typed_len == 0) {
            // Bare "--": suggest first long flag name
            for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++) {
                if (def->opts[i].long_name) {
                    if (def->opts[i].arg_hint) {
                        snprintf(hint_buf, sizeof(hint_buf), "%s <%s>",
                                 def->opts[i].long_name, def->opts[i].arg_hint);
                    } else {
                        snprintf(hint_buf, sizeof(hint_buf), "%s",
                                 def->opts[i].long_name);
                    }
                    return hint_buf;
                }
            }
        } else {
            // Partial long flag: suggest matching completion
            for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++) {
                if (def->opts[i].long_name) {
                    size_t ln_len = strlen(def->opts[i].long_name);
                    if (typed_len < ln_len &&
                        memcmp(typed_name, def->opts[i].long_name, typed_len) == 0) {
                        const char *rest = def->opts[i].long_name + typed_len;
                        if (def->opts[i].arg_hint) {
                            snprintf(hint_buf, sizeof(hint_buf), "%s <%s>",
                                     rest, def->opts[i].arg_hint);
                        } else {
                            snprintf(hint_buf, sizeof(hint_buf), "%s", rest);
                        }
                        return hint_buf;
                    }
                }
            }
        }
    }

    // If last token is a complete flag like "-f", suggest its arg_hint
    if (last_tok && *last_tok == '-' && tok_len(last_tok, end) == 2 && last_tok[1] != '-') {
        char fc = last_tok[1];
        const bp_command_opt_t *opt = find_opt_in_def(def, fc | 0x20);
        if (!opt) opt = find_opt_in_def(def, fc);
        if (opt && opt->arg_hint) {
            snprintf(hint_buf, sizeof(hint_buf), " <%s>", opt->arg_hint);
            return hint_buf;
        }
    }

    // If last token is a complete long flag like "--file", suggest its arg_hint
    if (last_tok && last_tok[0] == '-' && tok_len(last_tok, end) > 2 &&
        last_tok[1] == '-' && def->opts) {
        size_t tl = tok_len(last_tok, end);
        const bp_command_opt_t *opt = find_opt_by_long_name(def, last_tok + 2, tl - 2);
        if (opt && opt->arg_hint) {
            snprintf(hint_buf, sizeof(hint_buf), " <%s>", opt->arg_hint);
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

    // Positional argument hint: count how many positional tokens the user has
    // entered so far and suggest the next expected positional arg.
    if (def->positionals && def->positional_count > 0) {
        // Don't hint positional if the user is currently typing a flag
        bool last_is_flag_tok = (last_tok < end && *last_tok == '-');
        if (last_is_flag_tok) return NULL;

        // Count positional tokens (skip flags and their values)
        uint32_t pos_count = 0;
        const char *scan = buf;
        const char *scan_end = end;

        // Skip command name
        scan = skip_ws(scan, scan_end);
        scan = skip_tok(scan, scan_end);

        while (scan < scan_end) {
            scan = skip_ws(scan, scan_end);
            if (scan >= scan_end || *scan == '\0') break;

            if (*scan == '-') {
                // Skip flag
                if (scan + 1 < scan_end && scan[1] == '-') {
                    // Long flag
                    scan += 2;
                    const char *nm = scan;
                    while (scan < scan_end && *scan != '=' && *scan != ' ' &&
                           *scan != '\t' && *scan != '\0') scan++;
                    size_t nm_len = scan - nm;
                    const bp_command_opt_t *opt = find_opt_by_long_name(def, nm, nm_len);
                    if (*scan == '=') scan = skip_tok(scan, scan_end);
                    else if (opt && opt->arg_type != BP_ARG_NONE) {
                        scan = skip_ws(scan, scan_end);
                        if (scan < scan_end && *scan != '-') scan = skip_tok(scan, scan_end);
                    }
                } else if (scan + 1 < scan_end && scan[1] != ' ' && scan[1] != '\0') {
                    // Short flag
                    char oc = scan[1];
                    scan += 2;
                    const bp_command_opt_t *opt = find_opt_in_def(def, oc | 0x20);
                    if (!opt) opt = find_opt_in_def(def, oc);
                    if (opt && opt->arg_type != BP_ARG_NONE) {
                        scan = skip_ws(scan, scan_end);
                        if (scan < scan_end && *scan != '-') scan = skip_tok(scan, scan_end);
                    }
                } else {
                    scan = skip_tok(scan, scan_end);
                }
                continue;
            }

            // Non-flag token = positional
            pos_count++;
            scan = skip_tok(scan, scan_end);
        }

        // Check if trailing whitespace (cursor at new token position)
        bool at_new_token = (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t'));

        // Determine which positional to hint
        uint32_t hint_pos;
        if (at_new_token) {
            hint_pos = pos_count; // next positional (0-based into array)
        } else {
            // Currently typing a token — already counted, so suggest current
            hint_pos = (pos_count > 0) ? pos_count - 1 : 0;
        }

        // Don't hint positional if the user is typing an action verb (already handled)
        // or a flag. Only hint if we're past existing action verbs.
        if (at_new_token && hint_pos < def->positional_count) {
            const char *h = def->positionals[hint_pos].hint ?
                            def->positionals[hint_pos].hint :
                            def->positionals[hint_pos].name;
            if (def->positionals[hint_pos].required) {
                snprintf(hint_buf, sizeof(hint_buf), " <%s>", h);
            } else {
                snprintf(hint_buf, sizeof(hint_buf), " [%s]", h);
            }
            return hint_buf;
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

    // Check if we're still typing the command name (no space after it)
    if (p >= end) {
        // Complete command names (prefix matches)
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

    size_t tl = (last_tok < end) ? tok_len(last_tok, end) : 0;
    bool last_is_flag = (last_tok < end && *last_tok == '-');

    /*
     * Completion buffer pool.  In BP_EMBEDDED mode linenoiseAddCompletion()
     * stores the pointer we give it — it does NOT copy the string.  So each
     * completion needs its own persistent buffer.  We keep a small static
     * ring that is re-used each time the completion callback is invoked
     * (linenoise rebuilds the table from scratch on every key).
     */
    #define COMP_SLOTS 16
    #define COMP_BUF_SZ 64
    static char comp_pool[COMP_SLOTS][COMP_BUF_SZ];
    static int  comp_next;          /* next free slot */
    comp_next = 0;                  /* reset each invocation */

    #define COMP_BUF() (comp_next < COMP_SLOTS ? comp_pool[comp_next++] : NULL)

    // Complete action verbs
    if (!last_is_flag && def->actions) {
        for (uint32_t i = 0; i < def->action_count; i++) {
            if (tl <= strlen(def->actions[i].verb) &&
                memcmp(last_tok, def->actions[i].verb, tl) == 0) {
                // Build full line up to last_tok, then append completed verb
                size_t prefix_len = last_tok - buf;
                char *cb = COMP_BUF();
                if (cb && prefix_len + strlen(def->actions[i].verb) < COMP_BUF_SZ) {
                    memcpy(cb, buf, prefix_len);
                    strcpy(cb + prefix_len, def->actions[i].verb);
                    add_completion(cb, userdata);
                }
            }
        }
    }

    // Complete flag names
    if (last_is_flag && def->opts) {
        if (tl >= 2 && last_tok[1] == '-') {
            // Long flag completion: --name
            const char *typed_name = last_tok + 2;
            size_t typed_len = tl - 2;
            for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++) {
                if (def->opts[i].long_name &&
                    typed_len <= strlen(def->opts[i].long_name) &&
                    memcmp(typed_name, def->opts[i].long_name, typed_len) == 0) {
                    size_t prefix_len = last_tok - buf;
                    size_t long_len = strlen(def->opts[i].long_name);
                    char *cb = COMP_BUF();
                    if (cb && prefix_len + 2 + long_len < COMP_BUF_SZ) {
                        memcpy(cb, buf, prefix_len);
                        cb[prefix_len] = '-';
                        cb[prefix_len + 1] = '-';
                        strcpy(cb + prefix_len + 2, def->opts[i].long_name);
                        add_completion(cb, userdata);
                    }
                }
            }
        } else if (tl <= 2) {
            // Short flag completion: -x
            for (int i = 0; def->opts[i].long_name || def->opts[i].short_name; i++) {
                if (def->opts[i].short_name) {
                    char flag_str[4] = { '-', def->opts[i].short_name, '\0' };
                    if (tl == 1 || (tl == 2 && last_tok[1] == def->opts[i].short_name)) {
                        size_t prefix_len = last_tok - buf;
                        char *cb = COMP_BUF();
                        if (cb && prefix_len + 2 < COMP_BUF_SZ) {
                            memcpy(cb, buf, prefix_len);
                            strcpy(cb + prefix_len, flag_str);
                            add_completion(cb, userdata);
                        }
                    }
                }
            }
        }
    }

    /* Fallback: if no completions were added, echo the current input as a
     * single completion so linenoise doesn't insert a literal tab char.  */
    if (comp_next == 0) {
        char *cb = COMP_BUF();
        if (cb) {
            size_t copylen = (len < COMP_BUF_SZ - 1) ? len : COMP_BUF_SZ - 1;
            memcpy(cb, buf, copylen);
            cb[copylen] = '\0';
            add_completion(cb, userdata);
        }
    }
}
