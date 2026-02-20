# Command Migration Prompt

> **Purpose**: Instructions for a Claude Sonnet agent to migrate Bus Pirate global/utility commands from the old `ui_prompt_uint32()` / `ui_help_show()` / `cmdln_args_*()` system to the new constraint-based `bp_cmd` system.

---

## Context

The Bus Pirate firmware has commands (distinct from protocol *modes*) that use various old APIs for help display, argument parsing, and interactive prompts. The new unified system uses `bp_command_def_t` with `bp_val_constraint_t` constraints, driving help, hints, completion, command-line parsing, and interactive prompts from a single definition.

**`w_psu.c` has already been migrated** and serves as the reference implementation for commands. Your job is to migrate every other command listed below.

---

## Architecture Overview

### Old Systems (to be replaced)

There are three distinct old APIs in play. A command may use one, two, or all three:

#### 1. Old Help System (`ui_help_show`)
```c
#include "ui/ui_help.h"

static const char* const usage[] = { "mycmd [-h]", "Do thing: mycmd" };
static const struct ui_help_options options[] = {
    { 1, "", T_HELP_MYCMD },
    { 0, "-h", T_HELP_FLAG },
};

void mycmd_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }
    // ...
}
```

#### 2. Old CLI Parsing (`cmdln_args_*`)
```c
#include "ui/ui_cmdln.h"

bool has_val = cmdln_args_uint32_by_position(1, &val);
bool found = cmdln_args_find_flag('d');
bool got_str = cmdln_args_string_by_position(1, sizeof(buf), buf);
bool got_fstr = cmdln_args_find_flag_string('p', &arg, sizeof(buf), buf);
```

#### 3. Old Interactive Prompts (`ui_prompt_uint32`, `ui_prompt_bool`, `ui_prompt_float_units`)
```c
#include "ui/ui_prompt.h"

// Menu item arrays + ui_prompt structs
static const struct prompt_item menu_items[] = { { T_MENU_1 } };
static const struct ui_prompt menu[] = {
    { .description = T_MENU, .prompt_text = T_PROMPT,
      .minval = 1, .maxval = 100, .defval = 10,
      .config = &prompt_int_cfg },
};

prompt_result result;
uint32_t val;
ui_prompt_uint32(&result, &menu[0], &val);
if (result.exit) return;
```

### New System (constraint-based)

All three old systems are replaced by one `bp_command_def_t`:

```c
#include "lib/bp_args/bp_cmd.h"

// Constraints
static const bp_val_constraint_t my_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 100, .def = 10 },
    .prompt = T_MENU,
    .hint = T_MENU_1,
};

// Options (flags)
static const bp_command_opt_t my_opts[] = {
    { "value", 'v', BP_ARG_REQUIRED, "1-100", 0, &my_range },
    { 0 }
};

// Positionals (if any)
static const bp_command_positional_t my_positionals[] = {
    { "pin", NULL, T_HELP_PIN, false, &pin_range },
    { 0 }
};

// Usage strings
static const char* const usage[] = {
    "mycmd [pin] [-v <val>]",
    "Example: mycmd 3 -v 50",
};

// Definition
const bp_command_def_t mycmd_def = {
    .name = "mycmd",
    .description = T_HELP_MYCMD,
    .opts = my_opts,
    .positionals = my_positionals,
    .positional_count = 1,
    .usage = usage,
    .usage_count = count_of(usage),
};

// Handler
void mycmd_handler(struct command_result* res) {
    if (bp_cmd_help_check(&mycmd_def, res->help_flag)) return;

    // Positional: parse + validate, prompt if missing
    uint32_t pin;
    bp_cmd_status_t s = bp_cmd_positional(&mycmd_def, 1, &pin);
    if (s == BP_CMD_INVALID) { res->error = true; return; }
    if (s == BP_CMD_MISSING) {
        if (bp_cmd_prompt(&pin_range, &pin) != BP_CMD_OK) return;
    }

    // Flag: parse + validate, default if absent
    uint32_t val;
    s = bp_cmd_flag(&mycmd_def, 'v', &val);
    if (s == BP_CMD_INVALID) { res->error = true; return; }
    // val now has parsed value or default (10)
}
```

