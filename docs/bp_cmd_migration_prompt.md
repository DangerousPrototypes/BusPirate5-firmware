# bp_command_def_t Migration Prompt

> Give this entire document to the LLM along with the target command file.

---

## Task

Migrate a single Bus Pirate 5 firmware command from the legacy `ui_help`/`ui_cmdln` system to the new `bp_command_def_t` system. This is a **three-file touch** operation:

1. **Command file** (e.g. `src/commands/spi/flash.c`) — rewrite help/arg tables, update handler
2. **Command header** (e.g. `src/commands/spi/flash.h`) — add `extern` for the def struct
3. **Registration array** (e.g. `src/mode/hwspi.c` or `src/commands.c`) — wire `.def = &xxx_def`

---

## Architecture Context

The firmware has two command registration arrays:

- **Global commands** — `commands[]` in `src/commands.c`, each entry is `struct _global_command_struct`
- **Mode commands** — per-mode arrays like `hwspi_commands[]` in `src/mode/hwspi.c`, each entry is `struct _mode_command_struct`

Both structs have a `.def` field (type `const struct bp_command_def *`) that is `NULL` for unmigrated commands. Your job is to populate it.

---

## Reference: Completed Migration (flash.c)

This is the fully migrated `flash` command. Study it as the canonical pattern.

### File 1 of 3: `src/commands/spi/flash.c` (command implementation)

```c
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "command_struct.h"
/* ... other domain includes ... */
#include "ui/ui_help.h"
#include "ui/ui_prompt.h"
#include "lib/bp_args/bp_cmd.h"    // <-- NEW: replaces ui/ui_cmdln.h

static const char* const usage[] = {
    "flash [probe|dump|erase|write|read|verify|test]\r\n\t[-f <file>] [-e(rase)] [-v(verify)] [-h(elp)]",
    "Initialize and probe:%s flash probe",
    "Show flash contents (x to exit):%s flash dump",
    /* ... more usage lines ... */
};

enum flash_actions {
    FLASH_PROBE = 0,
    FLASH_DUMP,
    FLASH_ERASE,
    FLASH_WRITE,
    FLASH_READ,
    FLASH_VERIFY,
    FLASH_TEST
};

static const bp_command_action_t flash_action_defs[] = {
    { FLASH_PROBE,  "probe",  T_HELP_FLASH_PROBE },
    { FLASH_DUMP,   "dump",   T_HELP_EEPROM_DUMP },
    { FLASH_ERASE,  "erase",  T_HELP_FLASH_ERASE },
    { FLASH_WRITE,  "write",  T_HELP_FLASH_WRITE },
    { FLASH_READ,   "read",   T_HELP_FLASH_READ },
    { FLASH_VERIFY, "verify", T_HELP_FLASH_VERIFY },
    { FLASH_TEST,   "test",   T_HELP_FLASH_TEST },
};

static const bp_command_opt_t flash_opts[] = {
    { "file",     'f', BP_ARG_REQUIRED, "<file>",    T_HELP_FLASH_FILE_FLAG },
    { "erase",    'e', BP_ARG_NONE,     NULL,        T_HELP_FLASH_ERASE_FLAG },
    { "verify",   'v', BP_ARG_NONE,     NULL,        T_HELP_FLASH_VERIFY_FLAG },
    { "start",    's', BP_ARG_REQUIRED, "<addr>",    UI_HEX_HELP_START },
    { "bytes",    'b', BP_ARG_REQUIRED, "<count>",   UI_HEX_HELP_BYTES },
    { "quiet",    'q', BP_ARG_NONE,     NULL,        UI_HEX_HELP_QUIET },
    { "nopager",  'c', BP_ARG_NONE,     NULL,        T_HELP_DISK_HEX_PAGER_OFF },
    { "override", 'o', BP_ARG_NONE,     NULL,        T_HELP_FLASH_OVERRIDE },
    { "yes",      'y', BP_ARG_NONE,     NULL,        T_HELP_FLASH_YES_OVERRIDE },
    { 0 }    // <-- SENTINEL: always terminate with { 0 }
};

// NOT static — will be extern'd by the header
const bp_command_def_t flash_def = {
    .name         = "flash",
    .description  = T_HELP_FLASH,
    .actions      = flash_action_defs,
    .action_count = count_of(flash_action_defs),
    .opts         = flash_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void flash(struct command_result* res) {
    // Help check — replaces ui_help_show()
    if (bp_cmd_help_check(&flash_def, res->help_flag)) {
        return;
    }

    // Action verb — replaces cmdln_args_get_action()
    uint32_t flash_action = 0;
    if (!bp_cmd_get_action(&flash_def, &flash_action)) {
        bp_cmd_help_show(&flash_def);
        return;
    }

    // Boolean flags — replaces cmdln_args_find_flag()
    bool erase_flag  = bp_cmd_find_flag(&flash_def, 'e');
    bool verify_flag = bp_cmd_find_flag(&flash_def, 'v');

    // String flag value — replaces cmdln_args_find_flag_string()
    char file[13];
    bool file_flag = bp_cmd_get_string(&flash_def, 'f', file, sizeof(file));

    // uint32 flag value — replaces cmdln_args_find_flag_uint32()
    uint32_t end_address;
    bool has_bytes = bp_cmd_get_uint32(&flash_def, 'b', &end_address);

    /* ... rest of handler logic unchanged ... */
}
```

