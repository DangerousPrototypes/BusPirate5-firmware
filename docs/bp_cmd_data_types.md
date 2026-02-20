+++
weight = 90401
title = 'bp_cmd Data Types'
+++

# bp_cmd Data Types

> Developer reference for every type defined in `src/lib/bp_args/bp_cmd.h` — the unified command definition, parsing, and validation API.

---

## `bp_arg_type_t` — Argument Type Enum

Specifies whether a flag/option takes an argument value.

```c
typedef enum {
    BP_ARG_NONE = 0,
    BP_ARG_REQUIRED = 1,
    BP_ARG_OPTIONAL = 2
} bp_arg_type_t;
```

| Value | Meaning |
|-------|---------|
| `BP_ARG_NONE` | Boolean flag, no argument consumed |
| `BP_ARG_REQUIRED` | Flag requires an argument value |
| `BP_ARG_OPTIONAL` | Argument value is optional |

## `bp_val_type_t` — Constraint Value Type Tag

Selects which union member is active inside a `bp_val_constraint_t`.

```c
typedef enum {
    BP_VAL_NONE = 0,
    BP_VAL_UINT32,
    BP_VAL_INT32,
    BP_VAL_FLOAT,
    BP_VAL_CHOICE,
} bp_val_type_t;
```

| Value | Union member | Description |
|-------|-------------|-------------|
| `BP_VAL_NONE` | — | No constraint (skip validation) |
| `BP_VAL_UINT32` | `.u` | Unsigned 32-bit integer range |
| `BP_VAL_INT32` | `.i` | Signed 32-bit integer range |
| `BP_VAL_FLOAT` | `.f` | Floating-point range |
| `BP_VAL_CHOICE` | `.choice` | Named choice from a fixed set |

## `bp_val_choice_t` — Named Choice Entry

Maps a name string (and optional short alias) to an integer value. Used for command-line parsing and interactive numbered menus.

```c
typedef struct {
    const char *name;
    const char *alias;
    uint32_t    label;
    uint32_t    value;
} bp_val_choice_t;
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | `const char *` | Full name for CLI match: `"even"`, `"odd"`, `"none"` |
| `alias` | `const char *` | Short alias: `"e"`, `"o"`, `"n"` (`NULL` = none) |
| `label` | `uint32_t` | `T_` translation key for interactive menu display |
| `value` | `uint32_t` | Actual stored value written to `*out` |

## `bp_val_constraint_t` — Value Constraint

Drives range checking, interactive prompting, and default values. Attach via pointer to a `bp_command_opt_t` or `bp_command_positional_t`. `NULL` pointer means no validation.

```c
typedef struct {
    bp_val_type_t type;
    union {
        struct { uint32_t min, max, def; } u;
        struct { int32_t  min, max, def; } i;
        struct { float    min, max, def; } f;
        struct {
            const bp_val_choice_t *choices;
            uint32_t count;
            uint32_t def;
        } choice;
    };
    uint32_t prompt;
    uint32_t hint;
} bp_val_constraint_t;
```

| Field | Type | Description |
|-------|------|-------------|
| `type` | `bp_val_type_t` | Which union member is active |
| `u.min`, `u.max`, `u.def` | `uint32_t` | Range and default for `BP_VAL_UINT32` |
| `i.min`, `i.max`, `i.def` | `int32_t` | Range and default for `BP_VAL_INT32` |
| `f.min`, `f.max`, `f.def` | `float` | Range and default for `BP_VAL_FLOAT` |
| `choice.choices` | `const bp_val_choice_t *` | Array of named choices for `BP_VAL_CHOICE` |
| `choice.count` | `uint32_t` | Number of choices |
| `choice.def` | `uint32_t` | Default value (matches a choice `.value`) |
| `prompt` | `uint32_t` | `T_` translation key for interactive prompt text (0 = none) |
| `hint` | `uint32_t` | `T_` translation key for hint text below prompt (0 = none) |

## `bp_command_opt_t` — Flag/Option Descriptor

Defines a single CLI flag for parsing, help display, and hinting. Arrays must be terminated with `{ 0 }`.

```c
typedef struct {
    const char *long_name;
    char        short_name;
    bp_arg_type_t arg_type;
    const char *arg_hint;
    uint32_t    description;
    const bp_val_constraint_t *constraint;
} bp_command_opt_t;
```

| Field | Type | Description |
|-------|------|-------------|
| `long_name` | `const char *` | Long option name (without `--`), `NULL` if none |
| `short_name` | `char` | Short option character, `0` if none |
| `arg_type` | `bp_arg_type_t` | `BP_ARG_NONE` / `REQUIRED` / `OPTIONAL` |
| `arg_hint` | `const char *` | Value placeholder for help (bare word, auto-wrapped with `<>`/`[]`), `NULL` if flag-only |
| `description` | `uint32_t` | `T_` translation key for help text |
| `constraint` | `const bp_val_constraint_t *` | Optional value constraint, `NULL` = no validation |

## `bp_command_positional_t` — Positional Argument Descriptor

Defines a non-flag argument. Order in the array matches position index (1-based in the API). Arrays must be terminated with `{ 0 }`.

```c
typedef struct {
    const char *name;
    const char *hint;
    uint32_t    description;
    bool        required;
    const bp_val_constraint_t *constraint;
} bp_command_positional_t;
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | `const char *` | Argument name for display: `"bank"`, `"voltage"` |
| `hint` | `const char *` | Value placeholder for hints (bare word, auto-wrapped), `NULL` = use name |
| `description` | `uint32_t` | `T_` translation key for help text |
| `required` | `bool` | `true` if argument is required |
| `constraint` | `const bp_val_constraint_t *` | Optional value constraint, `NULL` = no validation |

