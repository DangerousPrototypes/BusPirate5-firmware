# Implementing a New Bus Pirate Command

> A step-by-step guide to creating a global or mode command for Bus Pirate 5/6/7 firmware.  
> Reference implementation: `src/commands/global/dummy.c`

---

## Overview

A **command** is a user-facing function invoked at the Bus Pirate prompt (e.g. `flash`, `dummy`, `W`). Commands come in two flavors:

| Type | Scope | Example |
|------|-------|---------|
| **Global** | Available in every mode (or restricted to non-HiZ) | `W` (PSU enable), `ls`, `dummy` |
| **Mode** | Available only when a specific mode is active | `flash` (SPI), `bridge` (UART) |

Both types share the same handler signature and the same `bp_cmd` definition system. The difference is where they are **registered** and a few structural fields.

### What a Command Provides

Every command consists of:

- **Definition** (`bp_command_def_t`) — single source of truth for help, parsing, hints, completion
- **Handler function** — entry point called by the dispatcher
- **Header file** — exports the def and handler (global) or command table (mode)
- **Registration** — entry in `commands[]` (global) or a mode's command table (mode)

### What `bp_cmd` Drives From One Definition

The `bp_command_def_t` struct drives **five** concerns from a single definition:

1. **Help display** — `dummy -h` shows usage, flags, and actions
2. **CLI parsing** — flags, positionals, and action verbs
3. **Value validation** — range and choice constraints
4. **Interactive prompting** — fallback wizard when args are missing
5. **Linenoise hints & tab-completion** — ghost text and `<Tab>` suggestions

---

## File Structure

### Global Command

| File | Purpose |
|------|---------|
| `src/commands/global/mycmd.c` | All command logic — def, handler, constraints |
| `src/commands/global/mycmd.h` | `extern` for def + handler declaration |
| `src/commands.c` | Registration in the `commands[]` dispatch table |

### Mode Command

| File | Purpose |
|------|---------|
| `src/commands/<mode>/mycmd.c` | All command logic — def, handler, constraints |
| `src/commands/<mode>/mycmd.h` | `extern` for def + handler declaration |
| `src/mode/hw<mode>.c` | Command table array (e.g. `hwuart_commands[]`) |
| `src/mode/hw<mode>.h` | `extern` for command table + count |
| `src/modes.c` | Wires the mode's command table via `.mode_commands` |

---

## Step 1: Includes

Start with the required headers. Annotate each include so future readers know what it provides:

```c
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "fatfs/ff.h"              // File system (FatFS)
#include "pirate/storage.h"        // File system helpers
#include "lib/bp_args/bp_cmd.h"    // Unified command parsing, validation, prompting, help, hints
#include "ui/ui_help.h"            // Help display utilities
#include "system_config.h"         // Current Bus Pirate system configuration
#include "pirate/amux.h"           // Analog voltage measurement functions
#include "pirate/button.h"         // Button press functions
```

The only **required** includes for a minimal command are `stdio.h`, `pirate.h`, `command_struct.h`, `system_config.h`, `lib/bp_args/bp_cmd.h`, and your own header. Add others as needed by your logic.

---

## Step 2: Usage Examples

Displayed when the user enters `mycmd -h`. The first entry is the synopsis; remaining entries are labeled examples:

```c
static const char* const usage[] = {
    "dummy [init|test]\r\n\t[-b(utton)] [-i(nteger) <value>] [-f <file>]",
    "Initialize:%s dummy init",
    "Test:%s dummy test",
    "Test, require button press:%s dummy test -b",
    "Integer, value required:%s dummy -i 123",
    "Interactive integer prompt:%s dummy init -b",
    "Create/write/read file:%s dummy -f dummy.txt",
    "Kitchen sink:%s dummy test -b -i 123 -f dummy.txt",
};
```

| Entry | Format | Notes |
|-------|--------|-------|
| First | Synopsis string | Use `\r\n\t` for line wrapping |
| Rest | `"Label:%s command example"` | `%s` is replaced with the Bus Pirate prompt name |

---

## Step 3: Actions / Subcommands (Optional)

If your command has verb-style subcommands (e.g. `flash probe`, `flash dump`), define an actions array. Actions are matched by `bp_cmd_get_action()` — this replaces manual `strcmp()` on positional arguments.