### File 2 of 3: `src/commands/spi/flash.h` (header — add extern)

```c
void flash(struct command_result* res);

// ADD THIS LINE:
extern const bp_command_def_t flash_def;
```

The `bp_command_def_t` typedef is defined in `lib/bp_args/bp_cmd.h`. The header that includes `flash.h` must have `bp_cmd.h` included (or use the forward declaration `struct bp_command_def` from `command_struct.h`). In practice, the registration file (File 3) already includes `command_struct.h` which forward-declares the struct, and the linker resolves the rest. So the extern in the header can use either form:

```c
// Option A — uses the typedef (requires bp_cmd.h to be included before this header)
extern const bp_command_def_t flash_def;

// Option B — uses the forward-declared struct name (works with just command_struct.h)
extern const struct bp_command_def flash_def;
```

**Use Option B** unless the header already includes `bp_cmd.h`. Option B is preferred because it avoids adding a new include dependency to the header file.

### File 3 of 3: `src/mode/hwspi.c` (registration — wire .def)

```c
#include "commands/spi/flash.h"   // already included

const struct _mode_command_struct hwspi_commands[] = {
    {   .command="flash",
        .func=&flash,
        .def=&flash_def,              // <-- ADD THIS
        .description_text=T_HELP_CMD_FLASH,
        .supress_fala_capture=true
    },
    /* ... other commands ... */
};
```

For **global commands** the registration file is `src/commands.c` and the struct is `_global_command_struct`:

```c
{   .command="somecommand",
    .func=&some_handler,
    .def=&some_def,               // <-- ADD THIS
    .description_text=T_SOME_KEY,
    .allow_hiz=true,
    .category=CMD_CAT_TOOLS,
},
```

---

## Step-by-Step Migration Procedure

### Step 1: Identify the legacy patterns in the command file

Look for these OLD patterns that must be replaced:

```c
// OLD include:
#include "ui/ui_cmdln.h"

// OLD action table:
static const struct cmdln_action_t xxx_actions[] = {
    { ACTION_ENUM, "verb" },
    ...
};

// OLD help options:
static const struct ui_help_options options[] = {
    { 1, "", T_HELP_DESCRIPTION },      // section header (help=1)
    { 0, "verb", T_HELP_VERB_DESC },    // action verb (help=0)
    { 0, "-f", T_HELP_FLAG_DESC },      // flag (help=0)
    ...
};

// OLD help check in handler:
if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
    return;
}

// OLD action dispatch:
uint32_t action;
if (cmdln_args_get_action(xxx_actions, count_of(xxx_actions), &action)) {  // NOTE: returns true on FAILURE
    // show help
    return;
}

// OLD argument parsing:
cmdln_args_find_flag('v')                              // boolean flag
cmdln_args_find_flag_string('f', &arg, sizeof(buf), buf)  // flag with string
cmdln_args_find_flag_uint32('u', &arg, &value)         // flag with uint32
cmdln_args_string_by_position(1, sizeof(buf), buf)     // positional string
cmdln_args_float_by_position(1, &value)                // positional float
```

