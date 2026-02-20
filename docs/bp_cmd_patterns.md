# bp_cmd Patterns & Recipes

> Quick-reference cookbook of common `bp_cmd` patterns found in the Bus Pirate firmware.

---

## Simple Command (Help + One Flag)

**Source:** `src/commands/uart/monitor.c`

```c
const bp_command_def_t uart_monitor_def = {
    .name        = "monitor",
    .description = T_UART_CMD_TEST,
    .opts        = monitor_opts,
    .usage       = usage,
    .usage_count = count_of(usage),
};

void uart_monitor_handler(struct command_result* res) {
    if (bp_cmd_help_check(&uart_monitor_def, res->help_flag)) return;

    bool pause_toolbar = bp_cmd_find_flag(&uart_monitor_def, 't');
    // ... do work ...
}
```

Minimal def: `name`, `description`, `opts`, `usage`.

---

## Command with Positionals

**Source:** `src/commands/global/w_psu.c`

Try `bp_cmd_positional()` first; if `MISSING` or `INVALID` → fall back to `bp_cmd_prompt()`.

```c
s = bp_cmd_positional(&psucmd_enable_def, 1, &volts);
if (s == BP_CMD_MISSING || s == BP_CMD_INVALID) {
    // not on command line — prompt interactively
    if (bp_cmd_prompt(&voltage_range, &volts) != BP_CMD_OK) {
        res->error = true; return;
    }
    if (bp_cmd_prompt(&current_range, &current) != BP_CMD_OK) {
        res->error = true; return;
    }
} else {
    if (bp_cmd_positional(&psucmd_enable_def, 2, &current) == BP_CMD_INVALID) {
        res->error = true; return;
    }
}
```

---

## Command with Actions/Subcommands

**Source:** `src/commands/spi/flash.c`

Define an enum + action table, then dispatch with `bp_cmd_get_action()`.

```c
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

void flash(struct command_result* res) {
    if (bp_cmd_help_check(&flash_def, res->help_flag)) return;

    uint32_t flash_action = 0;
    if (!bp_cmd_get_action(&flash_def, &flash_action)) {
        bp_cmd_help_show(&flash_def);
        return;
    }
    switch (flash_action) {
        case FLASH_PROBE: /* ... */ break;
        case FLASH_WRITE: /* ... */ break;
        // ...
    }
}
```

---

## Command with Dynamic Verbs (Delegate)

**Source:** `src/ui/ui_mode.c`

Use `bp_action_delegate_t` when the set of verbs is not known at compile time (e.g. loaded from a `modes[]` array).

```c
static const bp_action_delegate_t mode_delegate = {
    .verb_at      = mode_verb_at,
    .match        = mode_match,
    .def_for_verb = mode_def_for_verb,
};

const bp_command_def_t mode_def = {
    .name            = "m",
    .description     = T_CMDLN_MODE,
    .action_delegate = &mode_delegate,
    .usage           = mode_usage,
    .usage_count     = 3,
};
```

| Callback | Purpose |
|----------|---------|
| `verb_at()` | Enumerate verbs from the `modes[]` array |
| `match()` | Resolve a mode name string to an action ID |
| `def_for_verb()` | Return the mode's `setup_def` for contextual help |

---

## Mode Setup (Dual-Path: Wizard + CLI)

**Source:** `src/mode/hwuart.c`

Check one flag to decide interactive vs. CLI path. `BP_CMD_MISSING` means the user gave no flags → run the interactive wizard.

```c
bp_cmd_status_t st = bp_cmd_flag(&uart_setup_def, 'b', &mode_config.baudrate);
if (st == BP_CMD_INVALID) return 0;
bool interactive = (st == BP_CMD_MISSING);

if (interactive) {
    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        // Offer saved settings
    }
    if (bp_cmd_prompt(&uart_baud_range, &mode_config.baudrate) != BP_CMD_OK) return 0;
    if (bp_cmd_prompt(&uart_parity_choice, &temp) != BP_CMD_OK) return 0;
    // ...
} else {
    st = bp_cmd_flag(&uart_setup_def, 'd', &temp);
    if (st == BP_CMD_INVALID) return 0;
    // ...
}
storage_save_mode(config_file, config_t, count_of(config_t));
```

---

## Enable/Disable Command Pairs

**Source:** `src/commands/global/w_psu.c`

Separate defs, separate handlers; shared `usage` strings keep help consistent.

```c
const bp_command_def_t psucmd_enable_def = {
    .name             = "W",
    .description      = T_CMDLN_PSU_EN,
    .opts             = psucmd_opts,
    .positionals      = psucmd_enable_positionals,
    .positional_count = 2,
    .usage            = psucmd_usage,
    .usage_count      = count_of(psucmd_usage),
};

const bp_command_def_t psucmd_disable_def = {
    .name        = "w",
    .description = T_CMDLN_PSU_DIS,
    .usage       = psucmd_usage,
    .usage_count = count_of(psucmd_usage),
};
```

---

## Pattern Summary

| Pattern | Example File | Key APIs |
|---------|-------------|----------|
| Simple flag | `monitor.c` | `bp_cmd_help_check()`, `bp_cmd_find_flag()` |
| Positionals | `w_psu.c` | `bp_cmd_positional()`, `bp_cmd_prompt()` |
| Actions | `flash.c` | `bp_cmd_get_action()`, switch on enum |
| Dynamic verbs | `ui_mode.c` | `bp_action_delegate_t`, `bp_cmd_get_action()` |
| Mode setup | `hwuart.c` | `bp_cmd_flag()`, `bp_cmd_prompt()`, `storage_*` |
| Enable/disable | `w_psu.c` | Separate defs, shared usage |

---

## Related Documentation

- [new_command_guide.md](new_command_guide.md) — Full command implementation guide
- [new_mode_guide.md](new_mode_guide.md) — Full mode implementation guide
- [bp_cmd_data_types.md](bp_cmd_data_types.md) — Type reference
- [bp_cmd_parsing_api.md](bp_cmd_parsing_api.md) — API reference
- [bp_cmd_prompting.md](bp_cmd_prompting.md) — Prompting details
