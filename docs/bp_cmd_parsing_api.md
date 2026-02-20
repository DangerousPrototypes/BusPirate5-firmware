# bp_cmd Parsing API

> Stateless command-line parsing and validation API for Bus Pirate commands.

---

## Action Resolution

Match the first non-flag token against the command's `actions[]` array.

```c
bool bp_cmd_get_action(const bp_command_def_t *def, uint32_t *action);
```

| Parameter | Description |
|-----------|-------------|
| `def` | Command definition containing actions array |
| `action` | Output: matched action enum value |
| **Returns** | `true` if action verb found, `false` otherwise |

If the definition has a delegate instead of an actions array, the delegate function is called. Action enum values should start at 1 (0 = no action).

```c
uint32_t action = 0;
if (bp_cmd_get_action(&dummy_def, &action)) {
    printf("Action: %s (enum=%d)\r\n",
           (action == DUMMY_INIT ? "init" : "test"), action);
}
```

## Simple Flag Queries

Stateless functions that re-scan the command line buffer on each call. No validation against constraints.

```c
bool bp_cmd_find_flag(const bp_command_def_t *def, char flag);
bool bp_cmd_get_uint32(const bp_command_def_t *def, char flag, uint32_t *value);
bool bp_cmd_get_int32(const bp_command_def_t *def, char flag, int32_t *value);
bool bp_cmd_get_float(const bp_command_def_t *def, char flag, float *value);
bool bp_cmd_get_string(const bp_command_def_t *def, char flag, char *buf, size_t maxlen);
```

| Function | Returns `true` when |
|----------|---------------------|
| `bp_cmd_find_flag` | Flag is present (no value consumed) |
| `bp_cmd_get_uint32` | Flag found with valid uint32 (supports `0x`, `0b`, decimal) |
| `bp_cmd_get_int32` | Flag found with valid int32 |
| `bp_cmd_get_float` | Flag found with valid float |
| `bp_cmd_get_string` | Flag found with string value copied into `buf` |

All functions accept the `def` to correctly skip other flags' consumed values during scanning.

## Simple Positional Queries

Stateless functions that locate non-flag tokens by index. Flags and their consumed values are skipped automatically using the definition.

```c
bool bp_cmd_get_positional_string(const bp_command_def_t *def, uint32_t pos, char *buf, size_t maxlen);
bool bp_cmd_get_positional_uint32(const bp_command_def_t *def, uint32_t pos, uint32_t *value);
bool bp_cmd_get_positional_int32(const bp_command_def_t *def, uint32_t pos, int32_t *value);
bool bp_cmd_get_positional_float(const bp_command_def_t *def, uint32_t pos, float *value);
```

| Position | Meaning |
|----------|---------|
| 0 | Command name |
| 1 | First user argument |
| 2+ | Subsequent arguments |

## Remainder Access

Raw pointer to everything after the command name. The returned region is **not** null-terminated; use `*len`.

```c
bool bp_cmd_get_remainder(const bp_command_def_t *def, const char **out, size_t *len);
```

Returns `true` if any content exists after the command name.

## Constraint-Aware Resolution

These functions use the `bp_val_constraint_t` attached to the option or positional descriptor for type-aware parsing and range validation.

```c
bp_cmd_status_t bp_cmd_positional(const bp_command_def_t *def, uint32_t pos, void *out);
bp_cmd_status_t bp_cmd_flag(const bp_command_def_t *def, char flag, void *out);
bp_cmd_status_t bp_cmd_prompt(const bp_val_constraint_t *con, void *out);
```

**Behaviour differences:**

- `bp_cmd_flag()` — writes the constraint's default value to `out` when the flag is absent (`BP_CMD_MISSING`).
- `bp_cmd_positional()` — does **not** prompt; returns immediately.
- `bp_cmd_prompt()` — drives an interactive prompt loop using the constraint. Loops on invalid input until the user provides a valid value or cancels.

**Status codes:**

| Status | Meaning |
|--------|---------|
| `BP_CMD_OK` | Value obtained and valid |
| `BP_CMD_MISSING` | Not on command line (default written for flags) |
| `BP_CMD_INVALID` | Present but failed validation (error already printed) |
| `BP_CMD_EXIT` | User cancelled interactive prompt |