### Step 2: Create the new bp_command_def_t tables

Replace the old tables with the new ones:

| Old construct | New construct |
|---|---|
| `#include "ui/ui_cmdln.h"` | `#include "lib/bp_args/bp_cmd.h"` |
| `struct cmdln_action_t actions[]` | `bp_command_action_t action_defs[]` with 3 fields: `{ enum_val, "verb", T_HELP_KEY }` |
| `struct ui_help_options options[]` | `bp_command_opt_t opts[]` with 5 fields, `{ 0 }` sentinel |
| `usage[]` stays | Keep the `usage[]` array as-is, no changes needed |

**Action table conversion:**
```c
// OLD:
static const struct cmdln_action_t eeprom_actions[] = {
    { EEPROM_DUMP, "dump" },
    { EEPROM_WRITE, "write" },
};

// NEW — add description (3rd field, a translation key from the OLD options[]):
static const bp_command_action_t eeprom_action_defs[] = {
    { EEPROM_DUMP,  "dump",  T_HELP_EEPROM_DUMP },
    { EEPROM_WRITE, "write", T_HELP_EEPROM_WRITE },
};
```

**Options/flags conversion:**

Extract flag entries from the OLD `ui_help_options[]` and convert them. Ignore entries with `help=1` (section headers) — those are not needed. Ignore entries that describe action verbs (those go in the action table). Only convert flag entries (those starting with `-`):

```c
// OLD options[] entry:
{ 0, "-f", T_HELP_EEPROM_FILE_FLAG },
{ 0, "-v", T_HELP_EEPROM_VERIFY_FLAG },

// NEW opts[] entries:
{ "file",   'f', BP_ARG_REQUIRED, "<file>", T_HELP_EEPROM_FILE_FLAG },
{ "verify", 'v', BP_ARG_NONE,     NULL,     T_HELP_EEPROM_VERIFY_FLAG },
{ 0 }   // sentinel
```

Rules for the 5 fields:
1. **`long_name`** — descriptive name for `--long` style (pick a sensible word)
2. **`short_name`** — the flag character (same as the old `-x` flag)
3. **`arg_type`** — `BP_ARG_NONE` for boolean flags, `BP_ARG_REQUIRED` if the flag takes a value
4. **`arg_hint`** — placeholder string for help display (e.g. `"<file>"`, `"<addr>"`), `NULL` for boolean flags
5. **`description`** — translation key (reuse the same key from the old `options[]`)

**Determine arg_type by examining usage in the handler:**
- If the flag is checked with `cmdln_args_find_flag('x')` → `BP_ARG_NONE`
- If the flag is checked with `cmdln_args_find_flag_string('x', ...)` or `cmdln_args_find_flag_uint32('x', ...)` → `BP_ARG_REQUIRED`

**The def struct:**
```c
// Remove 'static' so it can be extern'd
const bp_command_def_t xxx_def = {
    .name         = "commandname",
    .description  = T_HELP_COMMAND_DESCRIPTION,   // the T_ key from the old options[] header entry (help=1)
    .actions      = xxx_action_defs,               // NULL if no subcommands
    .action_count = count_of(xxx_action_defs),     // 0 if no subcommands
    .opts         = xxx_opts,                      // NULL if no flags
    .usage        = usage,
    .usage_count  = count_of(usage),
};
```

### Step 3: Update the handler function

Replace every OLD call with its NEW equivalent:

| Old call | New call |
|---|---|
| `ui_help_show(res->help_flag, usage, count, &options[0], count)` | `bp_cmd_help_check(&xxx_def, res->help_flag)` |
| `cmdln_args_get_action(actions, count, &val)` (returns true=fail) | `bp_cmd_get_action(&xxx_def, &val)` (returns true=success) |
| `cmdln_args_find_flag('v')` | `bp_cmd_find_flag(&xxx_def, 'v')` |
| `cmdln_args_find_flag_uint32('b', &arg, &val)` | `bp_cmd_get_uint32(&xxx_def, 'b', &val)` |
| `cmdln_args_find_flag_string('f', &arg, size, buf)` | `bp_cmd_get_string(&xxx_def, 'f', buf, size)` |
| `cmdln_args_string_by_position(N, size, buf)` | `bp_cmd_get_positional_string(&xxx_def, N, buf, size)` |
| `cmdln_args_float_by_position(N, &val)` | `bp_cmd_get_positional_float(&xxx_def, N, &val)` |

**CRITICAL: Return value inversion for actions!**
The OLD `cmdln_args_get_action()` returns `true` on **failure** (no match).
The NEW `bp_cmd_get_action()` returns `true` on **success** (match found).

```c
// OLD pattern (true = failure):
if (cmdln_args_get_action(actions, count_of(actions), &action)) {
    printf("error\r\n");
    return;
}

// NEW pattern (true = success, so negate):
if (!bp_cmd_get_action(&xxx_def, &action)) {
    bp_cmd_help_show(&xxx_def);
    return;
}
```

**CRITICAL: The `command_var_t` arg struct is no longer needed.**
The old API required passing a `command_var_t` struct. The new API does not. Remove any `command_var_t` variable declarations that are no longer used.

### Step 4: Remove the old include

If `#include "ui/ui_cmdln.h"` is no longer needed (no other cmdln_args calls remain), remove it.

### Step 5: Delete the old tables

