# Implementing a New Bus Pirate Mode

> A step-by-step guide to creating a protocol mode for Bus Pirate 5/6/7 firmware.  
> Reference implementation: `src/mode/dummy1.c`

---

## Overview

A **mode** is a protocol handler (UART, SPI, I2C, etc.) that plugs into the Bus Pirate's syntax engine. Each mode provides:

- **Setup** — configuration UI (interactive wizard *and* CLI flags)
- **Saved settings** — persist/reload config to flash
- **Hardware init/teardown** — pin claiming, peripheral setup, cleanup
- **Syntax handlers** — write, read, start, stop (driven by the bytecode pipeline)
- **Macros** — numbered shortcuts like `(0)` menu, `(1)` action
- **Periodic service** — async polling (e.g. UART RX buffer)
- **Settings display** — shown by the `i` (info) command
- **Help** — mode-specific command listing
- **Mode commands** — extra commands available only in this mode

### How the Syntax Pipeline Works

The Bus Pirate uses a three-step pipeline for tight timing:

1. **Pre-process** — User input is compiled into simple bytecodes
2. **Execute** — Each bytecode is handed to your mode function for actual IO
3. **Post-process** — Results are formatted and printed to the terminal

**Important:** You cannot `printf()` during step 2. Instead, set fields on the `result` struct (`data_message`, `error`, `in_data`, etc.) and the pipeline will display them.

---

## File Structure

A mode consists of three touch points:

| File | Purpose |
|------|---------|
| `src/mode/mymode.c` | All mode logic — setup, handlers, constraints |
| `src/mode/mymode.h` | Function declarations, `extern` for def and commands |
| `src/modes.c` | Registration in the `modes[]` dispatch table |

You also need a `#define BP_USE_MYMODE` guard in `pirate.h` and a mode enum entry.

---

## Step 1: Includes and Config Struct

Start with the required headers and a file-static configuration struct.

```c
#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"           // Bytecode structure for data IO
#include "pirate/bio.h"         // Buffered pin IO functions
#include "pirate/storage.h"     // storage_load_mode / storage_save_mode
#include "ui/ui_help.h"         // ui_help_mode_commands
#include "ui/ui_prompt.h"       // ui_prompt_bool, ui_prompt_mode_settings_*
#include "ui/ui_term.h"         // ui_term_color_info, ui_term_color_reset
#include "dummy1.h"
#include "lib/bp_args/bp_cmd.h" // Constraint-based setup: flags, prompts, help, hints
```

Each mode keeps its own **file-static config struct**. All fields must be `uint32_t` so they work with the `storage_load_mode()` / `storage_save_mode()` helpers:

```c
static struct {
    uint32_t speed;     // Example numeric parameter (1..1000)
    uint32_t output;    // Example choice parameter (0=push-pull, 1=open-drain)
} mode_config;
```

---

## Step 2: Pin Labels and Mode Commands

```c
// Pin labels shown on the display and in the terminal status bar.
// No more than 4 characters long.
static const char pin_labels[][5] = { "OUT1", "OUT2", "OUT3", "IN1" };

// Mode-specific commands. If your mode has special commands
// (like UART's bridge, monitor, etc), add them here.
// The table MUST be { 0 } terminated.
const struct _mode_command_struct dummy1_commands[] = { 0 };
const uint32_t dummy1_commands_count = count_of(dummy1_commands);
```

---

## Step 3: Define Setup Constraints

The `bp_cmd` system uses a single `bp_command_def_t` to drive five concerns from one definition:

1. **CLI flag parsing** — `m dummy1 -s 500 -o od`
2. **Interactive prompting** — wizard menus with validation
3. **Help display** — `m dummy1 -h`
4. **Linenoise hints** — ghost text as you type
5. **Tab completion** — complete flag names

Build it bottom-up: **constraints → opts → def**.

### 3a: Value Constraints

Each configurable parameter gets a `bp_val_constraint_t` that defines its type, valid range (or choices), default value, and prompt text.

**Integer range** (`BP_VAL_UINT32`):

```c
static const bp_val_constraint_t dummy1_speed_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 1000, .def = 100 },
    .prompt = 0, // Use a T_ key here for translation, e.g. T_DUMMY1_SPEED_MENU
    .hint = 0,   // Use a T_ key here for a hint subtitle
};
```

