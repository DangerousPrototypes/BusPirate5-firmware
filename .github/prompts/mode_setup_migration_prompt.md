# Mode Setup Migration Prompt

> **Purpose**: Instructions for a Claude Sonnet agent to migrate all Bus Pirate protocol modes from the old `ui_prompt_uint32()` setup system to the new constraint-based `bp_cmd` system.

---

## Context

The Bus Pirate firmware has a mode system where each protocol (UART, SPI, I2C, etc.) has a `setup()` function that collects configuration from the user. The old system uses `ui_prompt_uint32()` with `struct ui_prompt` menus. The new system uses `bp_command_def_t` with `bp_val_constraint_t` constraints and `bp_command_opt_t` flag arrays, enabling both interactive prompts and command-line flag parsing (`m uart -b 115200 -p even`).

**UART (`hwuart.c`) has already been migrated** and serves as the reference implementation. Your job is to migrate every other mode that has interactive setup parameters.

---

## Architecture Overview

### Old System (to be replaced in `setup()`)
```c
// 1. Menu item arrays for display text
static const struct prompt_item speed_menu[] = { { T_SPEED_MENU_1 } };

// 2. ui_prompt structs with range/list config
static const struct ui_prompt my_menu[] = {
    { .description = T_SPEED_MENU,
      .menu_items = speed_menu,
      .menu_items_count = count_of(speed_menu),
      .prompt_text = T_SPEED_PROMPT,
      .minval = 1, .maxval = 1000, .defval = 400,
      .config = &prompt_int_cfg },       // integer range input
    { .description = T_PARITY_MENU,
      .menu_items = parity_menu,
      .menu_items_count = count_of(parity_menu),
      .prompt_text = T_PARITY_PROMPT,
      .minval = 0, .maxval = 0, .defval = 1,
      .config = &prompt_list_cfg },      // numbered list selection
};

// 3. In setup(): sequential interactive prompts
ui_prompt_uint32(&result, &my_menu[0], &config.speed);
if (result.exit) return 0;
ui_prompt_uint32(&result, &my_menu[1], &temp);
if (result.exit) return 0;
config.parity = (uint8_t)(temp - 1);   // list items are 1-based!
```

### New System (constraint-based)
```c
// 1. Constraint definitions
static const bp_val_constraint_t speed_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 1000, .def = 400 },
    .prompt = T_SPEED_MENU,        // reuse existing T_ key
    .hint = T_SPEED_MENU_1,        // reuse menu item as hint text
};

// 2. For list selections → bp_val_choice_t arrays
static const bp_val_choice_t parity_choices[] = {
    { "none", "n", T_PARITY_MENU_1, 0 },
    { "even", "e", T_PARITY_MENU_2, 1 },
    { "odd",  "o", T_PARITY_MENU_3, 2 },
};
static const bp_val_constraint_t parity_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = parity_choices, .count = 3, .def = 0 },
    .prompt = T_PARITY_MENU,
};

// 3. Option/flag table (sentinel-terminated)
static const bp_command_opt_t my_setup_opts[] = {
    { "speed",  's', BP_ARG_REQUIRED, "1-1000",       0, &speed_range },
    { "parity", 'p', BP_ARG_REQUIRED, "none/even/odd", 0, &parity_choice },
    { 0 },  // sentinel
};

// 4. Command definition (exported, non-static)
const bp_command_def_t my_setup_def = {
    .name = "mymode",        // matches modes[].protocol_name (lowercase)
    .description = 0,        // use 0 for T_ placeholder
    .opts = my_setup_opts,
};

// 5. In setup(): dual-path interactive/CLI
bp_cmd_status_t st = bp_cmd_flag(&my_setup_def, 's', &config.speed);
if (st == BP_CMD_INVALID) return 0;
bool interactive = (st == BP_CMD_MISSING);

if (interactive) {
    // offer saved config, then full wizard
    if (bp_cmd_prompt(&speed_range, &config.speed) != BP_CMD_OK) return 0;
    if (bp_cmd_prompt(&parity_choice, &temp) != BP_CMD_OK) return 0;
    config.parity = (uint8_t)temp;
} else {
    // CLI mode — remaining flags use defaults if absent
    st = bp_cmd_flag(&my_setup_def, 'p', &temp);
    if (st == BP_CMD_INVALID) return 0;
    config.parity = (uint8_t)temp;
}
```

---

## Reference Implementation: UART (`src/mode/hwuart.c`)

Study this file thoroughly — it is the canonical example. Key patterns:

