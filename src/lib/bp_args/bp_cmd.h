/**
 * @file bp_cmd.h
 * @brief Unified command definition, help, and argument query API.
 * @details Provides a single struct that drives:
 *          1. Stateless argument parsing (query pattern)
 *          2. Help/usage display
 *          3. Linenoise hints and completion
 *
 *          Each command defines one static bp_command_def_t that replaces
 *          the separate ui_help_options[], cmdln_action_t[], and bp_arg_opt_t[].
 *
 * @example
 *     // ── Define once ──
 *     static const bp_command_opt_t flash_opts[] = {
 *         {"file",  'f', BP_ARG_REQUIRED, "file", T_HELP_FLASH_FILE_FLAG},
 *         {"erase", 'e', BP_ARG_NONE,     NULL,   T_HELP_FLASH_ERASE_FLAG},
 *         {0}
 *     };
 *     static const bp_command_action_t flash_actions[] = {
 *         {FLASH_PROBE, "probe", T_HELP_FLASH_PROBE},
 *         {FLASH_WRITE, "write", T_HELP_FLASH_WRITE},
 *     };
 *     static const bp_command_def_t flash_def = {
 *         .name = "flash",
 *         .description = T_HELP_FLASH,
 *         .actions = flash_actions,
 *         .action_count = count_of(flash_actions),
 *         .opts = flash_opts,
 *         .usage = usage,
 *         .usage_count = count_of(usage),
 *     };
 *
 *     // ── Use in handler ──
 *     void flash(struct command_result* res) {
 *         if (bp_cmd_help_check(&flash_def, res->help_flag)) return;
 *
 *         uint32_t action;
 *         if (!bp_cmd_get_action(&flash_def, &action)) {
 *             bp_cmd_help_show(&flash_def);
 *             return;
 *         }
 *         char file[13];
 *         bool has_file = bp_cmd_get_string(&flash_def, 'f', file, sizeof(file));
 *         bool erase    = bp_cmd_find_flag(&flash_def, 'e');
 *     }
 */

#ifndef BP_CMD_H
#define BP_CMD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * =============================================================================
 * Argument type enum
 * =============================================================================
 */

/**
 * @brief Option argument requirement.
 */
typedef enum {
    BP_ARG_NONE = 0,      ///< Boolean flag, no argument
    BP_ARG_REQUIRED = 1,  ///< Requires an argument value
    BP_ARG_OPTIONAL = 2   ///< Argument value is optional
} bp_arg_type_t;

/*
 * =============================================================================
 * Value constraint (optional validation / auto-prompt)
 * =============================================================================
 */

/**
 * @brief Constraint value type tag.
 */
typedef enum {
    BP_VAL_NONE = 0,    ///< No constraint (skip validation)
    BP_VAL_UINT32,      ///< Unsigned 32-bit integer range
    BP_VAL_INT32,       ///< Signed 32-bit integer range
    BP_VAL_FLOAT,       ///< Floating-point range
    BP_VAL_CHOICE,      ///< Named choice from a fixed set (e.g. parity: none/even/odd)
} bp_val_type_t;

/**
 * @brief Single named choice entry for BP_VAL_CHOICE constraints.
 * @details Maps a name string (and optional short alias) to an integer value.
 *          Used for both command-line parsing and interactive numbered menus.
 *
 * @example
 *     static const bp_val_choice_t parity_choices[] = {
 *         { "none", "n", T_UART_PARITY_MENU_1, 0 },
 *         { "even", "e", T_UART_PARITY_MENU_2, 1 },
 *         { "odd",  "o", T_UART_PARITY_MENU_3, 2 },
 *     };
 */
typedef struct {
    const char *name;       ///< Full name for cmdline match: "even", "odd", "none"
    const char *alias;      ///< Short alias: "e", "o", "n" (NULL = none)
    uint32_t    label;      ///< T_ translation key for interactive menu display
    uint32_t    value;      ///< The actual stored value (written to *out)
} bp_val_choice_t;

