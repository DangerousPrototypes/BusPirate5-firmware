# `bp_cmd` Developer Documentation — Outline

> Unified command definition, parsing, validation, prompting, help, hints, and completion system.

---

## 1. Introduction & Motivation

- **Problem**: Three separate legacy systems for three concerns
  - `ui_help_show()` + `ui_help_options[]` — help display
  - `cmdln_args_*()` / `ui_cmdln.h` — command-line flag/positional parsing
  - `ui_prompt_uint32()` + `struct ui_prompt` — interactive menus and validation
  - (plus a fourth partial layer: `bp_args_t` / `bp_args_compat` — never finished)
- **Solution**: One `bp_command_def_t` struct drives all five concerns:
  1. Help display
  2. Command-line parsing (flags, positionals, actions)
  3. Value validation (range, choice)
  4. Interactive prompting (fallback when args missing)
  5. Linenoise hints and tab-completion
- **Design principles**: Zero allocation, stateless re-scan, static const data, backward-compatible opt-in

---

## 2. Architecture Overview

- **Single source of truth**: `bp_command_def_t` — one per command (or per enable/disable pair)
- **Registration**:
  - Global commands: `commands[]` in `commands.c` — `.def = &my_def`
  - Mode commands: per-mode `hwxxx_commands[]` — `.def = &my_def`
  - Mode setup: `modes[].setup_def` — wired in `modes.c`
- **Files**:
  - `src/lib/bp_args/bp_cmd.h` — all public types and function declarations
  - `src/lib/bp_args/bp_cmd.c` — parsing, validation, prompting, help, hints, completion
  - `src/lib/bp_args/bp_cmd_linenoise.c` — linenoise glue (callback wiring)
  - `src/lib/bp_args/bp_cmd_linenoise.h` — init function declaration

---

## 3. Data Types Reference

### 3.1 `bp_command_def_t` — The Command Definition
- Fields: `name`, `description`, `actions`/`action_count`/`action_delegate`, `opts`, `positionals`/`positional_count`, `usage`/`usage_count`
- Lifetime: always `static const` (or file-scope `const` if `extern`)
- Sentinel conventions: opts and positionals arrays terminated with `{ 0 }`

### 3.2 `bp_command_opt_t` — Flag/Option Descriptor
- Fields: `long_name`, `short_name`, `arg_type`, `arg_hint`, `description`, `constraint`
- `bp_arg_type_t`: `BP_ARG_NONE`, `BP_ARG_REQUIRED`, `BP_ARG_OPTIONAL`
- Relationship to constraint: NULL constraint = no auto-validation

### 3.3 `bp_command_positional_t` — Positional Argument Descriptor
- Fields: `name`, `hint`, `description`, `required`, `constraint`
- 1-based indexing in API (`pos=1` is first user arg)

### 3.4 `bp_val_constraint_t` — Value Constraint
- Type tag: `BP_VAL_UINT32`, `BP_VAL_INT32`, `BP_VAL_FLOAT`, `BP_VAL_CHOICE`, `BP_VAL_NONE`
- Union members: `.u` (uint32), `.i` (int32), `.f` (float), `.choice`
- Fields: `.prompt` (T_ key for interactive prompt text), `.hint` (T_ key for hint text below prompt)
- Used by: `bp_cmd_positional()`, `bp_cmd_flag()`, `bp_cmd_prompt()`, help display

### 3.5 `bp_val_choice_t` — Named Choice Entry
- Fields: `name` (cmdline string), `alias` (short alias), `label` (T_ key for menu display), `value` (stored integer)
- Used for both CLI name matching and interactive numbered menus

### 3.6 `bp_command_action_t` — Action/Subcommand Verb
- Fields: `action` (enum value), `verb` (string), `description` (T_ key)
- Static array of verbs for commands with subcommands (e.g. `flash probe`, `flash dump`)

### 3.7 `bp_action_delegate_t` — Dynamic Verb Source
- Three function pointers: `verb_at()`, `match()`, `def_for_verb()`
- When verbs come from a runtime data structure (e.g. `modes[]` array for `m uart`, `m spi`)
- Drives: action resolution, help display, hint generation, tab-completion
- Sub-definition support: `def_for_verb()` returns the resolved verb's own `bp_command_def_t` for contextual flag hinting (e.g. `m uart -b` hints UART flags)