```c
enum dummy_actions {
    DUMMY_INIT = 1, // enum values should start at 1 (0 = no action)
    DUMMY_TEST = 2,
};

static const bp_command_action_t dummy_action_defs[] = {
    { DUMMY_INIT, "init", T_HELP_DUMMY_INIT },  // "dummy init" → action=DUMMY_INIT
    { DUMMY_TEST, "test", T_HELP_DUMMY_TEST },   // "dummy test" → action=DUMMY_TEST
};
```

Each `bp_command_action_t` entry:

| Field | Purpose |
|-------|---------|
| `.action` | Enum value returned by `bp_cmd_get_action()` (start at 1) |
| `.verb` | String the user types after the command name |
| `.description` | T_ translation key for help display (`0` = placeholder) |

If your command does **not** have subcommands, skip this and set `.actions = NULL`, `.action_count = 0` in the def.

---

## Step 4: Value Constraints

Each validated parameter gets a `bp_val_constraint_t` that defines its type, valid range (or choices), default value, and prompt text. Constraints power three things simultaneously:

- `bp_cmd_flag()` — validates the CLI value against the range
- `bp_cmd_prompt()` — drives an interactive prompt with range display
- Help display — shows valid range in the help output

**Integer range** (`BP_VAL_UINT32`):

```c
static const bp_val_constraint_t integer_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 0, .max = 65535, .def = 0 },
    .prompt = 0, // T_ key for interactive prompt title (0 = placeholder)
    .hint = 0,   // T_ key for hint subtitle (0 = placeholder)
};
```

| Field | Purpose |
|-------|---------|
| `.u.min`, `.u.max` | Valid range (inclusive) |
| `.u.def` | Default value when flag is absent on CLI |
| `.prompt` | T_ translation key for interactive menu title (`0` = placeholder) |
| `.hint` | T_ translation key for subtitle below prompt (`0` = placeholder) |

**Named choices** (`BP_VAL_CHOICE`):

For parameters that select from a list of named options (e.g. parity, output type), use `BP_VAL_CHOICE` with a `bp_val_choice_t` array:

```c
static const bp_val_choice_t output_choices[] = {
    { "push-pull",  "pp", 0, 0 }, // value=0
    { "open-drain", "od", 0, 1 }, // value=1
};
static const bp_val_constraint_t output_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = output_choices, .count = 2, .def = 0 },
    .prompt = 0,
};
```

Each `bp_val_choice_t` entry:

| Field | Purpose |
|-------|---------|
| `.name` | CLI string the user types (e.g. `"push-pull"`) |
| `.alias` | Short alias (e.g. `"pp"`) |
| `.label` | T_ key for interactive menu label (`0` = placeholder) |
| `.value` | Integer stored when selected |

### All Constraint Types

| Type | Union | Fields |
|------|-------|--------|
| `BP_VAL_UINT32` | `.u` | `.min`, `.max`, `.def` |
| `BP_VAL_INT32` | `.i` | `.min`, `.max`, `.def` |
| `BP_VAL_FLOAT` | `.f` | `.min`, `.max`, `.def` |
| `BP_VAL_CHOICE` | `.choice` | `.choices`, `.count`, `.def` |

If a flag takes a string value (e.g. a filename) with no numeric validation, omit the constraint — use `bp_cmd_get_string()` directly.

---

## Step 5: Flag / Option Table

Maps CLI flags to constraints. Each entry defines a flag name, short name, argument type, hint text, description, and optional constraint. The array **must** end with a `{ 0 }` sentinel:

```c
static const bp_command_opt_t dummy_opts[] = {
    { "button",  'b', BP_ARG_NONE,     NULL,    T_HELP_DUMMY_B_FLAG },
    { "integer", 'i', BP_ARG_REQUIRED, "value", T_HELP_DUMMY_I_FLAG, &integer_range },
    { "file",    'f', BP_ARG_REQUIRED, "file",  T_HELP_DUMMY_FILE_FLAG },
    { 0 }, // ← sentinel — always required
};
```