---

## Reference Implementation: PSU (`src/commands/global/w_psu.c`)

Study this file thoroughly. Key patterns:

### Constraints
- `voltage_range`: `BP_VAL_FLOAT`, 0.8–5.0, default 3.3
- `current_range`: `BP_VAL_FLOAT`, 0.0–500.0, default 300.0
- `undervoltage_range`: `BP_VAL_UINT32`, 1–100, default 10

### Positionals
Two positional arguments for the enable command:
```c
static const bp_command_positional_t psucmd_enable_positionals[] = {
    { "volts",   NULL, T_HELP_GCMD_W_VOLTS,         false, &voltage_range },
    { "current", "mA", T_HELP_GCMD_W_CURRENT_LIMIT, false, &current_range },
    { 0 }
};
```

### Flags
One flag (`-u`) with constraint:
```c
static const bp_command_opt_t psucmd_opts[] = {
    { "undervoltage", 'u', BP_ARG_REQUIRED, "%", T_HELP_GCMD_W_UNDERVOLTAGE, &undervoltage_range },
    { 0 }
};
```

### Dual-path handler pattern
```c
void psucmd_enable_handler(struct command_result* res) {
    if (bp_cmd_help_check(&psucmd_enable_def, res->help_flag)) return;

    // Flag: parse + default if absent
    uint32_t undervoltage_percent;
    bp_cmd_status_t s = bp_cmd_flag(&psucmd_enable_def, 'u', &undervoltage_percent);
    if (s == BP_CMD_INVALID) { res->error = true; return; }

    // Positional 1: voltage
    float volts;
    s = bp_cmd_positional(&psucmd_enable_def, 1, &volts);
    if (s == BP_CMD_MISSING || s == BP_CMD_INVALID) {
        // not on command line — prompt interactively
        if (bp_cmd_prompt(&voltage_range, &volts) != BP_CMD_OK) { res->error = true; return; }
        // interactive: also prompt for current
        if (bp_cmd_prompt(&current_range, &current) != BP_CMD_OK) { res->error = true; return; }
    } else {
        // voltage on cmdline — try current from cmdline too
        if (bp_cmd_positional(&psucmd_enable_def, 2, &current) == BP_CMD_INVALID) {
            res->error = true; return;
        }
    }
    // ... rest of handler ...
}
```

### Two defs for enable/disable
PSU has two handlers (W/w) with two separate `bp_command_def_t` structs. The disable def is minimal (just name + description + usage).

---

## Key API Reference

All declarations are in `src/lib/bp_args/bp_cmd.h`.

### Status codes
```c
typedef enum {
    BP_CMD_OK = 0,      // Value obtained and valid
    BP_CMD_MISSING,      // Not supplied on command line
    BP_CMD_INVALID,      // Supplied but failed validation (error already printed)
    BP_CMD_EXIT,         // User exited interactive prompt
} bp_cmd_status_t;
```

### Constraint types
```c
typedef enum {
    BP_VAL_NONE = 0,    // No constraint
    BP_VAL_UINT32,      // Unsigned 32-bit integer range
    BP_VAL_INT32,       // Signed 32-bit integer range
    BP_VAL_FLOAT,       // Floating-point range
    BP_VAL_CHOICE,      // Named choice from a fixed set
} bp_val_type_t;
```

### Core functions
| Function | Purpose |
|----------|---------|
| `bp_cmd_help_check(def, flag)` | Show help if `-h` passed; returns true if displayed (caller should `return`) |
| `bp_cmd_help_show(def)` | Unconditionally show help |
| `bp_cmd_find_flag(def, ch)` | Boolean: is flag present? |
| `bp_cmd_get_uint32(def, ch, &val)` | Get uint32 value for flag |
| `bp_cmd_get_string(def, ch, buf, len)` | Get string value for flag |
| `bp_cmd_get_float(def, ch, &val)` | Get float value for flag |
| `bp_cmd_positional(def, pos, &out)` | Parse + validate positional (constraint-aware, returns `bp_cmd_status_t`) |
| `bp_cmd_flag(def, ch, &out)` | Parse + validate flag (constraint-aware, returns `bp_cmd_status_t`) |
| `bp_cmd_prompt(constraint, &out)` | Interactive prompt driven by constraint |
| `bp_cmd_get_positional_uint32(def, pos, &val)` | Simple positional uint32 (no constraint check) |
| `bp_cmd_get_positional_string(def, pos, buf, len)` | Simple positional string |
| `bp_cmd_get_action(def, &action)` | Find action/subcommand verb |