| Field | Purpose |
|-------|---------|
| `.u.min`, `.u.max` | Valid range (inclusive) |
| `.u.def` | Default value when flag is absent on CLI |
| `.prompt` | T_ translation key for interactive menu title (`0` = placeholder) |
| `.hint` | T_ translation key for subtitle below prompt (`0` = placeholder) |

**Named choices** (`BP_VAL_CHOICE`):

```c
static const bp_val_choice_t dummy1_output_choices[] = {
    { "push-pull",  "pp", 0, 0 }, // value=0 → push-pull
    { "open-drain", "od", 0, 1 }, // value=1 → open-drain
};
static const bp_val_constraint_t dummy1_output_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = dummy1_output_choices, .count = 2, .def = 0 },
    .prompt = 0, // e.g. T_DUMMY1_OUTPUT_MENU
};
```

Each `bp_val_choice_t` entry:

| Field | Purpose |
|-------|---------|
| `.name` | CLI string the user types (e.g. `"push-pull"`) |
| `.alias` | Short alias (e.g. `"pp"`) |
| `.label` | T_ key for interactive menu label (`0` = placeholder) |
| `.value` | Integer stored in config when selected |

### 3b: Flag/Option Table

Maps CLI flags to constraints. The array **must** end with a `{ 0 }` sentinel:

```c
static const bp_command_opt_t dummy1_setup_opts[] = {
    { "speed",  's', BP_ARG_REQUIRED, "1-1000",               0, &dummy1_speed_range },
    { "output", 'o', BP_ARG_REQUIRED, "push-pull/open-drain", 0, &dummy1_output_choice },
    { 0 }, // ← sentinel — always required
};
```

| Field | Purpose |
|-------|---------|
| `long_name` | `--speed` on the command line |
| `short_name` | `-s` on the command line |
| `arg_type` | `BP_ARG_REQUIRED` (takes a value) or `BP_ARG_NONE` (boolean flag) |
| `arg_hint` | Shown in help text: `-s <1-1000>` |
| `description` | T_ key for help text (`0` = placeholder) |
| `constraint` | Pointer to a `bp_val_constraint_t` (`NULL` = no auto-validation) |

### 3c: Command Definition

The master struct that ties everything together. **Must be non-static** so it can be exported via the header and wired into `modes[]` as `.setup_def`:

```c
const bp_command_def_t dummy1_setup_def = {
    .name = "dummy1",
    .description = 0,
    .opts = dummy1_setup_opts,
};
```

| Field | Purpose |
|-------|---------|
| `.name` | Matches the protocol name (lowercase) used with the `m` command |
| `.description` | T_ key (`0` = placeholder) |
| `.opts` | Pointer to the flag table |
| `.positionals` | `NULL` — modes use flags, not positional args |
| `.actions` | `NULL` — modes don't have sub-commands |
| `.usage` | `NULL` — auto-generated from opts |

---

## Step 4: The Setup Function

This is the most complex function in a mode. It handles two paths:

- **Interactive** (`m dummy1` with no flags) — load saved settings, offer wizard
- **CLI** (`m dummy1 -s 500 -o od`) — parse flags directly

Returns `1` on success (proceed to `setup_exc`), `0` on cancel or error.

### 4a: Storage Descriptor

Map each config field to a JSON tag for flash persistence:

```c
const char config_file[] = "bpdummy1.bp";
const mode_config_t config_t[] = {
    // clang-format off
    { "$.speed",  &mode_config.speed,  MODE_CONFIG_FORMAT_DECIMAL },
    { "$.output", &mode_config.output, MODE_CONFIG_FORMAT_DECIMAL },
    // clang-format on
};
```

Each `mode_config_t` entry:

| Field | Purpose |
|-------|---------|
| `.tag` | JSON path in the config file (e.g. `"$.speed"`) |
| `.config` | Pointer to the `uint32_t` field in `mode_config` |
| `.formatted_as` | `MODE_CONFIG_FORMAT_DECIMAL` or `MODE_CONFIG_FORMAT_HEXSTRING` |

### 4b: Detect Interactive vs CLI

Check the "primary" flag to determine which path to take:

```c
bp_cmd_status_t st = bp_cmd_flag(&dummy1_setup_def, 's', &mode_config.speed);
if (st == BP_CMD_INVALID) return 0;
bool interactive = (st == BP_CMD_MISSING);
```

`bp_cmd_flag()` return values:

| Status | Meaning |
|--------|---------|
| `BP_CMD_OK` | Flag found and valid — value written to output |
| `BP_CMD_MISSING` | Flag not on command line — constraint default written to output |
| `BP_CMD_INVALID` | Flag present but failed validation — error already printed |

### 4c: Interactive Path — Saved Settings

When no CLI flags are given, first try to load previously saved settings:

```c
if (interactive) {
    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(),
               GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
        dummy1_settings(); // Display the loaded values

        prompt_result result;
        bool user_value;
        if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
            return 0; // User pressed 'x' to exit
        }
        if (user_value) {
            return 1; // User accepted saved settings — skip wizard
        }
    }
```

The flow is:
1. `storage_load_mode()` reads the config file and populates `mode_config.*`
2. Display the loaded values so the user can review them
3. `ui_prompt_bool()` asks "use previous settings? (y/n)"
4. If yes → return immediately, skip the wizard
5. If no → fall through to the interactive wizard

### 4d: Interactive Path — Prompt Wizard

If no saved settings exist, or the user declined them, run the full wizard:

```c
    if (bp_cmd_prompt(&dummy1_speed_range, &mode_config.speed) != BP_CMD_OK) return 0;
    if (bp_cmd_prompt(&dummy1_output_choice, &mode_config.output) != BP_CMD_OK) return 0;
```

`bp_cmd_prompt()` drives an interactive prompt from a `bp_val_constraint_t`:
- **`BP_VAL_UINT32`**: shows `"min-max (default)"` and validates input
- **`BP_VAL_CHOICE`**: shows a numbered menu of named options, accepts name/alias/number
- Returns `BP_CMD_OK` on success, `BP_CMD_EXIT` if user cancels

### 4e: CLI Path

When flags are present, the primary flag is already parsed. Collect remaining flags — missing ones automatically get their constraint default:

```c
} else {
    st = bp_cmd_flag(&dummy1_setup_def, 'o', &mode_config.output);
    if (st == BP_CMD_INVALID) return 0;
}
```

### 4f: Save and Finish

Always save after a successful setup and display the final configuration:

```c
storage_save_mode(config_file, config_t, count_of(config_t));
dummy1_settings();
return 1;
```

---

## Step 5: Hardware Setup and Cleanup

### setup_exc — Hardware Init

Called **after** `setup()` returns successfully. This is where you configure peripherals and claim pins:

```c
uint32_t dummy1_setup_exc(void) {
    // 1. Configure hardware pins / peripherals.
    bio_output(BIO4);
    bio_output(BIO5);
    bio_output(BIO6);
    bio_input(BIO7);

    // 2. Claim IO pins so the Bus Pirate won't let the user manipulate them.
    system_bio_update_purpose_and_label(true, BIO4, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO5, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, BIO6, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO7, BP_PIN_MODE, pin_labels[3]);
    return 1;
}
```

Claimed pins are blocked from PWM/FREQ/etc while the mode is active.

### cleanup — Teardown

Called when the user exits the mode (returns to HiZ):

```c
void dummy1_cleanup(void) {
    // 1. Disable/deinit any hardware you configured.
    bio_init();

    // 2. Release IO pins and clear labels.
    system_bio_update_purpose_and_label(false, BIO4, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO5, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO6, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO7, BP_PIN_MODE, 0);
}
```

---

## Step 6: Settings Display

Shown by the `i` (info) command. Use the standardized display functions so output format matches all other modes:

```c
void dummy1_settings(void) {
    // Numeric setting: label, value, units string (0 for no units)
    ui_prompt_mode_settings_int(
        "Speed",                    // label — use GET_T(T_xxx) for translation
        mode_config.speed,          // current value
        0                           // units string (e.g. GET_T(T_KHZ)) or 0
    );
    // Choice setting: label, selected choice name, units
    const char* output_name = "push-pull";
    for (uint32_t i = 0; i < count_of(dummy1_output_choices); i++) {
        if (dummy1_output_choices[i].value == mode_config.output) {
            output_name = dummy1_output_choices[i].name;
            break;
        }
    }
    ui_prompt_mode_settings_string(
        "Output type",              // label
        output_name,                // current choice name
        0                           // units
    );
}
```