| Field | Purpose |
|-------|---------|
| `long_name` | `--button` on the command line |
| `short_name` | `-b` on the command line |
| `arg_type` | `BP_ARG_NONE` (boolean switch) or `BP_ARG_REQUIRED` (takes a value) |
| `arg_hint` | Shown in help text: `-i <value>`. `NULL` for boolean flags |
| `description` | T_ key for help text (`0` = placeholder) |
| `constraint` | Pointer to a `bp_val_constraint_t`, or omit/`NULL` for no auto-validation |

Supported flag syntax on the command line:

| Format | Example |
|--------|---------|
| `-f value` | `-i 123` |
| `-f=value` | `-i=123` |
| `--long value` | `--integer 123` |
| `--long=value` | `--integer=123` |

---

## Step 6: Command Definition

The master struct that ties everything together. **Must be non-static** so it can be exported via the header and wired into the registration array:

```c
const bp_command_def_t dummy_def = {
    .name = "dummy",
    .description = 0x00,        // T_ key for `h` listing (0x00 = no description)
    .actions = dummy_action_defs,
    .action_count = count_of(dummy_action_defs),
    .opts = dummy_opts,
    .usage = usage,
    .usage_count = count_of(usage),
};
```

| Field | Required? | Purpose |
|-------|-----------|---------|
| `.name` | **Yes** | Command string users type at the prompt |
| `.description` | No | T_ key for `h` listing (0 = hidden / placeholder) |
| `.actions` | No | Pointer to action verb array (`NULL` if no subcommands) |
| `.action_count` | No | Number of action entries (0 if no subcommands) |
| `.opts` | No | Pointer to flag table (`NULL` if no flags) |
| `.usage` | **Yes** | Pointer to usage string array |
| `.usage_count` | **Yes** | Number of usage entries |
| `.positionals` | No | Pointer to positional args table (`NULL` for most commands) |
| `.positional_count` | No | Number of positional entries |

---

## Step 7: The Handler Function

This is the entry point called when the user types your command. The dispatcher passes a `command_result` struct — `res->help_flag` is set automatically if the user entered `-h`.

### 7a: Help Check

Always handle help first. `bp_cmd_help_check()` displays auto-generated help (usage examples, flags table, actions list) and returns `true` if `-h` was given:

```c
void dummy_handler(struct command_result* res) {

    if (bp_cmd_help_check(&dummy_def, res->help_flag)) {
        return;
    }
```

### 7b: Safety / Precondition Checks (Optional)

If your command requires a valid voltage reference, call `ui_help_check_vout_vref()`. To restrict to a specific mode, check `system_config.mode` — but **mode commands** (registered per-mode) are the preferred way to scope commands:

```c
    printf("Current mode: %d\r\n", system_config.mode);

    if (!ui_help_check_vout_vref()) {
        printf("Warning: Vout pin is not connected to a valid voltage source\r\n");
    } else {
        printf("Vout pin is connected to a valid voltage source\r\n");
    }
```

### 7c: Action Resolution

`bp_cmd_get_action()` matches the first non-flag token against the actions array. Returns `true` and writes the enum value, or `false` if no action token is present:

```c
    uint32_t action = 0;
    if (bp_cmd_get_action(&dummy_def, &action)) {
        printf("Action: %s (enum=%d)\r\n",
               (action == DUMMY_INIT ? "init" : "test"),
               action);
    } else {
        printf("No action given (try: dummy init, dummy test)\r\n");
    }
```

Use a `switch` on the action enum in real commands to dispatch to separate logic paths.

### 7d: Boolean Flag — `bp_cmd_find_flag()`

Returns `true` if the flag is present, `false` if not. No value is consumed — this is a simple on/off switch:

```c
    bool b_flag = bp_cmd_find_flag(&dummy_def, 'b');
    printf("Flag -b is %s\r\n", (b_flag ? "set" : "not set"));
    if (b_flag) {
        printf("Press Bus Pirate button to continue\r\n");
        while (!button_get(0)) {
            tight_loop_contents();
        }
        printf("Button pressed\r\n");
    }
```

### 7e: Constraint-Aware Integer Flag — `bp_cmd_flag()`

`bp_cmd_flag()` uses the constraint on the opt to parse, validate, and provide a default. This is the preferred way to get a validated integer from a flag:

```c
    uint32_t value;
    bp_cmd_status_t i_status = bp_cmd_flag(&dummy_def, 'i', &value);

    if (i_status == BP_CMD_OK) {
        printf("Flag -i is set with value %d\r\n", value);
    } else if (i_status == BP_CMD_INVALID) {
        // Constraint violation — the API already printed the range error.
        printf("Flag -i has an invalid value. Try -i 0\r\n");
        system_config.error = true;
        return;
    } else { // BP_CMD_MISSING — flag not entered, default written to value
        printf("Flag -i is not set (default: %d)\r\n", value);
    }
```

`bp_cmd_flag()` return values:

| Status | Meaning |
|--------|---------|
| `BP_CMD_OK` | Flag found, value parsed and valid — written to output |
| `BP_CMD_MISSING` | Flag not on command line — constraint default written to output |
| `BP_CMD_INVALID` | Flag present but failed validation — error already printed |

### 7f: Interactive Prompt Fallback — The Dual-Path Pattern

If a flag is missing, you can fall back to an interactive prompt driven by the **same constraint**. This is the "dual-path" pattern — the user can provide the value on the CLI (`dummy init -i 123`) or be prompted interactively:

```c
    if (action == DUMMY_INIT && i_status == BP_CMD_MISSING) {
        printf("No -i flag given — entering interactive prompt:\r\n");
        // bp_cmd_prompt() displays a menu driven by the constraint,
        // validates input, and loops on error. Returns BP_CMD_OK or BP_CMD_EXIT.
        bp_cmd_status_t prompt_st = bp_cmd_prompt(&integer_range, &value);
        if (prompt_st != BP_CMD_OK) {
            printf("Prompt cancelled\r\n");
            return;
        }
        printf("User entered: %d\r\n", value);
    }
```

`bp_cmd_prompt()` return values:

| Status | Meaning |
|--------|---------|
| `BP_CMD_OK` | User entered a valid value |
| `BP_CMD_EXIT` | User cancelled (pressed `x`) |

For a full dual-path example with saved settings, see `dummy1.c` (mode setup) or `w_psu.c` (command).

### 7g: String Flag — `bp_cmd_get_string()`

Copies the flag's value as a string into a buffer. Returns `true` if present, `false` if not. Use for filenames, search strings, etc.:

```c
    char file[13]; // 8.3 filename + null = 13 characters max
    bool f_flag = bp_cmd_get_string(&dummy_def, 'f', file, sizeof(file));

    if (!f_flag) {
        printf("Flag -f is not set\r\n");
    } else {
        printf("Flag -f is set with file name %s\r\n", file);
        // ... use the filename ...
    }
```

### 7h: Error Reporting

To signal an error back to the command dispatcher (for chaining with `; || &&`), set `system_config.error = true` and return:

```c
    system_config.error = true;
    return;
```

---

## Step 8: The Header File

### Global Command Header

Export the def and handler function. The def must be `extern const` so `commands.c` can reference it:

```c
/**
 * @file dummy.h
 * @brief Dummy/test command interface.
 * @details Provides placeholder command for testing.
 */

extern const struct bp_command_def dummy_def;

/**
 * @brief Handler for dummy test command.
 * @param res  Command result structure
 */
void dummy_handler(struct command_result* res);
```

### Mode Command Header

Mode command handlers and defs are declared in **separate headers** under `src/commands/<mode>/`. The main mode header (`src/mode/hwxxx.h`) exports the **command table** and its count:

```c
// In src/mode/hwuart.h (bottom):
extern const struct _mode_command_struct hwuart_commands[];
extern const uint32_t hwuart_commands_count;
```

```c
// In src/commands/uart/bridge.h (individual command):
extern const struct bp_command_def uart_bridge_def;
void uart_bridge_handler(struct command_result* res);
```

---

## Step 9: Registration

### Global Commands — `commands.c`

Add your command to the `commands[]` array. Each entry is a `_global_command_struct`:

```c
{ .command="dummy", .allow_hiz=true, .func=&dummy_handler,
  .def=&dummy_def, .description_text=0x00, .category=CMD_CAT_HIDDEN },
```