## `bp_command_action_t` — Action/Subcommand Verb

Maps a verb string to an enum value with help text.

```c
typedef struct {
    uint32_t    action;
    const char *verb;
    uint32_t    description;
} bp_command_action_t;
```

| Field | Type | Description |
|-------|------|-------------|
| `action` | `uint32_t` | Action enum value (start at 1; 0 = no action) |
| `verb` | `const char *` | Verb string: `"probe"`, `"dump"`, etc. |
| `description` | `uint32_t` | `T_` translation key for help text |

## `bp_action_delegate_t` — Dynamic Verb Source

Used when a command's verbs come from a runtime data structure (e.g. the `modes[]` array) instead of a static `actions` array.

```c
typedef struct {
    const char *(*verb_at)(uint32_t index);
    bool (*match)(const char *tok, size_t len, uint32_t *action_out);
    const struct bp_command_def *(*def_for_verb)(uint32_t action);
} bp_action_delegate_t;
```

| Field | Type | Description |
|-------|------|-------------|
| `verb_at` | function pointer | Return verb string at index, `NULL` = end of list |
| `match` | function pointer | Case-insensitive match of user token → action enum value |
| `def_for_verb` | function pointer | Return sub-definition for resolved verb, `NULL` = none |

## `bp_command_def_t` — Complete Command Definition

One per command. Drives parsing, help, hints, and tab-completion. All pointers reference `static const` data — zero allocation.