/**
 * @brief Optional value constraint.
 * @details Attach via pointer to a positional or option descriptor.
 *          NULL pointer = no validation (fully backward compatible).
 *          When present, the parser/prompt helpers can:
 *          1. Range-check a parsed value and print an error.
 *          2. Prompt the user interactively if the value was not supplied.
 *
 * @note   Allocate as `static const` next to your positional/opt tables.
 *
 * @example
 *     static const bp_val_constraint_t voltage_range = {
 *         .type = BP_VAL_FLOAT,
 *         .f = { .min = 0.8f, .max = 5.0f, .def = 3.3f },
 *         .prompt = 0,   // translation key, 0 = no interactive prompt text
 *     };
 */
typedef struct {
    bp_val_type_t type;       ///< Which union member is active
    union {
        struct { uint32_t min, max, def; } u;   ///< BP_VAL_UINT32
        struct { int32_t  min, max, def; } i;   ///< BP_VAL_INT32
        struct { float    min, max, def; } f;   ///< BP_VAL_FLOAT
        struct {                                 ///< BP_VAL_CHOICE
            const bp_val_choice_t *choices;      ///< Array of named choices
            uint32_t count;                      ///< Number of choices
            uint32_t def;                        ///< Default value (matches a choice .value)
        } choice;
    };
    uint32_t prompt;          ///< Translation key for interactive prompt text (0 = none)
    uint32_t hint;            ///< Translation key for hint text below prompt (0 = none)
} bp_val_constraint_t;

/*
 * =============================================================================
 * Unified option descriptor
 * =============================================================================
 */

/**
 * @brief Command option/flag descriptor.
 * @details Defines a single flag for parsing, help display, and hinting.
 *          Terminate arrays with {0}.
 */
typedef struct {
    const char *long_name;    ///< Long option name (without --), NULL if none
    char        short_name;   ///< Short option character, 0 if none
    bp_arg_type_t arg_type;   ///< BP_ARG_NONE / REQUIRED / OPTIONAL
    const char *arg_hint;     ///< Value placeholder for help/hints (bare word, auto-wrapped with <>/[]): "file", NULL if flag-only
    uint32_t    description;  ///< Translation key for help text
    const bp_val_constraint_t *constraint; ///< Optional value constraint, NULL = no validation
} bp_command_opt_t;

/*
 * =============================================================================
 * Action/subcommand descriptor
 * =============================================================================
 */

/**
 * @brief Subcommand/action verb descriptor.
 * @details Maps a verb string to an enum value with help text.
 */
typedef struct {
    uint32_t    action;       ///< Action enum value
    const char *verb;         ///< Verb string ("probe", "dump", etc)
    uint32_t    description;  ///< Translation key for help text
} bp_command_action_t;

/**
 * @brief Action verb delegate for dynamic/external verb sources.
 * @details When a command's verbs come from a runtime data structure
 *          (e.g. the modes[] array), set action_delegate instead of
 *          the static actions array. All 5 action pipelines
 *          (get_action, help, hint-first, hint-partial, completion)
 *          will call through these two functions.
 *
 * verb_at(index) — Return the verb string at the given index,
 *                  or NULL if index >= count.  Used to enumerate.
 * match(tok, len, action_out) — Case-insensitive match of a user
 *                  token against the verb source. Writes the action
 *                  enum value into *action_out on match.
 * def_for_verb(action) — Return the command definition for the
 *                  resolved action verb, or NULL if the verb has
 *                  no sub-definition. Used for flag hinting and
 *                  contextual help after the verb is resolved.
 */
typedef struct {
    const char *(*verb_at)(uint32_t index);  ///< Return verb at index, NULL = end
    bool (*match)(const char *tok, size_t len, uint32_t *action_out); ///< Match token → action
    const struct bp_command_def *(*def_for_verb)(uint32_t action); ///< Sub-def for resolved verb, NULL = none
} bp_action_delegate_t;