### Constraint Definitions
Each setup parameter gets a `static const bp_val_constraint_t`:
- **Integer range** (`BP_VAL_UINT32`): baud rate 1–7372800, data bits 5–8
- **Named choice** (`BP_VAL_CHOICE`): parity (none/even/odd), stop bits (1/2), flow control (off/rts), signal inversion (normal/invert)

### Choice Naming Convention
Each `bp_val_choice_t` entry has:
- `name`: full lowercase word used on command line ("none", "even", "odd")
- `alias`: single-char shorthand ("n", "e", "o"), or NULL if the name is already short enough (e.g. "1", "2")
- `label`: existing `T_` translation key for interactive menu display
- `value`: the integer value stored in the config struct (matches what the old system stored)

### Flag Table
- All flags use `BP_ARG_REQUIRED` (they all take values)
- `arg_hint` is a human-readable value summary: "1-7372800", "5-8", "none/even/odd"
- `description` is `0` (no per-flag description translation key exists yet — use `0` placeholder)
- `constraint` points to the matching `bp_val_constraint_t`
- Short flag chars: pick single meaningful letters that don't collide within the mode

### Setup Function Pattern
```c
uint32_t hwuart_setup(void) {
    // ... periodic_attributes init (keep as-is) ...
    // ... config_file and config_t arrays (keep as-is) ...

    // Detect interactive vs CLI mode by checking the "primary" flag
    bp_cmd_status_t st = bp_cmd_flag(&uart_setup_def, 'b', &mode_config.baudrate);
    if (st == BP_CMD_INVALID) return 0;
    bool interactive = (st == BP_CMD_MISSING);

    if (interactive) {
        // Check saved config and offer to reuse (keep existing logic)
        if (storage_load_mode(...)) {
            // show settings, prompt yes/no
        }
        // Full wizard using bp_cmd_prompt()
        if (bp_cmd_prompt(&uart_baud_range, &mode_config.baudrate) != BP_CMD_OK) return 0;
        if (bp_cmd_prompt(&uart_databits_range, &temp) != BP_CMD_OK) return 0;
        mode_config.data_bits = (uint8_t)temp;
        // ... etc for each parameter ...
    } else {
        // CLI: remaining flags get defaults if absent
        st = bp_cmd_flag(&uart_setup_def, 'd', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.data_bits = (uint8_t)temp;
        // ... etc ...
    }

    storage_save_mode(...);  // keep as-is
    // ... hardware init ...
    return 1;
}
```

### Export Pattern
- The `bp_command_def_t` is **non-static** (no `static` keyword)
- The header declares: `extern const struct bp_command_def mymode_setup_def;`
- The modes.c entry adds: `.setup_def = &mymode_setup_def`

---

## Files to Modify Per Mode

For each mode migration, you will touch exactly 3 files:

### 1. `src/mode/<mode>.c` — The mode implementation
- Add `#include "lib/bp_args/bp_cmd.h"` to includes
- Add constraint definitions (`bp_val_constraint_t`) for each setup parameter
- For list-type parameters: add `bp_val_choice_t` arrays
- Add `bp_command_opt_t` array (sentinel-terminated with `{ 0 }`)
- Add non-static `bp_command_def_t` (named `<mode>_setup_def`)
- Rewrite the `setup()` function using the dual-path pattern
- **Keep**: `storage_load_mode()` / `storage_save_mode()` calls
- **Keep**: `periodic_attributes` init blocks
- **Keep**: all hardware init code after parameter collection
- **Keep**: all `_settings()`, `_help()`, and other non-setup functions untouched
- **Remove**: the old `struct ui_prompt` arrays and `prompt_item` arrays ONLY IF they are not referenced by `_settings()` or other functions. If `_settings()` references `prompt_item` arrays for display text, **keep them**.

### 2. `src/mode/<mode>.h` — The mode header
- Add: `extern const struct bp_command_def <mode>_setup_def;`

### 3. `src/modes.c` — The mode dispatch table
- Add `.setup_def = &<mode>_setup_def` to the mode's entry

---

## Modes to Migrate

### Modes WITH interactive setup parameters (MUST migrate):

#### 1. `hwi2c.c` — I2C Mode
- **Config struct**: `struct _i2c_mode_config` (fields: `baudrate`, `clock_stretch`, `data_bits`)
- **Parameters to convert**:
  - Speed: integer range 1–1000 kHz, default 400 → flag `-s`/`--speed`
  - Clock stretch: on/off list → flag `-c`/`--clockstretch` (choice: off=0, on=1; note old code does `(temp-1)` to convert 1-based list to bool)
  - Data bits: commented out in setup, skip this parameter
