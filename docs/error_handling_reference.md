+++
weight = 90416
title = 'Error Handling Conventions'
+++

# Error Handling Conventions

> Quick reference for error signaling patterns used across the Bus Pirate firmware.

---

## Command-Level Error Signaling

Set `system_config.error = true` to signal failure to the command dispatcher. This controls command chaining operators (`;`, `||`, `&&`). Always `return` immediately after setting the error flag.

```c
system_config.error = true;
return;
```

---

## SERR_* Codes — Bytecode Pipeline Errors

Defined in `src/bytecode.h`. Attached to `result->error` inside syntax write/read handlers.

```c
enum SYNTAX_ERRORS {
    SERR_NONE = 0,
    SERR_DEBUG,
    SERR_INFO,
    SERR_WARN,
    SERR_ERROR
};
```

| Code | Value | Behavior |
|------|-------|----------|
| `SERR_NONE` | 0 | No error |
| `SERR_DEBUG` | 1 | Display message, continue execution |
| `SERR_INFO` | 2 | Display message, continue execution |
| `SERR_WARN` | 3 | Display message, continue execution |
| `SERR_ERROR` | 4 | Display message, **halt execution** |

Usage in a syntax handler:

```c
void dummy1_write(struct _bytecode* result, struct _bytecode* next) {
    if (result->out_data == 0xff) {
        result->error = SERR_ERROR;
        result->error_message = err;
        return;
    }
    result->data_message = message;
}
```

---

## bp_cmd_status_t — Parsing Errors

Defined in `src/lib/bp_args/bp_cmd.h`. Returned by `bp_cmd_positional()`, `bp_cmd_flag()`, and `bp_cmd_prompt()`.

```c
typedef enum {
    BP_CMD_OK = 0,
    BP_CMD_MISSING,
    BP_CMD_INVALID,
    BP_CMD_EXIT,
} bp_cmd_status_t;
```

| Status | Meaning | Action |
|--------|---------|--------|
| `BP_CMD_OK` | Value obtained and valid | Proceed |
| `BP_CMD_MISSING` | Not supplied on command line | Prompt or use default |
| `BP_CMD_INVALID` | Failed validation (error already printed) | Set `system_config.error`, return |
| `BP_CMD_EXIT` | User cancelled prompt | Return without error |

---

## FRESULT — FatFS Error Codes

From FatFS library (`src/fatfs/ff.h`). Common codes:

| Code | Value | Meaning |
|------|-------|---------|
| `FR_OK` | 0 | Success |
| `FR_DISK_ERR` | 1 | Low-level disk error |
| `FR_NOT_READY` | 3 | Drive not ready |
| `FR_NO_FILE` | 4 | File not found |
| `FR_NO_PATH` | 5 | Path not found |
| `FR_DENIED` | 7 | Access denied |
| `FR_EXIST` | 8 | File already exists |

Use `storage_file_error(res)` (declared in `src/pirate/storage.h`) to print a human-readable error message.

---

## Error Handling Patterns

**Command handler:**

```c
void mycmd_handler(struct command_result* res) {
    if (bp_cmd_help_check(&mycmd_def, res->help_flag)) return;

    uint32_t value;
    bp_cmd_status_t st = bp_cmd_flag(&mycmd_def, 'v', &value);
    if (st == BP_CMD_INVALID) {
        system_config.error = true;
        return;
    }
    // ...
}
```

**Mode setup:**

```c
uint32_t mymode_setup(void) {
    bp_cmd_status_t st = bp_cmd_flag(&mymode_setup_def, 's', &mode_config.speed);
    if (st == BP_CMD_INVALID) return 0;  // 0 = setup failed
    // ...
    return 1;  // 1 = setup succeeded
}
```

---

## When to Return vs Continue

| Error Type | Action |
|------------|--------|
| `BP_CMD_INVALID` | Set error, return immediately |
| `BP_CMD_EXIT` | Return without error (user chose to exit) |
| `SERR_ERROR` | Set on result struct, pipeline halts automatically |
| `SERR_WARN` / `SERR_INFO` | Set on result, execution continues |

---

## Related Documentation

- [syntax_bytecode_guide.md](syntax_bytecode_guide.md) — SERR codes in pipeline
- [bp_cmd_parsing_api.md](bp_cmd_parsing_api.md) — bp_cmd_status_t details
- [system_config_reference.md](system_config_reference.md) — `system_config.error`
- [storage_guide.md](storage_guide.md) — FRESULT codes
- Source: `src/bytecode.h`, `src/lib/bp_args/bp_cmd.h`, `src/system_config.h`