---

## Commands to Migrate

### Tier 1: Full Interactive Prompt Migration

These commands have `ui_prompt_uint32()` calls in their setup/handler that collect configuration from users.

---

#### 1. `src/commands/uart/glitch.c` — UART Glitch Generator

**Current state**: Already has `bp_command_def_t uart_glitch_def` with one `-c` (config) flag for help. But `uart_glitch_setup()` uses **8 sequential `ui_prompt_uint32()` calls** plus `ui_prompt_bool()` for saved-config confirmation. Uses `storage_load_mode()`/`storage_save_mode()` for config persistence.

**Parameters** (from the `uart_menu[]` array):

| # | Field | Description T_ key | Prompt T_ key | Min | Max | Default | Units |
|---|-------|--------------------|---------------|-----|-----|---------|-------|
| 0 | `glitch_trg` | `T_UART_GLITCH_TRG_MENU` | `T_UART_GLITCH_TRG_PROMPT` | 1 | 255 | 13 | ASCII |
| 1 | `glitch_delay` | `T_UART_GLITCH_DLY_MENU` | `T_UART_GLITCH_DLY_PROMPT` | 1 | 5000000 | 1 | ns×10 |
| 2 | `glitch_wander` | `T_UART_GLITCH_VRY_MENU` | `T_UART_GLITCH_VRY_PROMPT` | 1 | 50 | 1 | ns×10 |
| 3 | `glitch_time` | `T_UART_GLITCH_LNG_MENU` | `T_UART_GLITCH_LNG_PROMPT` | 0 | 5000000 | 1 | ns×10 |
| 4 | `glitch_recycle` | `T_UART_GLITCH_CYC_MENU` | `T_UART_GLITCH_CYC_PROMPT` | 10 | 1000 | 10 | ms |
| 5 | `fail_resp` | `T_UART_GLITCH_FAIL_MENU` | `T_UART_GLITCH_FAIL_PROMPT` | 1 | 255 | 35 | ASCII |
| 6 | `retry_count` | `T_UART_GLITCH_CNT_MENU` | `T_UART_GLITCH_CNT_PROMPT` | 1 | 10000 | 100 | — |
| 7 | `disable_ready` | `T_UART_GLITCH_NORDY_MENU` | `T_UART_GLITCH_NORDY_PROMPT` | 0 | 1 | 1 | bool |

**Migration plan**:

1. **Create 8 `bp_val_constraint_t` structs**, one per parameter. All are `BP_VAL_UINT32`. Use the existing `T_*_MENU` key for `.prompt` and `T_*_MENU_1` key for `.hint`.

2. **Expand the existing `glitch_opts[]`** to include 8 new flags (plus the existing `-c`):
   - `-t` trigger char, `-d` delay, `-w` wander, `-g` glitch time, `-r` recycle, `-f` fail char, `-n` retry count, `-y` disable ready
   - Each with `BP_ARG_REQUIRED` and pointing to its constraint
   - Keep `-c` (config display) as `BP_ARG_NONE` with no constraint

3. **Update `uart_glitch_def`** — it already exists, just add/update fields as needed.

4. **Rewrite `uart_glitch_setup()`** using the dual-path pattern:
   - Check the "primary" flag (e.g. `-t` trigger) to detect interactive vs CLI mode
   - Interactive path: keep `storage_load_mode()` check + `ui_prompt_bool()` for "use saved?" + sequential `bp_cmd_prompt()` calls + `storage_save_mode()`
   - CLI path: all flags via `bp_cmd_flag()` with defaults
   - The `ui_prompt_bool()` for "use saved settings?" is a **confirmation prompt**, not a config prompt — keep it as `ui_prompt_bool()` for now (or replace with a simple y/n read)

5. **Remove**: the old `prompt_item` arrays, `struct ui_prompt uart_menu[]` array, the 8 `ui_prompt_uint32()` calls.