- **Config file**: `"bpi2c.bp"`, saves baudrate and clock_stretch
- **Saved config**: Has storage_load_mode pattern with "use previous settings" prompt (keep)
- **Note**: old setup has `-y` flag via `cmdln_args_find_flag('y')` — this can be dropped (the new system handles CLI mode natively)
- **Settings display**: `hwi2c_settings()` uses `ui_prompt_mode_settings_int/string` — does NOT reference prompt_item arrays directly, it uses `GET_T(T_ON)/GET_T(T_OFF)` for clock stretch
- **Old prompt items**: `i2c_clock_stretch_menu[]`, `i2c_data_bits_menu[]`, `i2c_speed_menu[]` — only referenced in setup, can be removed
- **Export as**: `i2c_setup_def` in `hwi2c.h`

#### 2. `hwspi.c` — SPI Mode
- **Config struct**: `struct _spi_mode_config` (fields: `baudrate`, `data_bits`, `clock_polarity`, `clock_phase`, `cs_idle`)
- **Parameters to convert**:
  - Speed: integer range 1–625000 kHz, default 100 → flag `-s`/`--speed` (note: stored as Hz, user enters kHz, `mode_config.baudrate = temp * 1000`)
  - Data bits: integer range 4–8, default 8 → flag `-d`/`--databits`
  - Clock polarity: 2-item list → flag `-o`/`--polarity` (choice: idle low=0, idle high=1; old code does `temp-1`)
  - Clock phase: 2-item list → flag `-a`/`--phase` (choice: leading=0, trailing=1; old code does `temp-1`)
  - CS idle: 2-item list → flag `-c`/`--csidle` (choice: low=0, high=1; old code does `temp-1`)
- **Config file**: `"bpspi.bp"`
- **Important**: Speed is entered in kHz but stored in Hz. In the constraint, set min/max in kHz (1–625000), then multiply by 1000 after parsing. For the CLI path, `bp_cmd_flag` will return the kHz value — multiply afterwards.
- **Settings display**: `spi_settings()` references `spi_polarity_menu[]`, `spi_phase_menu[]`, `spi_idle_menu[]` prompt_item arrays — **KEEP these arrays** for settings display
- **Export as**: `spi_setup_def` in `hwspi.h`

#### 3. `hwhduart.c` — Half-Duplex UART
- **Config struct**: `struct _uart_mode_config` (same struct as hwuart, different static instance)
- **Parameters to convert**:
  - Speed: integer range 1–1000000, default 115200 → flag `-b`/`--baud`
  - Data bits: integer range 5–8, default 8 → flag `-d`/`--databits`
  - Parity: 3-item list (none/even/odd) → flag `-p`/`--parity` (old code does `temp--` to get 0-based)
  - Stop bits: 2-item list → flag `-s`/`--stopbits`
  - Blocking: 2-item list — currently commented out in setup, skip
- **Config file**: `"bphduart.bp"`
- **Settings display**: `hwhduart_settings()` references `uart_parity_menu[]`, `uart_stop_bits_menu[]` — **KEEP these** (check actual settings function)
- **Export as**: `hduart_setup_def` in `hwhduart.h`

#### 4. `hw2wire.c` — 2-Wire Mode
- **Config struct**: `struct _hw2wire_mode_config` (fields: `baudrate`, `data_bits`)
- **Parameters to convert**:
  - Speed: integer range 1–1000 kHz, default 400 → flag `-s`/`--speed`
  - Data bits: commented out in setup, skip
- **Config file**: `"bp2wire.bp"`
- **Settings display**: `hw2wire_settings()` — just shows speed, no prompt_item refs
- **Note**: Very simple — only one parameter
- **Export as**: `hw2wire_setup_def` in `hw2wire.h`

#### 5. `hw3wire.c` — 3-Wire Mode
- **Config struct**: `struct _hw3wire_mode_config` (fields: `baudrate`, `cs_idle`, plus commented-out `data_bits`, `clock_polarity`, `clock_phase`)
- **Parameters to convert** (only the ones actually prompted — others are commented out):
  - Speed: integer range 1–3900 kHz, default 100 → flag `-s`/`--speed` (note: stored as Hz, `mode_config.baudrate = temp * 1000`)
  - CS idle: 2-item list → flag `-c`/`--csidle` (old code does `temp-1`)