| Field | Purpose |
|-------|---------|
| `.command` | String users type at the prompt |
| `.func` | Pointer to your handler function |
| `.def` | Pointer to your `bp_command_def_t` (enables help, hints, completion) |
| `.allow_hiz` | `true` = works in HiZ mode, `false` = requires active mode |
| `.description_text` | T_ key for the `h` help listing (`0x00` = hidden from listing) |
| `.category` | Help menu group (see categories below) |

Don't forget the include at the top of `commands.c`:

```c
#include "commands/global/dummy.h"
```

#### Command Categories

| Category | Shown Under |
|----------|-------------|
| `CMD_CAT_IO` | Pin I/O, power, measurement |
| `CMD_CAT_CONFIGURE` | Terminal, display, mode config |
| `CMD_CAT_SYSTEM` | Info, reboot, selftest |
| `CMD_CAT_FILES` | Storage and file operations |
| `CMD_CAT_SCRIPT` | Scripting and macros |
| `CMD_CAT_TOOLS` | Utilities and converters |
| `CMD_CAT_MODE` | Mode selection |
| `CMD_CAT_HIDDEN` | Not shown in help listing |

### Mode Commands — Per-Mode Command Table

Mode commands are registered in the mode's source file as an array of `_mode_command_struct`. The command name comes from the def (not a separate `.command` field):

```c
// In src/mode/hwuart.c:
const struct _mode_command_struct hwuart_commands[] = {
    {   .func=&nmea_decode_handler,
        .def=&nmea_decode_def,
        .supress_fala_capture=true
    },
    {   .func=&uart_bridge_handler,
        .def=&uart_bridge_def,
        .supress_fala_capture=true
    },
    { 0 }, // sentinel
};
const uint32_t hwuart_commands_count = count_of(hwuart_commands);
```

| Field | Purpose |
|-------|---------|
| `.func` | Pointer to the handler function |
| `.def` | Pointer to the `bp_command_def_t` (command name comes from `.def->name`) |
| `.supress_fala_capture` | `true` = disable follow-along logic analyzer during this command |

The command table is then wired into the `modes[]` array in `modes.c`:

```c
[HWUART] = {
    // ...protocol function pointers...
    .mode_commands = hwuart_commands,
    .mode_commands_count = &hwuart_commands_count,
    // ...
},
```

---

## Global vs. Mode Commands — Comparison

| Aspect | Global Command | Mode Command |
|--------|---------------|-------------|
| **Scope** | Available in all modes (or non-HiZ) | Only when mode is active |
| **Registration struct** | `_global_command_struct` | `_mode_command_struct` |
| **Registered in** | `commands[]` in `commands.c` | e.g. `hwuart_commands[]` in mode file |
| **Command name** | `.command` string field | Comes from `.def->name` |
| **HiZ control** | `.allow_hiz` field | N/A — only active when mode is selected |
| **Help category** | `.category` (`CMD_CAT_*`) | N/A — listed under mode help |
| **FALA suppression** | N/A | `.supress_fala_capture` field |
| **Header exports** | `extern def` + handler | `extern` command table + count |

---

## Step 10: CMakeLists.txt

Add your `.c` file to the build. Open `src/CMakeLists.txt` and add your source file to the appropriate section:

```cmake
# Global command:
commands/global/mycmd.c

# Mode command (under the mode's section):
commands/uart/mycmd.c
```

---

## Parsing API Quick Reference

### Simple Queries (No Validation)

| Function | Returns | Purpose |
|----------|---------|---------|
| `bp_cmd_find_flag(def, 'e')` | `bool` | Is flag present? (boolean switch) |
| `bp_cmd_get_uint32(def, 'n', &val)` | `bool` | Parse flag value as uint32 |
| `bp_cmd_get_int32(def, 'n', &val)` | `bool` | Parse flag value as int32 |
| `bp_cmd_get_float(def, 'f', &val)` | `bool` | Parse flag value as float |
| `bp_cmd_get_string(def, 'f', buf, len)` | `bool` | Copy flag value as string |
| `bp_cmd_get_positional_string(def, pos, buf, len)` | `bool` | Positional arg as string |
| `bp_cmd_get_positional_uint32(def, pos, &val)` | `bool` | Positional arg as uint32 |
| `bp_cmd_get_remainder(def, &ptr, &len)` | `bool` | Raw text after command name |

### Constraint-Aware (Validates + Provides Defaults)