```c
typedef struct bp_command_def {
    const char *name;
    uint32_t    description;
    const bp_command_action_t *actions;
    uint32_t action_count;
    const bp_action_delegate_t *action_delegate;
    const bp_command_opt_t *opts;
    const bp_command_positional_t *positionals;
    uint32_t positional_count;
    const char *const *usage;
    uint32_t usage_count;
} bp_command_def_t;
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | `const char *` | Command name: `"flash"`, `"eeprom"` |
| `description` | `uint32_t` | `T_` translation key for top-level help |
| `actions` | `const bp_command_action_t *` | Subcommand verbs, `NULL` if none |
| `action_count` | `uint32_t` | Number of actions |
| `action_delegate` | `const bp_action_delegate_t *` | Dynamic verb source, `NULL` = use `actions` array |
| `opts` | `const bp_command_opt_t *` | Flags/options, `{ 0 }`-terminated |
| `positionals` | `const bp_command_positional_t *` | Positional args, `{ 0 }`-terminated, `NULL` if none |
| `positional_count` | `uint32_t` | Number of positional args |
| `usage` | `const char *const *` | Usage example strings |
| `usage_count` | `uint32_t` | Number of usage lines |

## `bp_cmd_status_t` — Return Codes

Result status from constraint-aware argument fetch functions (`bp_cmd_flag`, `bp_cmd_positional`, `bp_cmd_prompt`).

```c
typedef enum {
    BP_CMD_OK = 0,
    BP_CMD_MISSING,
    BP_CMD_INVALID,
    BP_CMD_EXIT,
} bp_cmd_status_t;
```

| Value | Meaning |
|-------|---------|
| `BP_CMD_OK` | Value obtained and valid |
| `BP_CMD_MISSING` | Not supplied on command line (default written if constraint exists) |
| `BP_CMD_INVALID` | Supplied but failed validation (error already printed) |
| `BP_CMD_EXIT` | User exited interactive prompt |

## Sentinel Convention

Both `bp_command_opt_t` and `bp_command_positional_t` arrays **must** be terminated with a `{ 0 }` sentinel entry. The parser walks the array until it hits a zero-initialized element.

```c
static const bp_command_opt_t my_opts[] = {
    { "verbose", 'v', BP_ARG_NONE, NULL, T_HELP_VERBOSE },
    { "count",   'c', BP_ARG_REQUIRED, "n", T_HELP_COUNT, &count_range },
    { 0 },  // ← sentinel — always required
};
```

## Lifetime Rules

All definition data must be `static const` (file-scope) or `const` with `extern` linkage. The `bp_command_def_t` and every array it points to must live for the entire program lifetime. The parser stores no copies — it reads pointers directly.

- Constraints: `static const bp_val_constraint_t` next to the opt/positional tables
- Option arrays: `static const bp_command_opt_t[]` in the `.c` file
- Command def: `const bp_command_def_t` (non-static) if exported via header

## Usage Examples

### Global Command — `dummy.c`

From `src/commands/global/dummy.c` — a global command with actions, a boolean flag, a constrained integer flag, and a string flag:

```c
static const bp_command_opt_t dummy_opts[] = {
    { "button",  'b', BP_ARG_NONE,     NULL,    T_HELP_DUMMY_B_FLAG },
    { "integer", 'i', BP_ARG_REQUIRED, "value", T_HELP_DUMMY_I_FLAG, &integer_range },
    { "file",    'f', BP_ARG_REQUIRED, "file",  T_HELP_DUMMY_FILE_FLAG },
    { 0 },
};

const bp_command_def_t dummy_def = {
    .name = "dummy",
    .description = 0x00,
    .actions = dummy_action_defs,
    .action_count = count_of(dummy_action_defs),
    .opts = dummy_opts,
    .usage = usage,
    .usage_count = count_of(usage),
};
```

### Mode Setup — `dummy1.c`

From `src/mode/dummy1.c` — a mode setup definition with a uint32 range constraint and a named-choice constraint:

```c
static const bp_val_constraint_t dummy1_speed_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 1000, .def = 100 },
    .prompt = 0,
    .hint = 0,
};

static const bp_val_choice_t dummy1_output_choices[] = {
    { "push-pull",  "pp", 0, 0 },
    { "open-drain", "od", 0, 1 },
};
static const bp_val_constraint_t dummy1_output_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = dummy1_output_choices, .count = 2, .def = 0 },
    .prompt = 0,
};

static const bp_command_opt_t dummy1_setup_opts[] = {
    { "speed",  's', BP_ARG_REQUIRED, "1-1000",               0, &dummy1_speed_range },
    { "output", 'o', BP_ARG_REQUIRED, "push-pull/open-drain", 0, &dummy1_output_choice },
    { 0 },
};

const bp_command_def_t dummy1_setup_def = {
    .name = "dummy1",
    .description = 0,
    .opts = dummy1_setup_opts,
};
```

## Related Documentation

- [new_command_guide.md](new_command_guide.md) — step-by-step guide to creating a new command
- [new_mode_guide.md](new_mode_guide.md) — step-by-step guide to creating a new protocol mode
- [args_parse_migration.md](args_parse_migration.md) — migrating from legacy argument parsing to `bp_cmd`