| Function | When to use |
|----------|-------------|
| `ui_prompt_mode_settings_int()` | Numeric values (baud rate, speed, data bits) |
| `ui_prompt_mode_settings_string()` | Choice/enum values (parity, output type, flow control) |

---

## Step 7: Syntax Handlers

These are the bytecode pipeline handlers. **Do not `printf()` in these functions** — set fields on the `result` struct instead.

### Write — User enters a number or string

```c
void dummy1_write(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "--DUMMY1- write()";

    // your code — user data is in result->out_data
    for (uint8_t i = 0; i < 8; i++) {
        bio_put(BIO5, result->out_data & (0b1 << i));
    }

    // example error handling
    static const char err[] = "Halting: 0xff entered";
    if (result->out_data == 0xff) {
        result->error = SERR_ERROR;
        result->error_message = err;
        return;
    }

    result->data_message = message;
}
```

#### Result struct fields

| Field | Purpose |
|-------|---------|
| `result->out_data` | Data value the user entered (up to 32 bits) |
| `result->in_data` | Value to show the user (e.g. SPI read-back) |
| `result->bits` | Bit count config (e.g. `0xff.4` = 4 bits) |
| `result->number_format` | Format used: `df_bin`, `df_hex`, `df_dec`, `df_ascii` |
| `result->data_message` | Text decoration to display (e.g. "ACK", "NACK") |
| `result->error` | Error level (see table below) |
| `result->error_message` | Error text to display |
| `result->repeat` | **Handled by the layer above** — do not implement |

#### Error codes

| Code | Behavior |
|------|----------|
| `SERR_NONE` | No error |
| `SERR_DEBUG` | Display message, continue |
| `SERR_INFO` | Display message, continue |
| `SERR_WARN` | Display message, continue |
| `SERR_ERROR` | Display message, **halt execution** |

### Read — User enters `r`

```c
void dummy1_read(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "--DUMMY1- read()";

    uint32_t data = bio_get(BIO7);

    result->in_data = data;         // put the read value in in_data
    result->data_message = message; // optional text decoration
}
```

### Start / Stop — `[` and `]` keys

```c
void dummy1_start(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-DUMMY1- start()";
    bio_put(BIO4, 1);
    result->data_message = message;
}

void dummy1_stop(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-DUMMY1- stop()";
    bio_put(BIO4, 0);
    result->data_message = message;
}
```

### Full Duplex Start/Stop — `{` and `}` keys

Used by SPI for simultaneous read/write. Most modes leave these as stubs or point `protocol_start_alt` / `protocol_stop_alt` at the regular start/stop in the modes.c registration:

```c
void dummy1_startr(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
}
```

---

## Step 8: Macros

Macros are passed from the command line directly (not through the syntax system). Macro `(0)` is **always** a menu listing available macros:

```c
void dummy1_macro(uint32_t macro) {
    printf("-DUMMY1- macro(%d)\r\n", macro);
    switch (macro) {
        case 0:
            printf(" 0. This menu\r\n 1. Print \"Hello World!\"\r\n");
            break;
        case 1:
            printf("Never gonna give you up\r\n");
            break;
    }
}
```

---

## Step 9: Periodic Service

Called regularly by the main loop. Useful for async polling (e.g. checking for bytes in a UART buffer). Link via `.protocol_periodic` in modes.c — use `noperiodic` if not needed:

```c
void dummy1_periodic(void) {
    static uint32_t cnt;
    if (cnt > 0xffffff) {
        printf("\r\n-DUMMY1- periodic\r\n");
        cnt = 0;
    }
    cnt++;
}
```

---

## Step 10: Bitwise Handlers

Legacy bitbang support for clock/data line manipulation. Most hardware modes don't need these — use `nullfunc1_temp` in your modes.c registration instead.

**Important:** The signature must match the `_mode` struct: `void(struct _bytecode*, struct _bytecode*)`.