| Function | Returns | Purpose |
|----------|---------|---------|
| `bp_cmd_flag(def, 'u', &out)` | `bp_cmd_status_t` | Parse + validate flag with constraint |
| `bp_cmd_positional(def, pos, &out)` | `bp_cmd_status_t` | Parse + validate positional with constraint |
| `bp_cmd_prompt(constraint, &out)` | `bp_cmd_status_t` | Interactive prompt from constraint |

### Action Resolution

| Function | Returns | Purpose |
|----------|---------|---------|
| `bp_cmd_get_action(def, &action)` | `bool` | Match first non-flag token to actions array |

### Help

| Function | Returns | Purpose |
|----------|---------|---------|
| `bp_cmd_help_check(def, help_flag)` | `bool` | Show help if `-h`, return `true` if displayed |
| `bp_cmd_help_show(def)` | `void` | Unconditional help display |

### Status Codes

| Status | Meaning |
|--------|---------|
| `BP_CMD_OK` | Value obtained and valid |
| `BP_CMD_MISSING` | Not on command line (default written for flags) |
| `BP_CMD_INVALID` | Present but failed validation (error already printed) |
| `BP_CMD_EXIT` | User cancelled interactive prompt |

---

## Patterns & Recipes

### Simple Command (Help + One Flag)

Minimal pattern — help check and a boolean flag:

```c
void mycmd_handler(struct command_result* res) {
    if (bp_cmd_help_check(&mycmd_def, res->help_flag)) return;

    bool verbose = bp_cmd_find_flag(&mycmd_def, 'v');
    // ... do work ...
}
```

### Command with Actions

Route to different logic based on a subcommand verb:

```c
    uint32_t action = 0;
    bp_cmd_get_action(&mycmd_def, &action);
    switch (action) {
        case MYCMD_PROBE: /* ... */ break;
        case MYCMD_DUMP:  /* ... */ break;
        default: printf("No action given\r\n"); break;
    }
```

### Dual-Path (CLI + Interactive Fallback)

Try the CLI flag first; if missing, prompt interactively:

```c
    uint32_t voltage;
    bp_cmd_status_t st = bp_cmd_flag(&mycmd_def, 'v', &voltage);
    if (st == BP_CMD_INVALID) { system_config.error = true; return; }
    if (st == BP_CMD_MISSING) {
        if (bp_cmd_prompt(&voltage_constraint, &voltage) != BP_CMD_OK) return;
    }
```

### Enable/Disable Command Pairs

Commands like `W`/`w` (PSU on/off) use separate defs sharing usage strings:

```c
// W = enable, w = disable — separate handlers, separate defs
const bp_command_def_t psu_enable_def  = { .name="W", .opts=psu_opts, ... };
const bp_command_def_t psu_disable_def = { .name="w", ... };
```

---

## Checklist

- [ ] Create `src/commands/global/mycmd.c` (or `src/commands/<mode>/mycmd.c`)
- [ ] Define usage string array
- [ ] Define action enum + `bp_command_action_t[]` (if subcommands needed)
- [ ] Define `bp_val_constraint_t` for each validated parameter
- [ ] Define `bp_command_opt_t[]` flag table (sentinel-terminated with `{ 0 }`)
- [ ] Define `bp_command_def_t` (non-static, exported)
- [ ] Implement handler: `bp_cmd_help_check()` → preconditions → parsing → logic
- [ ] Create header file: `extern` for def + handler declaration
- [ ] **Global**: Add to `commands[]` in `commands.c` with include
- [ ] **Mode**: Add to mode's command table array + wire in `modes.c`
- [ ] Add `.c` file to `src/CMakeLists.txt`
- [ ] Build and verify

---

## Related Documentation

- [bp_cmd_developer_docs_outline.md](bp_cmd_developer_docs_outline.md) — Full `bp_cmd` API reference outline
- [new_mode_guide.md](new_mode_guide.md) — Implementing a new mode (setup, syntax handlers, modes.c)
- [command_setup_migration_prompt.md](command_setup_migration_prompt.md) — Migrating existing commands to `bp_cmd`
- `src/commands/global/dummy.c` — Full reference implementation (copy this as a starting point)
- `src/mode/hwuart.c` — Real-world mode with mode commands (bridge, monitor, glitch)