/*
 * =============================================================================
 * Positional argument descriptor
 * =============================================================================
 */

/**
 * @brief Positional argument descriptor.
 * @details Defines a positional (non-flag) argument for help display and hinting.
 *          Order in the array matches position index (1-based in the API).
 *          Terminate arrays with {0}.
 */
typedef struct {
    const char *name;         ///< Argument name for display: "bank", "voltage"
    const char *hint;         ///< Value placeholder for hints (bare word, auto-wrapped with <>/[]): "volts", "addr". NULL = use name
    uint32_t    description;  ///< Translation key for help text
    bool        required;     ///< true if argument is required
    const bp_val_constraint_t *constraint; ///< Optional value constraint, NULL = no validation
} bp_command_positional_t;

/*
 * =============================================================================
 * Full command definition
 * =============================================================================
 */

/**
 * @brief Complete command definition.
 * @details One per command. Drives parsing, help, and hints.
 *          All pointers are to static const data — zero allocation.
 */
typedef struct bp_command_def {
    const char *name;                       ///< Command name ("flash", "eeprom")
    uint32_t    description;                ///< Translation key for top-level help

    const bp_command_action_t *actions;     ///< Subcommand verbs, NULL if none
    uint32_t action_count;                  ///< Number of actions
    const bp_action_delegate_t *action_delegate; ///< Dynamic verb source, NULL = use actions array

    const bp_command_opt_t *opts;           ///< Flags/options, {0}-terminated

    const bp_command_positional_t *positionals; ///< Positional args, {0}-terminated, NULL if none
    uint32_t positional_count;              ///< Number of positional args

    const char *const *usage;               ///< Usage example strings
    uint32_t usage_count;                   ///< Number of usage lines
} bp_command_def_t;

/*
 * =============================================================================
 * Action verb query
 * =============================================================================
 */

/**
 * @brief Find action verb in current command line.
 * @param def       Command definition with actions array
 * @param[out] action  Matched action enum value
 * @return true if action verb found, false if none matched
 */
bool bp_cmd_get_action(const bp_command_def_t *def, uint32_t *action);

/*
 * =============================================================================
 * Flag query API (stateless, re-scans each call)
 * =============================================================================
 */

/**
 * @brief Check if a flag is present.
 * @param def   Command definition (needed to correctly skip other flags' values)
 * @param flag  Short flag character (e.g. 'e')
 * @return true if flag found
 */
bool bp_cmd_find_flag(const bp_command_def_t *def, char flag);

/**
 * @brief Get uint32 value for a flag.
 * @param def         Command definition
 * @param flag        Short flag character
 * @param[out] value  Parsed value (supports 0x, 0b, decimal)
 * @return true if flag found with valid uint32 value
 */
bool bp_cmd_get_uint32(const bp_command_def_t *def, char flag, uint32_t *value);

/**
 * @brief Get string value for a flag.
 * @param def         Command definition
 * @param flag        Short flag character
 * @param[out] buf    Output buffer
 * @param maxlen      Buffer size (including null terminator)
 * @return true if flag found with string value
 */
bool bp_cmd_get_string(const bp_command_def_t *def, char flag, char *buf, size_t maxlen);

/**
 * @brief Get float value for a flag.
 * @param def         Command definition
 * @param flag        Short flag character
 * @param[out] value  Parsed float value
 * @return true if flag found with valid float value
 */
bool bp_cmd_get_float(const bp_command_def_t *def, char flag, float *value);

/**
 * @brief Get int32 value for a flag.
 * @param def         Command definition
 * @param flag        Short flag character
 * @param[out] value  Parsed value
 * @return true if flag found with valid int32 value
 */
bool bp_cmd_get_int32(const bp_command_def_t *def, char flag, int32_t *value);