```c
void dummy1_clkh(struct _bytecode* result, struct _bytecode* next) {
    (void)result; (void)next;
    // set clock high
}
```

---

## Step 11: Help

Show mode-specific commands when the user presses `?`:

```c
void dummy1_help(void) {
    ui_help_mode_commands(dummy1_commands, dummy1_commands_count);
}
```

---

## Step 12: The Header File

Export everything that `modes.c` needs:

```c
void dummy1_write(struct _bytecode* result, struct _bytecode* next);
void dummy1_read(struct _bytecode* result, struct _bytecode* next);
void dummy1_start(struct _bytecode* result, struct _bytecode* next);
void dummy1_stop(struct _bytecode* result, struct _bytecode* next);

// full duplex start/stop — signature matches _mode struct
void dummy1_startr(struct _bytecode* result, struct _bytecode* next);
void dummy1_stopr(struct _bytecode* result, struct _bytecode* next);

void dummy1_macro(uint32_t macro);
void dummy1_periodic(void);

uint32_t dummy1_setup(void);
uint32_t dummy1_setup_exc(void);
void dummy1_cleanup(void);
void dummy1_settings(void);

// bitwise handlers — signature must match _mode struct
void dummy1_clkh(struct _bytecode* result, struct _bytecode* next);
void dummy1_clkl(struct _bytecode* result, struct _bytecode* next);
void dummy1_dath(struct _bytecode* result, struct _bytecode* next);
void dummy1_datl(struct _bytecode* result, struct _bytecode* next);
void dummy1_dats(struct _bytecode* result, struct _bytecode* next);
void dummy1_clk(struct _bytecode* result, struct _bytecode* next);
void dummy1_bitr(struct _bytecode* result, struct _bytecode* next);

void dummy1_help(void);

extern const struct _mode_command_struct dummy1_commands[];
extern const uint32_t dummy1_commands_count;
extern const struct bp_command_def dummy1_setup_def;
```

Key points:
- All bitwise and syntax handler signatures must be `void(struct _bytecode*, struct _bytecode*)`
- `dummy1_setup_def` must be `extern const` so modes.c can reference it
- `dummy1_commands[]` and `dummy1_commands_count` must be `extern` for the dispatch table

---

## Step 13: Register in modes.c

### Add the include guard

```c
#ifdef BP_USE_DUMMY1
#include "mode/dummy1.h"
#endif
```

### Add the mode entry

Wire every function pointer in the `modes[]` array:

```c
#ifdef BP_USE_DUMMY1
    [DUMMY1] = {
        .protocol_name = "DUMMY1",                       // friendly name (promptname)
        .protocol_start = dummy1_start,                  // start
        .protocol_start_alt = dummy1_start,              // start with read
        .protocol_stop = dummy1_stop,                    // stop
        .protocol_stop_alt = dummy1_stop,                // stop with read
        .protocol_write = dummy1_write,                  // send(/read) max 32 bit
        .protocol_read = dummy1_read,                    // read max 32 bit
        .protocol_clkh = dummy1_clkh,                    // set clk high
        .protocol_clkl = dummy1_clkl,                    // set clk low
        .protocol_dath = dummy1_dath,                    // set dat hi
        .protocol_datl = dummy1_datl,                    // set dat lo
        .protocol_dats = dummy1_dats,                    // toggle dat
        .protocol_tick_clock = dummy1_clk,               // tick clk
        .protocol_bitr = dummy1_bitr,                    // read 1 bit
        .protocol_periodic = dummy1_periodic,            // async polling
        .protocol_macro = dummy1_macro,                  // macro handler
        .protocol_setup = dummy1_setup,                  // setup UI
        .protocol_setup_exc = dummy1_setup_exc,          // hardware init
        .protocol_cleanup = dummy1_cleanup,              // teardown
        .protocol_settings = dummy1_settings,            // display settings
        .protocol_help = dummy1_help,                    // help
        .mode_commands = dummy1_commands,                 // mode-specific commands
        .mode_commands_count = &dummy1_commands_count,    // command count
        .protocol_get_speed = nullfunc7_no_error,        // speed (or implement)
        .setup_def = &dummy1_setup_def,                  // ← enables CLI flags & hints
    },
#endif
```

### Key fields