6. **Keep**: `glitch_settings()` display function (it uses `ui_prompt_mode_settings_int()`), `storage_load_mode()`/`storage_save_mode()`, all hardware setup, the main glitch loop.

7. **Update the handler** `uart_glitch_handler()`: Currently it checks `-c` flag via `bp_cmd_find_flag()` and calls `uart_glitch_setup()` unconditionally when running. After migration, the handler should support both:
   - `glitch` → interactive setup + run
   - `glitch -t 13 -d 100 -w 5 -g 1 -r 10 -f 35 -n 100 -y 1` → CLI setup + run
   - `glitch -c` → display current settings (keep as-is)

**Suggested flag chars**:
| Flag | Long name | Parameter |
|------|-----------|-----------|
| `-t` | `trigger` | Trigger character (ASCII) |
| `-d` | `delay` | Delay after trigger (ns×10) |
| `-w` | `wander` | Timing variation (ns×10) |
| `-g` | `glitchtime` | Glitch output time (ns×10) |
| `-r` | `recycle` | Recycle time (ms) |
| `-f` | `failchar` | Expected fail response (ASCII) |
| `-n` | `retries` | Retry count |
| `-y` | `noready` | Disable ready check (0/1) |
| `-c` | `config` | Show config (no argument) |

---

#### 2. `src/commands/global/cmd_binmode.c` — Binary Mode Selection

**Current state**: Has `bp_command_def_t cmd_binmode_def` for help. Uses `ui_prompt_uint32()` with a **custom `ui_prompt_config`** that has:
- Dynamic menu renderer (`binmode_prompt_menu`) that iterates runtime `binmodes[]` array
- Custom validation callback (`binmode_check_range`) checking against `count_of(binmodes)`
- Also uses `ui_prompt_bool()` for "Save setting?" confirmation

**This is a SPECIAL CASE**: The menu items come from a runtime array (`binmodes[]`), not from static `T_` translation keys. This means you **cannot** use `BP_VAL_CHOICE` (which requires static `bp_val_choice_t` arrays).

**Migration plan**:

1. **Use `BP_VAL_UINT32` constraint** with min=1, max=`count_of(binmodes)`, default=1. The interactive prompt from `bp_cmd_prompt()` will show "1-N" range.

2. **Add a positional argument** for the binmode number:
   ```c
   static const bp_val_constraint_t binmode_range = {
       .type = BP_VAL_UINT32,
       .u = { .min = 1, .max = count_of(binmodes), .def = 1 },
       .prompt = T_CONFIG_BINMODE_SELECT,
   };
   ```
   **Important**: `count_of(binmodes)` may not work in a static initializer if `binmodes` is `extern`. If so, use a hardcoded max or validate at runtime.

3. **Update `cmd_binmode_def`** with the positional.