/*
 * =============================================================================
 * Positional argument query (stateless, re-scans each call)
 * =============================================================================
 */

/**
 * @brief Get positional (non-flag) argument as string by index.
 * @details Position 0 is the command name itself. Position 1 is the first
 *          non-flag token after the command name. Flags and their consumed
 *          values are skipped using the command definition.
 * @param def         Command definition (needed to skip flags' values)
 * @param pos         Position index (0 = command, 1 = first arg, ...)
 * @param[out] buf    Output buffer
 * @param maxlen      Buffer size (including null terminator)
 * @return true if positional argument found at index
 */
bool bp_cmd_get_positional_string(const bp_command_def_t *def, uint32_t pos,
                                  char *buf, size_t maxlen);

/**
 * @brief Get positional argument as uint32 by index.
 * @param def         Command definition
 * @param pos         Position index (0 = command, 1 = first arg, ...)
 * @param[out] value  Parsed value (supports 0x, 0b, decimal)
 * @return true if positional argument found with valid uint32 value
 */
bool bp_cmd_get_positional_uint32(const bp_command_def_t *def, uint32_t pos,
                                  uint32_t *value);

/**
 * @brief Get positional argument as int32 by index.
 * @param def         Command definition
 * @param pos         Position index
 * @param[out] value  Parsed value
 * @return true if positional argument found with valid int32 value
 */
bool bp_cmd_get_positional_int32(const bp_command_def_t *def, uint32_t pos,
                                 int32_t *value);

/**
 * @brief Get positional argument as float by index.
 * @param def         Command definition
 * @param pos         Position index
 * @param[out] value  Parsed float value
 * @return true if positional argument found with valid float value
 */
bool bp_cmd_get_positional_float(const bp_command_def_t *def, uint32_t pos,
                                 float *value);

/**
 * @brief Get raw pointer to everything after the command name.
 * @details Skips the command word (position 0) and any leading whitespace,
 *          returns a pointer into the live command line buffer. The returned
 *          region is NOT null-terminated; use *len.
 * @param def         Command definition (unused currently, reserved)
 * @param[out] out    Pointer to start of remaining text
 * @param[out] len    Length of remaining text
 * @return true if any content exists after the command name
 */
bool bp_cmd_get_remainder(const bp_command_def_t *def,
                          const char **out, size_t *len);

/*
 * =============================================================================
 * Constraint-aware argument resolution
 * =============================================================================
 */

/**
 * @brief Result status from constraint-aware argument fetch.
 */
typedef enum {
    BP_CMD_OK = 0,       ///< Value obtained and valid
    BP_CMD_MISSING,      ///< Not supplied on command line
    BP_CMD_INVALID,      ///< Supplied but failed validation (error already printed)
    BP_CMD_EXIT,         ///< User exited interactive prompt
} bp_cmd_status_t;

/**
 * @brief Parse positional argument from command line and validate.
 * @details Uses the constraint attached to the positional descriptor to:
 *          1. Select the correct parser (float/uint32/int32) from the type tag
 *          2. Range-check the parsed value, print error if out of range
 *
 *          Does NOT prompt. Returns immediately.
 *
 * @param def   Command definition
 * @param pos   Positional index (1-based)
 * @param out   Pointer to result variable (type must match constraint)
 * @return BP_CMD_OK if parsed and valid, BP_CMD_MISSING if not on cmdline,
 *         BP_CMD_INVALID if present but out of range (error already printed)
 *
 * @example
 *     float volts;
 *     bp_cmd_status_t s = bp_cmd_positional(&def, 1, &volts);
 *     if (s == BP_CMD_INVALID) { res->error = true; return; }
 *     if (s == BP_CMD_MISSING) {
 *         s = bp_cmd_prompt(&voltage_range, &volts);
 *         ...
 *     }
 */
bp_cmd_status_t bp_cmd_positional(const bp_command_def_t *def,
                                  uint32_t pos, void *out);