### 3.8 `bp_cmd_status_t` — Constraint-Aware Return Code
- `BP_CMD_OK` — value obtained and valid
- `BP_CMD_MISSING` — not on command line (default written for flags)
- `BP_CMD_INVALID` — present but failed validation (error already printed)
- `BP_CMD_EXIT` — user cancelled interactive prompt

---

## 4. Parsing API

### 4.1 Action/Subcommand Resolution
- `bp_cmd_get_action(def, &action)` — match first non-flag token against actions array or delegate

### 4.2 Flag Query (Simple)
- `bp_cmd_find_flag(def, 'e')` — boolean presence check
- `bp_cmd_get_uint32(def, 'n', &val)` — parse flag value as uint32
- `bp_cmd_get_int32(def, 'n', &val)` — parse as int32
- `bp_cmd_get_float(def, 'f', &val)` — parse as float
- `bp_cmd_get_string(def, 'f', buf, maxlen)` — copy flag value as string
- Stateless: re-scans command line buffer each call
- Supports `-f value`, `-f=value`, `--long value`, `--long=value`

### 4.3 Positional Query (Simple)
- `bp_cmd_get_positional_string(def, pos, buf, maxlen)`
- `bp_cmd_get_positional_uint32(def, pos, &val)`
- `bp_cmd_get_positional_int32(def, pos, &val)`
- `bp_cmd_get_positional_float(def, pos, &val)`
- Position 0 = command name, 1 = first user argument
- Flags and their consumed values are skipped automatically using the def

### 4.4 Remainder Access
- `bp_cmd_get_remainder(def, &ptr, &len)` — raw pointer to everything after command name

### 4.5 Constraint-Aware Resolution
- `bp_cmd_positional(def, pos, &out)` → `bp_cmd_status_t`
  - Selects parser from constraint type tag, range-checks, prints error if invalid
  - Returns `BP_CMD_MISSING` if not present (does NOT prompt)
- `bp_cmd_flag(def, 'u', &out)` → `bp_cmd_status_t`
  - Same as above for flags
  - Writes default value to `out` when flag is absent (`BP_CMD_MISSING`)

---

## 5. Interactive Prompting

### 5.1 `bp_cmd_prompt(constraint, &out)`
- Drives an interactive prompt loop from a `bp_val_constraint_t`
- Prints prompt text (from `.prompt` T_ key), shows range/choices, displays default
- For `BP_VAL_CHOICE`: displays numbered menu from `bp_val_choice_t` entries, accepts name/alias/number
- For numeric types: accepts bare number, validates range, loops on error
- Returns `BP_CMD_OK` or `BP_CMD_EXIT`

### 5.2 Dual-Path Pattern (CLI vs Interactive)
- Check "primary" argument: `bp_cmd_positional()` or `bp_cmd_flag()`
- If `BP_CMD_MISSING` → interactive mode: call `bp_cmd_prompt()` for each parameter
- If `BP_CMD_OK` → CLI mode: remaining params use defaults or are fetched via `bp_cmd_flag()`
- If `BP_CMD_INVALID` → bail with error
- Reference: `w_psu.c` (command), `hwuart.c` (mode setup)

### 5.3 Saved Configuration Integration
- `storage_load_mode()` / `storage_save_mode()` pattern preserved in interactive path
- In CLI path: skip saved-config check, use provided flags directly

---

## 6. Help System

### 6.1 `bp_cmd_help_check(def, help_flag)`
- If help_flag is true, displays help and returns true (caller should `return`)
- Drop-in replacement for `ui_help_show()`

### 6.2 `bp_cmd_help_show(def)`
- Unconditional help display
- Renders from the def: name, description, usage examples, flags table, positionals table, actions list

### 6.3 Help Display Format
- Usage examples with formatting placeholders (`%s`)
- Flag table: `-short, --long <hint> description`
- Positional args listed
- Action verbs listed with descriptions
- Contextual help: `m uart -h` shows UART setup flags (via `def_for_verb()`)

---

## 7. Linenoise Integration (Hints & Completion)

### 7.1 Hint Generation — `bp_cmd_hint()`
- Called on every keystroke via linenoise callback
- Walks registered defs to find matching command
- Ghost text suggestions:
  - After command name: suggests action verbs or first flag
  - After action verb: suggests flags from sub-definition (via `def_for_verb()`)
  - After partial flag: suggests matching flag name
  - After flag that requires value: suggests `<arg_hint>` placeholder