4. **Rewrite `cmd_binmode_handler()`**:
   - `bp_cmd_help_check()` — keep (already present)
   - Try `bp_cmd_positional()` for position 1 → binmode number
   - If `BP_CMD_MISSING`: print the dynamic binmode menu manually (loop over `binmodes[]`, print `i+1. name`), then call `bp_cmd_prompt(&binmode_range, &binmode_number)` for the number
   - If `BP_CMD_OK`: use the value directly
   - Keep the `ui_prompt_bool()` for "Save setting?" as-is (it's a confirmation, not config)

5. **Remove**: `binmode_prompt_menu()`, `binmode_check_range()`, the old `ui_prompt_config`, `ui_prompt` struct, and `ui_prompt_uint32()` call.

6. **Note**: The custom menu display (printing binmode names) needs to happen *before* `bp_cmd_prompt()` since the constraint prompt only shows the numeric range. Print the numbered list, then prompt for the number.

---

### Tier 2: Pin Selection + Complex Prompts

These commands use `ui_prompt_menu_bio_pin` with hardware-state validation callbacks. The pin menus are dynamic (available pins depend on what's currently in use).

---

#### 3. `src/commands/global/freq.c` — Frequency Measurement

**Current state**: Already has `bp_command_def_t freq_single_def` and `freq_cont_def` with positional pin argument. Already uses `bp_cmd_help_check()` and `bp_cmd_get_positional_uint32()`. **BUT** falls back to `ui_prompt_uint32()` with `ui_prompt_menu_bio_pin` and custom validation callbacks when no pin is given on the command line.

**Two handlers**: `freq_single()` (f) and `freq_cont()` (F).
**Two internal functions with old prompts**: `freq_configure_enable()` and `freq_configure_disable()`.

**Custom validation callbacks**:
- `freq_check_pin_is_available()` — checks PWM channel B, pin not in use, no PWM conflict, PSU bug workaround
- `freq_check_pin_is_active()` — checks `freq_active` bitmap

**Migration plan**:

1. **Create a pin constraint** `BP_VAL_UINT32` with min=0, max=7 (BIO_MAX_PINS-1), no default. The constraint provides basic range checking; the hardware-state validation must happen **after** the value is obtained.

2. **Add the constraint to the existing `freq_positionals[]`**:
   ```c
   static const bp_val_constraint_t freq_pin_range = {
       .type = BP_VAL_UINT32,
       .u = { .min = 0, .max = 7, .def = 1 },
       .prompt = T_MODE_CHOOSE_AVAILABLE_PIN,
   };

   static const bp_command_positional_t freq_positionals[] = {
       { "pin", NULL, T_HELP_GCMD_FREQ_PIN, false, &freq_pin_range },
       { 0 }
   };
   ```

3. **Rewrite `freq_configure_enable()`**:
   - Print available pins manually (loop over `bio2bufiopin`, check `freq_check_pin_is_available()` for each, print numbered list)
   - Use `bp_cmd_prompt(&freq_pin_range, &pin)` to get the pin number
   - After getting the value, validate with `freq_check_pin_is_available()` — if invalid, print error and re-prompt or return error
   - Remove old `ui_prompt_config`, `ui_prompt` struct, `ui_prompt_uint32()` call

4. **Rewrite `freq_configure_disable()`**: Same pattern but using `freq_check_pin_is_active()`.

5. **Update `freq_single()` / `freq_cont()`**:
   - Replace `bp_cmd_get_positional_uint32()` with `bp_cmd_positional()` for constraint validation
   - On `BP_CMD_MISSING`, call the rewritten configure function
   - On `BP_CMD_OK`, validate pin availability then proceed

6. **Remove**: the old `ui_prompt_config` structs, `ui_prompt` structs inside `freq_configure_enable/disable`.

7. **Keep**: `freq_check_pin_is_available()` and `freq_check_pin_is_active()` — these are still needed for runtime hardware-state validation. They just won't be passed as callbacks to the old prompt system anymore. Their signatures will need to change: remove the `const struct ui_prompt* menu` parameter, take just `uint32_t pin` directly.

---

#### 4. `src/commands/global/pwm.c` — PWM Generation

**Current state**: **No `bp_command_def_t` at all.** Uses:
- `ui_prompt_uint32()` with `ui_prompt_menu_bio_pin` for pin selection (enable)
- `ui_prompt_uint32()` with `ui_prompt_menu_bio_pin` for pin selection (disable, multi-pin case)
- `cmdln_args_uint32_by_position()` for disable pin on command line
- `ui_prompt_float_units()` for frequency input (accepts ns, us, ms, Hz, kHz, MHz)
- `ui_prompt_float_units()` for duty cycle input (accepts %)

**Two handlers**: `pwm_configure_enable()` (G) and `pwm_configure_disable()` (g).

**Custom validation callbacks**:
- `pwm_check_pin_is_available()` — checks pin not in use, no freq conflict, no PWM conflict
- `pwm_check_pin_is_active()` — checks `pwm_active` bitmap

**SPECIAL: `ui_prompt_float_units()`**: This prompt accepts a number with unit suffix (e.g. `100kHz`, `500ns`, `50%`). `bp_cmd_prompt()` does NOT support this — it only handles bare numbers. The frequency/duty-cycle interactive entry is complex custom logic that should be kept as a custom prompt loop.

**Migration plan**:

1. **Create `bp_command_def_t` for both commands**:
   ```c
   const bp_command_def_t pwm_enable_def = {
       .name = "G",
       .description = 0,  // placeholder
       .positionals = pwm_positionals,
       .positional_count = 1,
       .usage = pwm_usage,
       .usage_count = count_of(pwm_usage),
   };

   const bp_command_def_t pwm_disable_def = {
       .name = "g",
       .description = 0,
       .positionals = pwm_positionals,
       .positional_count = 1,
       .usage = pwm_usage,
       .usage_count = count_of(pwm_usage),
   };
   ```

2. **Create a pin constraint** (same as freq):
   ```c
   static const bp_val_constraint_t pwm_pin_range = {
       .type = BP_VAL_UINT32,
       .u = { .min = 0, .max = 7, .def = 0 },
       .prompt = T_MODE_CHOOSE_AVAILABLE_PIN,
   };
   ```

3. **Create usage strings and positionals**:
   ```c
   static const char* const pwm_usage[] = {
       "g|G [pin]",
       "Enable PWM with menu:%s G",
       "Disable PWM on pin 2:%s g 2",
   };

   static const bp_command_positional_t pwm_positionals[] = {
       { "pin", NULL, 0, false, &pwm_pin_range },
       { 0 }
   };
   ```

4. **Rewrite `pwm_configure_enable()`**:
   - Add `bp_cmd_help_check()` at top
   - Try `bp_cmd_positional()` for pin
   - If `BP_CMD_MISSING`: print available pins (loop, check `pwm_check_pin_is_available()`), then `bp_cmd_prompt(&pwm_pin_range, &pin)`, then validate
   - If `BP_CMD_OK`: validate pin availability
   - **Keep `pwm_get_settings()` and its `ui_prompt_float_units()` calls as-is** — the frequency/duty cycle entry is too complex for simple constraints. This is acceptable for now.

5. **Rewrite `pwm_configure_disable()`**:
   - Replace `cmdln_args_uint32_by_position()` with `bp_cmd_positional()`
   - Replace the `ui_prompt_uint32()` multi-pin menu with manual pin list + `bp_cmd_prompt()`
   - Validate with `pwm_check_pin_is_active()` after getting value

6. **Add header declarations** for the new defs if needed (check if commands.c or similar needs extern access).

7. **Remove**: `#include "ui/ui_cmdln.h"`, old `ui_prompt_config` structs, old `ui_prompt` structs, `ui_prompt_uint32()` calls for pin selection.

8. **Keep**: `pwm_get_settings()` with its `ui_prompt_float_units()` loop — **do not attempt to migrate this**. The unit-parsing prompt is a special interactive feature. Also keep `pwm_check_pin_is_available()` and `pwm_check_pin_is_active()`. Their signatures will need to change: remove the `const struct ui_prompt* menu` parameter, take just `uint32_t* pin` or `uint32_t pin` directly.

---

### Tier 3: Old Help/CLI Parsing Only (No Interactive Prompts)

These commands use `ui_help_show()` and/or `cmdln_args_*()` but have no `ui_prompt_uint32()` interactive prompts.

---

#### 5. `src/commands/global/p_pullups.c` — Programmable Pull Resistors

**Current state**: Uses `ui_help_show()` for help, `cmdln_args_string_by_position()` for resistor value, `cmdln_args_find_flag('d')` for down direction, `cmdln_args_find_flag_string('p', ...)` for pin selection. No interactive prompts. Two handlers (`pullups_enable_handler`, `pullups_disable_handler`). Has `#if BP_HW_PULLX` conditional for rev10+ hardware.

**Migration plan**:

1. **Create `bp_command_def_t` for both P and p**:
   ```c
   // For rev10+ (BP_HW_PULLX):
   static const bp_command_opt_t pullx_opts[] = {
       { "down", 'd', BP_ARG_NONE, NULL, 0 },
       { "pins", 'p', BP_ARG_REQUIRED, "01234567", 0 },
       { 0 }
   };

   static const bp_command_positional_t pullx_positionals[] = {
       { "value", NULL, 0, false },  // resistor value string
       { 0 }
   };

   const bp_command_def_t pullups_enable_def = {
       .name = "P",
       .description = T_HELP_GCMD_P,
       .opts = pullx_opts,          // NULL if !BP_HW_PULLX
       .positionals = pullx_positionals,  // NULL if !BP_HW_PULLX
       .positional_count = 1,       // 0 if !BP_HW_PULLX
       .usage = p_usage,
       .usage_count = count_of(p_usage),
   };
   ```

2. **Rewrite `pullups_enable_handler()`**:
   - Replace `ui_help_show()` with `bp_cmd_help_check()`
   - Replace `cmdln_args_string_by_position(1, ...)` with `bp_cmd_get_positional_string(&def, 1, ...)`
   - Replace `cmdln_args_find_flag('d')` with `bp_cmd_find_flag(&def, 'd')`
   - Replace `cmdln_args_find_flag_string('p', ...)` with `bp_cmd_get_string(&def, 'p', ...)`
   - Keep all the `pullx_parse_args()` logic but rewire it to use `bp_cmd_*` functions (or inline the logic into the handler)

3. **Rewrite `pullups_disable_handler()`**:
   - Replace `ui_help_show()` with `bp_cmd_help_check()`

4. **Remove**: `#include "ui/ui_cmdln.h"`, old `ui_help_options` array.

5. **Add**: `#include "lib/bp_args/bp_cmd.h"`.

6. **Note**: The `#if BP_HW_PULLX` conditional compilation must be preserved. The defs may need to differ between hardware revisions.

---

#### 6. `src/commands/uart/monitor.c` — UART Monitor/Test

**Current state**: Uses `ui_help_show()` for help and `cmdln_args_find_flag('t')` for a toolbar-pause flag. No interactive prompts.

**Migration plan**:

1. **Create `bp_command_def_t`**:
   ```c
   static const bp_command_opt_t monitor_opts[] = {
       { "toolbar", 't', BP_ARG_NONE, NULL, 0 },
       { 0 }
   };

   const bp_command_def_t uart_monitor_def = {
       .name = "test",
       .description = T_UART_CMD_TEST,
       .opts = monitor_opts,
       .usage = usage,
       .usage_count = count_of(usage),
   };
   ```

2. **Rewrite handler**:
   - Replace `ui_help_show(...)` with `bp_cmd_help_check(&uart_monitor_def, res->help_flag)`
   - Replace `cmdln_args_find_flag('t' | 0x20)` with `bp_cmd_find_flag(&uart_monitor_def, 't')`
   - Note: the old code does `'t' | 0x20` (forces lowercase) — `bp_cmd_find_flag` handles case naturally

3. **Remove**: `#include "ui/ui_cmdln.h"`, old `ui_help_options` array.

4. **Add**: `#include "lib/bp_args/bp_cmd.h"`.

---

#### 7. `src/commands/uart/simcard.c` — SIM Card Handler

**Current state**: Uses `ui_help_show()` for help. No CLI parsing, no interactive prompts. Mostly stub code.

**Migration plan**:

1. **Create `bp_command_def_t`**:
   ```c
   const bp_command_def_t simcard_def = {
       .name = "sim",
       .description = T_HELP_UART_BRIDGE,
       .usage = usage,
       .usage_count = count_of(usage),
   };
   ```

2. **Rewrite handler**:
   - Replace `ui_help_show(...)` with `bp_cmd_help_check(&simcard_def, res->help_flag)`

3. **Remove**: old `ui_help_options` array.

---

## Commands NOT to Migrate

### Confirmation-only `ui_prompt_bool()` commands
These commands use `ui_prompt_bool()` only for yes/no confirmation before destructive actions. They are NOT configuration prompts and should be **left as-is**:
- `src/commands/spi/eeprom_base.c` — "Erase EEPROM?" confirm
- `src/commands/spi/flash.c` — "Write flash?" confirm
- `src/commands/global/disk.c` — "Format disk?" confirm
- `src/commands/global/cmd_selftest.c` — "Run self-test?" confirm

### Already migrated
- `src/commands/global/w_psu.c` — reference implementation ✅
- `src/commands/global/cmd_sine.c` — already on new system ✅

### Excluded
- `c` config command — special, do not migrate

---

## Critical Rules

1. **Do NOT modify translation files**. Use `0` as a placeholder for any new `T_` keys that don't exist yet. Reuse existing `T_` keys wherever possible (especially for `.prompt` and `.hint` fields).

2. **Include guard**: Add `#include "lib/bp_args/bp_cmd.h"` to each migrated file. Remove `#include "ui/ui_cmdln.h"` if no longer used. Keep `#include "ui/ui_prompt.h"` only if `ui_prompt_bool()` or `ui_prompt_float_units()` is still used.

3. **Keep `ui_help_check_vout_vref()`** — this is a utility function, not part of the help system being replaced.

4. **Keep `ui_prompt_mode_settings_int()` / `ui_prompt_mode_settings_string()`** — these are display functions used by settings-show commands, not prompt functions.

5. **Keep `ui_prompt_bool()`** for confirmation prompts (yes/no before destructive actions, "use saved settings?"). Only migrate `ui_prompt_bool()` if it's being used as a config parameter (like glitch's `disable_ready` — that should become a flag).

6. **Keep `ui_prompt_float_units()`** — the unit-parsing prompt in `pwm.c` is complex and has no `bp_cmd` equivalent. Leave it for now.

7. **Storage pattern**: Commands that use `storage_load_mode()` / `storage_save_mode()` must preserve this. The dual-path pattern works: in interactive mode, check saved config first; in CLI mode, always use the provided flags.

8. **Pin selection pattern**: For commands that select BIO pins with hardware-state validation (`freq.c`, `pwm.c`):
   - Use `BP_VAL_UINT32` constraint for basic range (0-7)
   - Print available pins manually before prompting
   - Validate hardware state **after** getting the value from `bp_cmd_prompt()` or `bp_cmd_positional()`
   - Re-prompt or return error if validation fails
   - The old validation callback signatures include `const struct ui_prompt* menu` — update them to take simpler parameters (just `uint32_t pin` or `uint32_t* pin`)

9. **Extern declarations**: If a `bp_command_def_t` needs to be referenced from outside the file (e.g., for hint registration in `commands.c`), declare it `extern` in the corresponding header file. Non-static def → extern in header.

10. **Build check**: After each file migration, the project must build cleanly with `cmake --build build`.

11. **Usage string format**: Usage strings follow the pattern `"command\t[-flags] [args]"` for the first line (synopsis), then `"Description:%s command args"` for examples (the `%s` is replaced with formatting at display time).

---

## Step-by-Step Per Command

For each command:

1. Read the entire source file to understand all parameters, handlers, and special logic.
2. Identify all old API calls: `ui_help_show`, `ui_prompt_uint32`, `ui_prompt_bool`, `ui_prompt_float_units`, `cmdln_args_*`.
3. Create `bp_val_constraint_t` structs for each parameter that needs one.
4. Create `bp_command_opt_t` array for flags (sentinel-terminated with `{ 0 }`).
5. Create `bp_command_positional_t` array if the command takes positional args.
6. Create usage strings.
7. Create `bp_command_def_t` (or update existing one).
8. Rewrite handler(s) using `bp_cmd_help_check()`, `bp_cmd_positional()`, `bp_cmd_flag()`, `bp_cmd_prompt()`.
9. Remove dead old code: old prompt arrays, old help arrays, old `#include`s.
10. Build and verify.

---

## Summary Checklist

| # | Command | File | Complexity | Status |
|---|---------|------|------------|--------|
| 1 | glitch | `src/commands/uart/glitch.c` | High (8 params + storage) | ☐ |
| 2 | binmode | `src/commands/global/cmd_binmode.c` | Medium (dynamic menu) | ☐ |
| 3 | freq (f/F) | `src/commands/global/freq.c` | Medium (pin selection) | ☐ |
| 4 | pwm (g/G) | `src/commands/global/pwm.c` | High (pin + float units) | ☐ |
| 5 | pullups (p/P) | `src/commands/global/p_pullups.c` | Medium (flags + conditional) | ☐ |
| 6 | monitor/test | `src/commands/uart/monitor.c` | Low (help + 1 flag) | ☐ |
| 7 | sim | `src/commands/uart/simcard.c` | Low (help only) | ☐ |

**Suggested order**: Start with Tier 3 (simcard → monitor → pullups) for quick wins, then Tier 1 (glitch → binmode), then Tier 2 (freq → pwm). Build after each.