- **Config file**: `"bp3wire.bp"` (saves baudrate and cs_idle)
- **Settings display**: `hw3wire_settings()` references `hw3wire_idle_menu[]` — **KEEP this array**
- **Export as**: `hw3wire_setup_def` in `hw3wire.h`

#### 6. `hwled.c` — LED Mode
- **Config struct**: `led_mode_config_t` (fields: `device`, `baudrate`, `num_leds`)
- **Parameters to convert**:
  - Device type: 3-item list (WS2812/APA102/WS2812 Onboard) → flag `-d`/`--device` (old code does `temp--` to convert 1-based to 0-based)
  - Num LEDs: commented out in current setup, skip
- **Config file**: `"bpled.bp"`
- **Settings display**: `hwled_settings()` references `leds_type_menu[]` — **KEEP this array**
- **Export as**: `led_setup_def` in `hwled.h`

#### 7. `infrared.c` — Infrared Mode
- **Config struct**: `struct _infrared_mode_config` (fields: `protocol`, `rx_sensor`, `tx_freq`)
- **Parameters to convert**:
  - Protocol: 3-item list (RAW/NEC/RC5) → flag `-p`/`--protocol` (old code does `temp--`)
  - TX frequency: integer range 20–60 kHz, default 38 → flag `-f`/`--freq` (note: stored as Hz, `mode_config.tx_freq *= 1000`; but only prompted for RAW protocol — other protocols auto-set freq)
  - RX sensor: 3-item list (barrier/38kHz demod/56kHz demod) → flag `-r`/`--rxsensor` (old code does `temp--`)
- **Config file**: `"bpirrxtx.bp"`
- **Special logic**: TX frequency is conditionally prompted — only for RAW protocol. In CLI mode, always accept the flag; in interactive mode, only prompt if protocol is RAW.
- **Settings display**: `infrared_settings()` references `infrared_protocol_menu[]` and `infrared_rx_sensor_menu[]` — **KEEP these arrays**
- **Export as**: `infrared_setup_def` in `infrared.h`

#### 8. `i2s.c` — I2S Audio Mode
- **Config struct**: `struct _i2s_mode_config` (fields: `freq`, `bits`)
- **Parameters to convert**:
  - Sample rate: integer range 4000–96000 Hz, default 44100 → flag `-f`/`--freq`
  - Data bits: integer range 16–16 (fixed), default 16 → flag `-d`/`--databits` (note: currently min==max==16, keep the constraint anyway for future)
- **Config file**: `"bpi2s.bp"`
- **Special logic**: Has a `do/while` loop that validates frequency via `update_pio_frequency()`. In CLI mode, just parse and check once. In interactive mode, loop until valid.
- **Settings display**: `i2s_settings()` — no prompt_item refs, uses `ui_prompt_mode_settings_int` directly
- **Export as**: `i2s_setup_def` in `i2s.h` (note: header file is `src/mode/i2s.h`)

### Modes WITHOUT interactive setup (NO migration needed):
- `hiz.c` — returns 1 immediately, no parameters
- `hw1wire.c` — returns 1 immediately, no parameters
- `dio.c` — returns 1 immediately, no parameters
- `jtag.c` — returns 1 immediately, no parameters
- `usbpd.c` — returns 1 immediately, delegates to I2C internally
- `dummy1.c` — template mode, prints and returns 1 (will be updated separately)

---

## Critical Rules

### DO NOT touch translation files
- Never modify any translation `.h` or `.json` files
- Use `0` as a placeholder for any new `T_` key that doesn't exist yet
- Reuse existing `T_` keys wherever they match (prompts, menu items)

### Value mapping: old 1-based lists → new 0-based choices
The old `prompt_list_cfg` system returns 1-based values. Many modes do `temp - 1` after prompting. In the new system:
- `bp_val_choice_t.value` is the **actual stored value** (0-based)
- `bp_cmd_prompt()` and `bp_cmd_flag()` write the `.value` directly — **no `temp - 1` needed**
- Make sure the choice `.value` fields match what the config struct expects

### Keep `prompt_item` arrays when referenced by `_settings()`
Some modes' `_settings()` functions display the current config using the `prompt_item` arrays:
```c
ui_prompt_mode_settings_string(GET_T(T_MENU), GET_T(some_menu[config.value].description), 0x00);
```
These arrays MUST be kept. Only remove `prompt_item`/`ui_prompt` arrays that are ONLY referenced by the old `setup()` code.