- Appearance: configurable color/bold in `bp_cmd_linenoise.c`

### 7.2 Tab-Completion — `bp_cmd_completion()`
- Called on Tab press via linenoise callback
- Completes: command names, action verbs, flag names (both `-short` and `--long`)
- Sub-definition aware: `m uart -<Tab>` completes UART flags

### 7.3 Linenoise Glue — `bp_cmd_linenoise.c`
- `bp_cmd_linenoise_init()` — wires hint/completion callbacks at startup
- `collect_defs()` — dynamically builds flat array of all registered `bp_command_def_t*` from global `commands[]` + current mode's `mode_commands[]`
- Entries with `.def = NULL` (legacy commands) silently skipped

---

## 8. Registration & Wiring

### 8.1 Global Commands (`commands.c`)
- `struct _global_command_struct`: `.command`, `.func`, `.def`, `.description_text`, `.allow_hiz`, `.category`
- Set `.def = &my_def` to enable help/hints/completion for a command
- `.description_text` is legacy — to be deprecated in favor of `.def->description`

### 8.2 Mode Commands
- `struct _mode_command_struct`: `.func`, `.def`, `.supress_fala_capture`
- Same pattern: set `.def = &my_def`

### 8.3 Mode Setup Definitions
- `modes[].setup_def` — pointer to mode's setup `bp_command_def_t`
- Enables `m uart -h` contextual help and `m uart -b <Tab>` flag completion
- Wired in `modes.c` via `.setup_def = &uart_setup_def`

### 8.4 Categories (`enum cmd_category`)
- `CMD_CAT_IO`, `CMD_CAT_CONFIGURE`, `CMD_CAT_SYSTEM`, `CMD_CAT_FILES`, `CMD_CAT_SCRIPT`, `CMD_CAT_TOOLS`, `CMD_CAT_MODE`, `CMD_CAT_HIDDEN`
- Used by `h` command to group help output

---

## 9. Patterns & Recipes

### 9.1 Simple Command (Help + One Flag)
- Example: `monitor.c` — `bp_cmd_help_check()` + `bp_cmd_find_flag()`
- Minimal def: name, description, opts, usage

### 9.2 Command with Positionals
- Example: `w_psu.c` — voltage + current positionals with interactive fallback
- Pattern: try `bp_cmd_positional()`, if MISSING → `bp_cmd_prompt()`

### 9.3 Command with Actions/Subcommands
- Example: `flash.c` — probe/dump/erase/write/read/verify/test
- Pattern: `bp_cmd_get_action()` → switch on action enum

### 9.4 Command with Dynamic Verbs (Delegate)
- Example: `m` command (`ui_mode.c`) — verbs from `modes[]` array
- Pattern: `bp_action_delegate_t` with `verb_at()`, `match()`, `def_for_verb()`
- Enables contextual sub-definition hinting

### 9.5 Mode Setup (Dual-Path: Interactive Wizard + CLI Flags)
- Example: `hwuart.c` — baud, databits, parity, stopbits, flow, inversion
- Constraint types: `BP_VAL_UINT32` for ranges, `BP_VAL_CHOICE` for named options
- Flag table drives both `-b 115200` and interactive `bp_cmd_prompt()` wizard
- Saved-config check in interactive path

### 9.6 Pin Selection with Hardware Validation
- Example: `freq.c`, `pwm.c` — pin menus filtered by hardware state
- Pattern: `BP_VAL_UINT32` range constraint (0-7) + post-prompt hardware validation
- Print available pins manually before `bp_cmd_prompt()`

### 9.7 Enable/Disable Command Pairs
- Example: `W`/`w` (PSU), `G`/`g` (PWM), `P`/`p` (pullups), `F`/`f` (freq)
- Pattern: separate `bp_command_def_t` per handler, shared usage strings

---

## 10. Migration Guide

