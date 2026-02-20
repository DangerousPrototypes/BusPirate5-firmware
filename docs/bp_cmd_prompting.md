+++
weight = 90403
title = 'bp_cmd Interactive Prompting'
+++

# bp_cmd Interactive Prompting

> Developer reference for driving interactive prompts via `bp_cmd_prompt()` and the dual-path (CLI / interactive) pattern.

---

## bp_cmd_prompt() API

```c
bp_cmd_status_t bp_cmd_prompt(const bp_val_constraint_t *con, void *out);
```

`bp_cmd_prompt()` drives an interactive prompt loop from a constraint descriptor:

| Constraint type  | Behaviour                                                        |
|------------------|------------------------------------------------------------------|
| `BP_VAL_UINT32`  | Shows range display (min–max), validates input, retries on error |
| `BP_VAL_CHOICE`  | Shows numbered menu, accepts name / alias / number               |

**Return values**

| Value           | Meaning                      |
|-----------------|------------------------------|
| `BP_CMD_OK`     | User provided a valid value  |
| `BP_CMD_EXIT`   | User cancelled the prompt    |

---

## Dual-Path Pattern

Check the "primary" argument first. If it is missing, fall back to interactive prompts. This lets every command work both from the command line and as a guided wizard.

### Mode Setup — `src/mode/hwuart.c`

```c
bp_cmd_status_t st = bp_cmd_flag(&uart_setup_def, 'b', &mode_config.baudrate);
if (st == BP_CMD_INVALID) return 0;
bool interactive = (st == BP_CMD_MISSING);

if (interactive) {
    // Try loading saved settings first
    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(),
               GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
        hwuart_settings();
        prompt_result result;
        bool user_value;
        if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
            return 0;
        }
        if (user_value) {
            return 1; // User accepted saved settings
        }
    }

    // Full interactive wizard
    if (bp_cmd_prompt(&uart_baud_range, &mode_config.baudrate) != BP_CMD_OK) return 0;
    if (bp_cmd_prompt(&uart_databits_range, &temp) != BP_CMD_OK) return 0;
    mode_config.data_bits = (uint8_t)temp;
    if (bp_cmd_prompt(&uart_parity_choice, &temp) != BP_CMD_OK) return 0;
    mode_config.parity = (uint8_t)temp;
    // ... remaining prompts ...
} else {
    // CLI mode — remaining flags use defaults if absent
    st = bp_cmd_flag(&uart_setup_def, 'd', &temp);
    if (st == BP_CMD_INVALID) return 0;
    mode_config.data_bits = (uint8_t)temp;
    // ... remaining flags ...
}
storage_save_mode(config_file, config_t, count_of(config_t));
```

### Command with Positionals — `src/commands/global/w_psu.c`

```c
// Get voltage from command line
s = bp_cmd_positional(&psucmd_enable_def, 1, &volts);
if (s == BP_CMD_MISSING || s == BP_CMD_INVALID) {
    // Not on command line — prompt interactively
    if (bp_cmd_prompt(&voltage_range, &volts) != BP_CMD_OK) {
        res->error = true;
        return;
    }
    if (bp_cmd_prompt(&current_range, &current) != BP_CMD_OK) {
        res->error = true;
        return;
    }
} else {
    // Voltage on command line — try current too
    if (bp_cmd_positional(&psucmd_enable_def, 2, &current) == BP_CMD_INVALID) {
        res->error = true;
        return;
    }
}
```

---

## BP_VAL_UINT32 Prompts

Define a numeric range constraint and pass it to `bp_cmd_prompt()`:

```c
static const bp_val_constraint_t uart_baud_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 7372800, .def = 115200 },
    .prompt = T_UART_SPEED_MENU,
    .hint = T_UART_SPEED_MENU_1,
};
```

**Prompt flow:** prompt text → range (min–max) → default → validate input → loop on error.

---

## BP_VAL_CHOICE Prompts

Define a choice array and a constraint that references it:

```c
static const bp_val_choice_t parity_choices[] = {
    { "none", "n", T_UART_PARITY_MENU_1, 0 },
    { "even", "e", T_UART_PARITY_MENU_2, 1 },
    { "odd",  "o", T_UART_PARITY_MENU_3, 2 },
};
static const bp_val_constraint_t uart_parity_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = parity_choices, .count = 3, .def = 0 },
    .prompt = T_UART_PARITY_MENU,
};
```

**Prompt flow:** numbered menu with labels → accepts name / alias / number → validate.

---

## Saved Configuration Integration

Modes combine `bp_cmd_prompt()` with the storage subsystem so users can reuse previous settings:

1. `storage_load_mode()` — read config from flash.
2. Display the loaded values to the user.
3. `ui_prompt_bool()` — ask "Use previous settings?"
4. If **yes** → skip the wizard and return immediately.
5. If **no** → run the full prompt sequence.
6. `storage_save_mode()` — persist the new settings after setup completes.

---

## Related Documentation

- [new_mode_guide.md](new_mode_guide.md) — Step 4: The Setup Function
- [new_command_guide.md](new_command_guide.md) — Step 7f: Interactive Prompt Fallback
- [bp_cmd_data_types.md](bp_cmd_data_types.md) — Constraint types
- [bp_cmd_parsing_api.md](bp_cmd_parsing_api.md) — Parsing functions
- [storage_guide.md](storage_guide.md) — Saved settings
- Source: `src/lib/bp_args/bp_cmd.h`, `src/mode/hwuart.c`, `src/commands/global/w_psu.c`