Delete the old `struct ui_help_options options[]` and `struct cmdln_action_t xxx_actions[]` arrays. Keep the `usage[]` array (it's reused by the new def).

### Step 6: Add extern to the header file

In the command's `.h` file, add:

```c
extern const struct bp_command_def xxx_def;
```

If the file doesn't have a header, create one with the function declaration and the extern.

### Step 7: Wire `.def` in the registration array

Find where the command is registered:
- **Global commands**: `src/commands.c` — find the entry in `commands[]` with matching `.command` string
- **Mode commands**: the mode file (e.g. `src/mode/hwspi.c`) — find the entry in the mode's command array

Add `.def = &xxx_def` to the entry. Example:

```c
// BEFORE:
{   .command="flash",
    .func=&flash,
    .description_text=T_HELP_CMD_FLASH,
    .supress_fala_capture=true
},

// AFTER:
{   .command="flash",
    .func=&flash,
    .def=&flash_def,
    .description_text=T_HELP_CMD_FLASH,
    .supress_fala_capture=true
},
```

### Step 8: Verify — Checklist

Before submitting, verify:

- [ ] `#include "lib/bp_args/bp_cmd.h"` added to the command `.c` file
- [ ] `#include "ui/ui_cmdln.h"` removed (if no other cmdln_args calls remain)
- [ ] Old `struct ui_help_options options[]` deleted
- [ ] Old `struct cmdln_action_t xxx_actions[]` deleted (if present)
- [ ] `usage[]` array kept (unchanged)
- [ ] New `bp_command_action_t` array created with 3-field entries (if command has subcommands)
- [ ] New `bp_command_opt_t` array created with `{ 0 }` sentinel (if command has flags)
- [ ] `const bp_command_def_t xxx_def = { ... };` is NOT `static`
- [ ] Handler uses `bp_cmd_help_check()` instead of `ui_help_show()`
- [ ] Handler uses `bp_cmd_get_action()` with correct return-value sense (true=success)
- [ ] All `cmdln_args_*` calls replaced with `bp_cmd_*` equivalents
- [ ] All `command_var_t` declarations removed if unused
- [ ] `extern const struct bp_command_def xxx_def;` added to the `.h` file
- [ ] `.def = &xxx_def` wired in the registration array
- [ ] No other files in the codebase reference the deleted old tables

---

## API Quick Reference

### Types

```c
typedef struct {
    const char *long_name;      // "--file" (without --)
    char        short_name;     // 'f'
    bp_arg_type_t arg_type;     // BP_ARG_NONE | BP_ARG_REQUIRED | BP_ARG_OPTIONAL
    const char *arg_hint;       // "<file>" or NULL
    uint32_t    description;    // T_HELP_* translation key
} bp_command_opt_t;

typedef struct {
    uint32_t    action;         // enum value
    const char *verb;           // "probe"
    uint32_t    description;    // T_HELP_* translation key
} bp_command_action_t;

typedef struct bp_command_def {
    const char *name;
    uint32_t    description;
    const bp_command_action_t *actions;
    uint32_t action_count;
    const bp_command_opt_t *opts;       // { 0 }-terminated
    const char *const *usage;
    uint32_t usage_count;
} bp_command_def_t;
```

### Functions

```c
// Help
bool bp_cmd_help_check(const bp_command_def_t *def, bool help_flag);  // returns true if help shown
void bp_cmd_help_show(const bp_command_def_t *def);                    // unconditional

// Actions
bool bp_cmd_get_action(const bp_command_def_t *def, uint32_t *action); // true = found

// Flags
bool bp_cmd_find_flag(const bp_command_def_t *def, char flag);                          // boolean
bool bp_cmd_get_uint32(const bp_command_def_t *def, char flag, uint32_t *value);        // -b 1024
bool bp_cmd_get_int32(const bp_command_def_t *def, char flag, int32_t *value);          // -o -5
bool bp_cmd_get_string(const bp_command_def_t *def, char flag, char *buf, size_t max);  // -f file.bin
bool bp_cmd_get_float(const bp_command_def_t *def, char flag, float *value);            // -v 3.3

// Positional (pos 0 = command name, pos 1 = first arg)
bool bp_cmd_get_positional_string(const bp_command_def_t *def, uint32_t pos, char *buf, size_t max);
bool bp_cmd_get_positional_uint32(const bp_command_def_t *def, uint32_t pos, uint32_t *value);
bool bp_cmd_get_positional_int32(const bp_command_def_t *def, uint32_t pos, int32_t *value);
bool bp_cmd_get_positional_float(const bp_command_def_t *def, uint32_t pos, float *value);

// Raw remainder after command name
bool bp_cmd_get_remainder(const bp_command_def_t *def, const char **out, size_t *len);
```

---

## Special Cases

### Commands with no subcommands (no actions)

Set `.actions = NULL` and `.action_count = 0`. Don't create an action table. Don't call `bp_cmd_get_action()` in the handler.

### Commands with no flags

Set `.opts = NULL`. Don't create an opts table.

### Commands that only use positional arguments

Some commands (like `w`/`W` for PSU) don't use flags at all — they use positional args (`W 3.3 100`). These still benefit from migration because they get proper `-h` help. Create the def with `.opts = NULL`, and use `bp_cmd_get_positional_*()` in the handler.

### Commands with uppercase/lowercase variants (w/W, p/P, etc.)

These are registered as separate entries in `commands[]` with different handler functions (e.g., `psucmd_disable_handler` for `w`, `psucmd_enable_handler` for `W`). They can share the same `bp_command_def_t` or have separate ones — use separate defs if they have different help text.

### Commands that use `ui_help_check_vout_vref()` or other sanity checks

Keep those calls as-is — they are independent of the help/arg system.

### Commands where `usage[]` is not `static`

Some commands declare `usage[]` without `static` (it's `const char* const usage[]`). Add `static` if possible, or leave it if other files reference it.

---

## Output Format

When you complete a migration, output the three file changes clearly labeled:

1. **File 1: `path/to/command.c`** — show the complete modified file OR the specific diff hunks
2. **File 2: `path/to/command.h`** — show the added extern line
3. **File 3: `path/to/registration.c`** — show the modified struct entry with `.def` added