### 10.1 What's Being Replaced
| Old API | New Replacement |
|---------|-----------------|
| `ui_help_show()` + `ui_help_options[]` | `bp_cmd_help_check()` / `bp_cmd_help_show()` |
| `cmdln_args_find_flag()` | `bp_cmd_find_flag()` |
| `cmdln_args_uint32_by_position()` | `bp_cmd_get_positional_uint32()` or `bp_cmd_positional()` |
| `cmdln_args_string_by_position()` | `bp_cmd_get_positional_string()` |
| `cmdln_args_find_flag_string()` | `bp_cmd_get_string()` |
| `cmdln_args_find_flag_uint32()` | `bp_cmd_get_uint32()` or `bp_cmd_flag()` |
| `cmdln_args_get_action()` | `bp_cmd_get_action()` |
| `ui_prompt_uint32()` + `struct ui_prompt` | `bp_cmd_prompt()` + `bp_val_constraint_t` |
| `struct prompt_item` / `prompt_int_cfg` / `prompt_list_cfg` | `bp_val_constraint_t` / `bp_val_choice_t` |
| `bp_args_t` / `bp_args_compat_*()` | `bp_cmd_*()` (direct replacement) |

### 10.2 Step-by-Step Migration Checklist
1. Create `bp_val_constraint_t` for each validated parameter
2. Create `bp_command_opt_t[]` array (sentinel-terminated)
3. Create `bp_command_positional_t[]` if applicable
4. Create usage strings
5. Create `bp_command_def_t`
6. Rewrite handler: `bp_cmd_help_check()` → parsing → prompting
7. Wire `.def` in registration array
8. Add `extern` in header if needed
9. Remove dead old code / old includes
10. Build and verify

### 10.3 Migration Status
- Link to: `docs/bp_cmd_migration_prompt.md` — per-command migration prompt
- Link to: `docs/mode_setup_migration_prompt.md` — per-mode setup migration prompt
- Link to: `docs/command_setup_migration_prompt.md` — remaining commands migration prompt

---

## 11. What's NOT Replaced

- `ui_prompt_bool()` — confirmation prompts (yes/no). Still used, no `bp_cmd` equivalent needed.
- `ui_prompt_float_units()` — unit-aware float input (e.g. `100kHz`, `50%`). Complex, kept as-is.
- `ui_help_check_vout_vref()` — safety check utility function, not a help function.
- `ui_prompt_mode_settings_int()` / `ui_prompt_mode_settings_string()` — settings display functions.
- `c` config command — special case, not migrated.
- Translation files — never modified by migration. Use `0` placeholder for new T_ keys.

---

## 12. API Reference (Quick)

### Parsing
| Function | Signature | Returns |
|----------|-----------|---------|
| `bp_cmd_get_action` | `(def, &action)` | `bool` |
| `bp_cmd_find_flag` | `(def, char)` | `bool` |
| `bp_cmd_get_uint32` | `(def, char, &val)` | `bool` |
| `bp_cmd_get_int32` | `(def, char, &val)` | `bool` |
| `bp_cmd_get_float` | `(def, char, &val)` | `bool` |
| `bp_cmd_get_string` | `(def, char, buf, maxlen)` | `bool` |
| `bp_cmd_get_positional_string` | `(def, pos, buf, maxlen)` | `bool` |
| `bp_cmd_get_positional_uint32` | `(def, pos, &val)` | `bool` |
| `bp_cmd_get_positional_int32` | `(def, pos, &val)` | `bool` |
| `bp_cmd_get_positional_float` | `(def, pos, &val)` | `bool` |
| `bp_cmd_get_remainder` | `(def, &ptr, &len)` | `bool` |

### Constraint-Aware
| Function | Signature | Returns |
|----------|-----------|---------|
| `bp_cmd_positional` | `(def, pos, &out)` | `bp_cmd_status_t` |
| `bp_cmd_flag` | `(def, char, &out)` | `bp_cmd_status_t` |
| `bp_cmd_prompt` | `(constraint, &out)` | `bp_cmd_status_t` |

### Help
| Function | Signature | Returns |
|----------|-----------|---------|
| `bp_cmd_help_check` | `(def, help_flag)` | `bool` |
| `bp_cmd_help_show` | `(def)` | `void` |

### Hints & Completion
| Function | Signature | Returns |
|----------|-----------|---------|
| `bp_cmd_hint` | `(buf, len, defs, count)` | `const char*` |
| `bp_cmd_completion` | `(buf, len, defs, count, callback, userdata)` | `void` |
| `bp_cmd_linenoise_init` | `(void)` | `void` |