/**
 * @brief Parse flag/option value from command line and validate.
 * @details Uses the constraint attached to the option descriptor to:
 *          1. Select the correct parser from the type tag
 *          2. Range-check the parsed value, print error if out of range
 *
 *          If the flag is not present, writes the constraint's default
 *          value to `out` and returns BP_CMD_MISSING.
 *
 * @param def   Command definition
 * @param flag  Short flag character (e.g. 'u')
 * @param out   Pointer to result variable (type must match constraint)
 * @return BP_CMD_OK if parsed and valid, BP_CMD_MISSING if flag not present
 *         (default written to out), BP_CMD_INVALID if present but out of range
 */
bp_cmd_status_t bp_cmd_flag(const bp_command_def_t *def,
                            char flag, void *out);

/**
 * @brief Interactive prompt loop driven by a constraint.
 * @details Prints the constraint's prompt text, shows default, loops
 *          until the user provides a valid value or exits.
 *
 *          Works for any constraint — positional, flag, or standalone.
 *          The developer already has the constraint; no need to look it up.
 *
 * @param con   Value constraint (must not be NULL)
 * @param out   Pointer to result variable (type must match constraint)
 * @return BP_CMD_OK if valid value obtained, BP_CMD_EXIT if user cancelled
 */
bp_cmd_status_t bp_cmd_prompt(const bp_val_constraint_t *con, void *out);

/*
 * =============================================================================
 * Destructive-action confirmation
 * =============================================================================
 */

/**
 * @brief Prompt user for y/n confirmation before a destructive action.
 * @details Self-contained y/n loop — no ui_prompt_bool dependency.
 *          If @p def is non-NULL and the command line contains a `-y` flag,
 *          the prompt is skipped and the function returns true immediately.
 *          Pass NULL for @p def when there is no `-y` bypass.
 *
 * @param def      Command definition (used to check `-y`), or NULL
 * @param message  Message printed before the y/n prompt
 * @return true if user confirmed (or `-y` present), false if declined/cancelled
 */
bool bp_cmd_confirm(const bp_command_def_t *def, const char *message);

/*
 * =============================================================================
 * Help display
 * =============================================================================
 */

/**
 * @brief Show help if help_flag is set.
 * @param def        Command definition
 * @param help_flag  true if user passed -h
 * @return true if help was displayed (caller should return)
 */
bool bp_cmd_help_check(const bp_command_def_t *def, bool help_flag);

/**
 * @brief Unconditionally display help for a command.
 * @param def  Command definition
 */
void bp_cmd_help_show(const bp_command_def_t *def);

/*
 * =============================================================================
 * Linenoise hints and completion (Phase 3 stubs)
 * =============================================================================
 */

/**
 * @brief Generate hint text for current input buffer.
 * @param buf   Current line buffer
 * @param len   Length of buffer content
 * @param defs  Array of all registered command definitions
 * @param count Number of definitions
 * @return Hint string (static buffer), or NULL if no hint
 *
 * @note Walks registered defs to find matching command, then suggests
 *       action verbs or flag names as ghost text.
 */
const char *bp_cmd_hint(const char *buf, size_t len,
                        const bp_command_def_t *const *defs, size_t count);

/**
 * @brief Generate completions for current input.
 * @param buf   Current line buffer
 * @param len   Length of buffer content
 * @param defs  Array of all registered command definitions
 * @param count Number of definitions
 * @param add_completion  Callback to add a completion string
 * @param userdata        Passed to callback
 *
 * @note Phase 3: will be wired to linenoiseSetCompletionCallback.
 */
typedef void (*bp_cmd_add_completion_fn)(const char *text, void *userdata);

void bp_cmd_completion(const char *buf, size_t len,
                       const bp_command_def_t *const *defs, size_t count,
                       bp_cmd_add_completion_fn add_completion, void *userdata);

#endif // BP_CMD_H