| Field | Required? | Purpose |
|-------|-----------|---------|
| `.protocol_setup` | **Yes** | Your setup UI function |
| `.protocol_setup_exc` | **Yes** | Hardware init (called after setup succeeds) |
| `.protocol_cleanup` | **Yes** | Teardown on mode exit |
| `.protocol_settings` | **Yes** | Display config for `i` command |
| `.setup_def` | **Yes** | Enables `m mymode -h` help and `m mymode -<Tab>` completion |
| `.protocol_write` / `.protocol_read` | **Yes** | Core IO handlers |
| `.protocol_start` / `.protocol_stop` | **Yes** | `[` / `]` handlers |
| `.protocol_start_alt` / `.protocol_stop_alt` | Use start/stop | `{` / `}` full duplex (point at start/stop if unused) |
| `.protocol_periodic` | Use `noperiodic` | Async polling (or `noperiodic` if not needed) |
| `.protocol_macro` | Use `nullfunc4` | Macro handler (or `nullfunc4` if not needed) |
| `.protocol_clkh` etc. | Use `nullfunc1_temp` | Bitwise ops (or `nullfunc1_temp` stubs) |
| `.mode_commands` | `{ 0 }` table | Mode-specific commands |
| `.protocol_get_speed` | Use `nullfunc7_no_error` | Return current protocol speed |

---

## Quick Reference: `bp_cmd` API

### Constraint-Aware Functions (used in mode setup)

| Function | Signature | Returns |
|----------|-----------|---------|
| `bp_cmd_flag` | `(def, char, &out)` | `bp_cmd_status_t` |
| `bp_cmd_prompt` | `(constraint, &out)` | `bp_cmd_status_t` |

### Status Codes

| Status | Meaning |
|--------|---------|
| `BP_CMD_OK` | Value obtained and valid |
| `BP_CMD_MISSING` | Not on command line (default written for flags) |
| `BP_CMD_INVALID` | Present but failed validation (error printed) |
| `BP_CMD_EXIT` | User cancelled interactive prompt |

### Constraint Types

| Type | Union | Fields |
|------|-------|--------|
| `BP_VAL_UINT32` | `.u` | `.min`, `.max`, `.def` |
| `BP_VAL_INT32` | `.i` | `.min`, `.max`, `.def` |
| `BP_VAL_FLOAT` | `.f` | `.min`, `.max`, `.def` |
| `BP_VAL_CHOICE` | `.choice` | `.choices`, `.count`, `.def` |

---

## Checklist

- [ ] Create `src/mode/mymode.c` with config struct and all handlers
- [ ] Define `bp_val_constraint_t` for each setup parameter
- [ ] Define `bp_command_opt_t[]` flag table (sentinel-terminated with `{ 0 }`)
- [ ] Define `bp_command_def_t` (non-static, exported)
- [ ] Implement `setup()` with dual-path (interactive + CLI)
- [ ] Add saved settings: `storage_load_mode()` / `storage_save_mode()`
- [ ] Implement `setup_exc()` — hardware init and pin claiming
- [ ] Implement `cleanup()` — hardware deinit and pin release
- [ ] Implement `settings()` — `ui_prompt_mode_settings_int/string()`
- [ ] Implement syntax handlers: write, read, start, stop
- [ ] Implement macro handler (at minimum macro `(0)` menu)
- [ ] Create `src/mode/mymode.h` with all declarations and `extern` for def
- [ ] Add `#define BP_USE_MYMODE` in `pirate.h` and mode enum entry
- [ ] Register in `modes.c` — include guard + `modes[]` entry with `.setup_def`
- [ ] Build and verify

---

## Related Documentation

- [bp_cmd_developer_docs_outline.md](bp_cmd_developer_docs_outline.md) — Full `bp_cmd` API reference outline
- [mode_setup_migration_prompt.md](mode_setup_migration_prompt.md) — Migrating existing modes to `bp_cmd`
- [command_setup_migration_prompt.md](command_setup_migration_prompt.md) — Migrating commands to `bp_cmd`
- `src/mode/dummy1.c` — Full reference implementation (copy this as a starting point)
- `src/mode/hwuart.c` — Real-world reference (6 parameters, full saved-settings flow)