### kHz/Hz conversion
Some modes store speed in Hz but prompt in kHz (SPI: `baudrate = temp * 1000`, 3-Wire: same). In the new system:
- Set the constraint range in the user-facing unit (kHz)
- After `bp_cmd_flag()`/`bp_cmd_prompt()`, multiply by 1000 yourself
- The `arg_hint` in the flag table should reflect the user-facing unit ("1-625000" for kHz)

### Preserve setup logic beyond parameter collection
- Keep `periodic_attributes` initialization
- Keep `storage_load_mode()` / `storage_save_mode()` calls and config arrays
- Keep all hardware initialization code after parameters are collected
- Keep "use previous settings" logic in interactive path
- Keep all non-setup functions completely unchanged

### Flag short names must not collide within a mode
Choose meaningful single characters. Suggested convention:
- `-b` for baud/speed (UART-family)
- `-s` for speed (non-UART) or stopbits
- `-d` for databits or device
- `-p` for parity or protocol
- `-f` for frequency or flow control
- `-c` for clock-stretch or cs-idle
- `-o` for polarity
- `-a` for phase
- `-r` for rx-sensor
- `-i` for invert

### The `_setup_def` name convention
Use `<shortname>_setup_def` where shortname is descriptive:
- `i2c_setup_def`, `spi_setup_def`, `hduart_setup_def`, `hw2wire_setup_def`, `hw3wire_setup_def`, `led_setup_def`, `infrared_setup_def`, `i2s_setup_def`

### The `.name` field of `_setup_def`
Set to the **lowercase version of `protocol_name`** from the modes table. This is used for `m <name> -h` help display:
- `"i2c"`, `"spi"`, `"hduart"`, `"2wire"`, `"3wire"`, `"led"`, `"infrared"`, `"i2s"`

---

## Step-by-Step Per Mode

For each mode in the migration list:

1. **Read the existing `setup()` function** — identify every parameter prompted
2. **Check `_settings()` function** — note which `prompt_item` arrays it references (keep those)
3. **Create constraint definitions** — one `bp_val_constraint_t` per parameter
4. **Create choice arrays** — one `bp_val_choice_t[]` per list-type parameter
5. **Create flag table** — `bp_command_opt_t[]`, sentinel-terminated
6. **Create command def** — non-static `bp_command_def_t`
7. **Rewrite `setup()`** — dual-path pattern (see reference)
8. **Add extern declaration** to header
9. **Wire `.setup_def`** in modes.c entry
10. **Build and verify** — `cmake --build build` must succeed with 0 errors

---

## dummy1.c Update

After all modes are migrated, update `dummy1.c` to serve as a template that demonstrates the new system:

1. Add a sample constraint (e.g. speed 1–1000, default 100)
2. Add a sample choice (e.g. output: push-pull/open-drain)
3. Add a flag table and `bp_command_def_t`
4. Rewrite `dummy1_setup()` with the dual-path pattern
5. Add `extern const struct bp_command_def dummy1_setup_def;` to `dummy1.h`
6. Wire `.setup_def = &dummy1_setup_def` in modes.c

The dummy mode is a teaching example — add clear comments explaining each section.

---

## Build & Verify

After all changes:
```bash
cd /home/ian/bp5fw/build
cmake --build . 2>&1 | tail -20
```

The build must complete with **0 errors**. Warnings are acceptable but errors are not.

The project builds with ninja (the build directory is already configured). Just run the build command above.

---

## Summary Checklist

| Mode | File | Params | Flag chars | Def name | Done? |
|------|------|--------|------------|----------|-------|
| I2C | hwi2c.c | speed, clockstretch | -s, -c | `i2c_setup_def` | ☐ |
| SPI | hwspi.c | speed, bits, polarity, phase, csidle | -s, -d, -o, -a, -c | `spi_setup_def` | ☐ |
| HDUART | hwhduart.c | baud, databits, parity, stopbits | -b, -d, -p, -s | `hduart_setup_def` | ☐ |
| 2WIRE | hw2wire.c | speed | -s | `hw2wire_setup_def` | ☐ |
| 3WIRE | hw3wire.c | speed, csidle | -s, -c | `hw3wire_setup_def` | ☐ |
| LED | hwled.c | device | -d | `led_setup_def` | ☐ |
| IR | infrared.c | protocol, freq, rxsensor | -p, -f, -r | `infrared_setup_def` | ☐ |
| I2S | i2s.c | freq, databits | -f, -d | `i2s_setup_def` | ☐ |
| DUMMY1 | dummy1.c | (template) | -s, -o | `dummy1_setup_def` | ☐ |