## Help System

```c
bool bp_cmd_help_check(const bp_command_def_t *def, bool help_flag);
void bp_cmd_help_show(const bp_command_def_t *def);
```

| Function | Description |
|----------|-------------|
| `bp_cmd_help_check` | If `help_flag` is `true`, displays help and returns `true` (caller should `return`) |
| `bp_cmd_help_show` | Unconditional help display |

Help output is auto-generated from the definition: usage examples, flags table, and actions list.

## Flag Syntax

All flag query functions accept these formats:

| Format | Example |
|--------|---------|
| `-f value` | `-i 123` |
| `-f=value` | `-i=123` |
| `--long value` | `--integer 123` |
| `--long=value` | `--integer=123` |

## Usage Example

From `src/commands/global/dummy.c` — a reference command exercising every API group:

```c
void dummy_handler(struct command_result* res) {
    // 1. Help gate
    if (bp_cmd_help_check(&dummy_def, res->help_flag)) return;

    // 2. Action resolution
    uint32_t action = 0;
    if (bp_cmd_get_action(&dummy_def, &action)) {
        printf("Action: %s (enum=%d)\r\n",
               (action == DUMMY_INIT ? "init" : "test"), action);
    }

    // 3. Boolean flag
    bool b_flag = bp_cmd_find_flag(&dummy_def, 'b');

    // 4. Constraint-aware flag with status handling
    uint32_t value;
    bp_cmd_status_t i_status = bp_cmd_flag(&dummy_def, 'i', &value);
    if (i_status == BP_CMD_INVALID) { system_config.error = true; return; }

    // 5. Interactive prompt fallback
    if (action == DUMMY_INIT && i_status == BP_CMD_MISSING) {
        bp_cmd_status_t prompt_st = bp_cmd_prompt(&integer_range, &value);
        if (prompt_st != BP_CMD_OK) return;
    }

    // 6. String flag
    char file[13];
    bool f_flag = bp_cmd_get_string(&dummy_def, 'f', file, sizeof(file));
}
```

## Function Signature Tables

### Simple Queries

| Function | Returns | Description |
|----------|---------|-------------|
| `bp_cmd_get_action(def, &action)` | `bool` | Match first non-flag token to actions array |
| `bp_cmd_find_flag(def, flag)` | `bool` | Check flag presence (no value) |
| `bp_cmd_get_uint32(def, flag, &val)` | `bool` | Parse flag value as uint32 |
| `bp_cmd_get_int32(def, flag, &val)` | `bool` | Parse flag value as int32 |
| `bp_cmd_get_float(def, flag, &val)` | `bool` | Parse flag value as float |
| `bp_cmd_get_string(def, flag, buf, len)` | `bool` | Copy flag value as string |
| `bp_cmd_get_positional_string(def, pos, buf, len)` | `bool` | Get positional arg as string |
| `bp_cmd_get_positional_uint32(def, pos, &val)` | `bool` | Get positional arg as uint32 |
| `bp_cmd_get_positional_int32(def, pos, &val)` | `bool` | Get positional arg as int32 |
| `bp_cmd_get_positional_float(def, pos, &val)` | `bool` | Get positional arg as float |
| `bp_cmd_get_remainder(def, &out, &len)` | `bool` | Raw pointer past command name |

### Constraint-Aware

| Function | Returns | Description |
|----------|---------|-------------|
| `bp_cmd_positional(def, pos, out)` | `bp_cmd_status_t` | Parse + validate positional arg |
| `bp_cmd_flag(def, flag, out)` | `bp_cmd_status_t` | Parse + validate flag; writes default if absent |
| `bp_cmd_prompt(con, out)` | `bp_cmd_status_t` | Interactive prompt driven by constraint |
| `bp_cmd_help_check(def, help_flag)` | `bool` | Show help if flag set; returns true if shown |
| `bp_cmd_help_show(def)` | `void` | Unconditional help display |

## Related Documentation

- [bp_cmd_data_types.md](bp_cmd_data_types.md) — constraint types and value definitions
- [new_command_guide.md](new_command_guide.md) — step-by-step guide for adding a new command
- [new_mode_guide.md](new_mode_guide.md) — guide for adding a new protocol mode
