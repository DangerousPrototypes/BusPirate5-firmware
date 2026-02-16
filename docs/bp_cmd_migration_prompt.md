# bp_command_def_t Migration Prompt — Global Commands

> Give this document to the LLM along with the target command file.

---

## Task

Migrate a **global** Bus Pirate 5 firmware command from the legacy `ui_help`/`ui_cmdln` system to the new `bp_command_def_t` system. This is a **three-file touch** operation:

1. **Command file** (e.g. `src/commands/global/hex.c`) — rewrite help/arg tables, update handler
2. **Command header** (e.g. `src/commands/global/hex.h`) — add `extern` for the def struct
3. **Registration array** (`src/commands.c`) — wire `.def = &xxx_def`

---

## Architecture Context

Global commands live in `commands[]` in `src/commands.c`. Each entry is a `struct _global_command_struct`:

```c
struct _global_command_struct {
    const char *command;                          // command string (stays)
    void (*func)(struct command_result* res);      // handler (stays)
    const struct bp_command_def *def;              // NULL → needs migration
    uint32_t description_text;                     // legacy (will be deprecated)
    bool allow_hiz;                                // stays
    uint8_t category;                              // stays
};
```

Your job is to populate `.def` for commands where it is currently `NULL`.

**Note:** The `.description_text` field is still present in `_global_command_struct` but is slated for deprecation (already removed from `_mode_command_struct`). During migration, keep the existing `.description_text` value in the registration entry — it will be bulk-removed later. The new `.def->description` will take priority when both exist.

---

## Reference: Completed Migration (hex.c)

Study this as the canonical pattern for a global command with actions and flags.

### File 1 of 3: `src/commands/global/hex.c` (command implementation)

```c
#include "lib/bp_args/bp_cmd.h"    // <-- NEW: replaces ui/ui_cmdln.h

static const char* const usage[] = {
    "hex <file> [-s <start>] [-b <bytes>] [-q(uiet)] [-c (no pager)]",
    "View file in hex:%s hex example.bin",
    /* ... */
};

static const bp_command_opt_t hex_opts[] = {
    { "start",   's', BP_ARG_REQUIRED, "<addr>",  UI_HEX_HELP_START },
    { "bytes",   'b', BP_ARG_REQUIRED, "<count>", UI_HEX_HELP_BYTES },
    { "quiet",   'q', BP_ARG_NONE,     NULL,      UI_HEX_HELP_QUIET },
    { "nopager", 'c', BP_ARG_NONE,     NULL,      T_HELP_DISK_HEX_PAGER_OFF },
    { 0 }
};

const bp_command_def_t hex_def = {
    .name         = "hex",
    .description  = T_CMDLN_HEX,
    .actions      = NULL,
    .action_count = 0,
    .opts         = hex_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void hex_handler(struct command_result* res) {
    if (bp_cmd_help_check(&hex_def, res->help_flag)) return;

    char file[13];
    bool file_flag = bp_cmd_get_positional_string(&hex_def, 1, file, sizeof(file));
    uint32_t start;
    bool has_start = bp_cmd_get_uint32(&hex_def, 's', &start);
    /* ... */
}
```

### File 2 of 3: `src/commands/global/hex.h` (header — add extern)

```c
void hex_handler(struct command_result* res);
extern const struct bp_command_def hex_def;   // Option B: forward-declared struct
```

**Use `extern const struct bp_command_def xxx_def;`** (Option B) — avoids adding `bp_cmd.h` to the header.

### File 3 of 3: `src/commands.c` (registration — wire .def)

```c
{ .command="hex", .allow_hiz=true, .func=&hex_handler, .def=&hex_def, .description_text=T_CMDLN_HEX, .category=CMD_CAT_FILES },
```

---

## Step-by-Step Migration Procedure

### Step 1: Identify legacy patterns in the command file

Look for these OLD patterns that must be replaced:

```c
#include "ui/ui_cmdln.h"

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_HEADING },
    { 0, "verb", T_HELP_VERB },
    { 0, "-f", T_HELP_FLAG },
};

if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
    return;
}

cmdln_args_find_flag('v')
cmdln_args_find_flag_string('f', &arg, sizeof(buf), buf)
cmdln_args_find_flag_uint32('u', &arg, &value)
cmdln_args_string_by_position(1, sizeof(buf), buf)
cmdln_args_float_by_position(1, &value)

// Action dispatch (returns true on FAILURE):
if (cmdln_args_get_action(actions, count_of(actions), &action)) { ... }
```

### Step 2: Create new bp_command_def_t tables

| Old construct | New construct |
|---|---|
| `#include "ui/ui_cmdln.h"` | `#include "lib/bp_args/bp_cmd.h"` |
| `struct cmdln_action_t actions[]` | `bp_command_action_t action_defs[]` — 3 fields: `{ enum, "verb", T_KEY }` |
| `struct ui_help_options options[]` | `bp_command_opt_t opts[]` — 5 fields, `{ 0 }` sentinel |
| `usage[]` | Keep as-is |

**Flags conversion** — extract only `-x` entries from old `options[]`:
```c
// OLD: { 0, "-f", T_HELP_FILE_FLAG },
// NEW: { "file", 'f', BP_ARG_REQUIRED, "<file>", T_HELP_FILE_FLAG },
```

Fields: `long_name`, `short_name`, `arg_type` (`BP_ARG_NONE` or `BP_ARG_REQUIRED`), `arg_hint` (NULL for booleans), `description` (reuse T_ key).

**The def struct** (NOT static):
```c
const bp_command_def_t xxx_def = {
    .name         = "commandname",        // must match .command in commands[]
    .description  = T_HELP_DESCRIPTION,   // from old options[] heading or description_text
    .actions      = xxx_action_defs,      // NULL if none
    .action_count = count_of(...),        // 0 if none
    .opts         = xxx_opts,             // NULL if none
    .usage        = usage,
    .usage_count  = count_of(usage),
};
```

### Step 3: Update the handler function

| Old call | New call |
|---|---|
| `ui_help_show(res->help_flag, usage, count, &options[0], count)` | `bp_cmd_help_check(&xxx_def, res->help_flag)` |
| `cmdln_args_get_action(actions, count, &val)` (**true=fail**) | `bp_cmd_get_action(&xxx_def, &val)` (**true=success — INVERTED!**) |
| `cmdln_args_find_flag('v')` | `bp_cmd_find_flag(&xxx_def, 'v')` |
| `cmdln_args_find_flag_uint32('b', &arg, &val)` | `bp_cmd_get_uint32(&xxx_def, 'b', &val)` |
| `cmdln_args_find_flag_string('f', &arg, size, buf)` | `bp_cmd_get_string(&xxx_def, 'f', buf, size)` |
| `cmdln_args_string_by_position(N, size, buf)` | `bp_cmd_get_positional_string(&xxx_def, N, buf, size)` |
| `cmdln_args_float_by_position(N, &val)` | `bp_cmd_get_positional_float(&xxx_def, N, &val)` |
| `cmdln_args_uint32_by_position(N, &val)` | `bp_cmd_get_positional_uint32(&xxx_def, N, &val)` |

Remove unused `command_var_t` declarations.

### Step 4: Clean up includes

Remove `#include "ui/ui_cmdln.h"` if no `cmdln_args_*` calls remain.

### Step 5: Delete old tables

Delete `struct ui_help_options options[]` and `struct cmdln_action_t xxx_actions[]`. Keep `usage[]`.

### Step 6: Add extern to the header

```c
extern const struct bp_command_def xxx_def;
```

### Step 7: Wire `.def` in `src/commands.c`

Add `.def=&xxx_def` to the matching entry. Keep all other fields unchanged.

### Step 8: Verify — Checklist

- [ ] `#include "lib/bp_args/bp_cmd.h"` added
- [ ] `#include "ui/ui_cmdln.h"` removed
- [ ] Old `options[]` and action arrays deleted
- [ ] `usage[]` kept
- [ ] New action/opts arrays created (if applicable)
- [ ] `const bp_command_def_t xxx_def` is NOT `static`
- [ ] `.def->name` matches `.command` in `commands[]`
- [ ] Handler uses `bp_cmd_help_check()` not `ui_help_show()`
- [ ] `bp_cmd_get_action()` return sense is correct (true=success)
- [ ] All `cmdln_args_*` replaced with `bp_cmd_*`
- [ ] `command_var_t` removed if unused
- [ ] `extern const struct bp_command_def xxx_def;` in header
- [ ] `.def=&xxx_def` wired in `src/commands.c`

---

## API Quick Reference

```c
// Types
typedef struct {
    const char *long_name; char short_name;
    bp_arg_type_t arg_type;  // BP_ARG_NONE | BP_ARG_REQUIRED | BP_ARG_OPTIONAL
    const char *arg_hint; uint32_t description;
} bp_command_opt_t;

typedef struct {
    uint32_t action; const char *verb; uint32_t description;
} bp_command_action_t;

typedef struct bp_command_def {
    const char *name; uint32_t description;
    const bp_command_action_t *actions; uint32_t action_count;
    const bp_command_opt_t *opts;  // { 0 }-terminated
    const char *const *usage; uint32_t usage_count;
} bp_command_def_t;

// Help
bool bp_cmd_help_check(const bp_command_def_t *def, bool help_flag);
void bp_cmd_help_show(const bp_command_def_t *def);

// Actions
bool bp_cmd_get_action(const bp_command_def_t *def, uint32_t *action); // true = found

// Flags
bool bp_cmd_find_flag(const bp_command_def_t *def, char flag);
bool bp_cmd_get_uint32(const bp_command_def_t *def, char flag, uint32_t *value);
bool bp_cmd_get_int32(const bp_command_def_t *def, char flag, int32_t *value);
bool bp_cmd_get_string(const bp_command_def_t *def, char flag, char *buf, size_t max);
bool bp_cmd_get_float(const bp_command_def_t *def, char flag, float *value);

// Positional (pos 0 = command name, pos 1 = first arg)
bool bp_cmd_get_positional_string(const bp_command_def_t *def, uint32_t pos, char *buf, size_t max);
bool bp_cmd_get_positional_uint32(const bp_command_def_t *def, uint32_t pos, uint32_t *value);
bool bp_cmd_get_positional_int32(const bp_command_def_t *def, uint32_t pos, int32_t *value);
bool bp_cmd_get_positional_float(const bp_command_def_t *def, uint32_t pos, float *value);

// Raw remainder
bool bp_cmd_get_remainder(const bp_command_def_t *def, const char **out, size_t *len);
```

---

## Special Cases

- **No actions:** `.actions = NULL`, `.action_count = 0`. Don't call `bp_cmd_get_action()`.
- **No flags:** `.opts = NULL`.
- **Positional-only commands** (e.g. `W 3.3 100`): `.opts = NULL`, use `bp_cmd_get_positional_*()`.
- **Uppercase/lowercase pairs** (`w`/`W`, `p`/`P`): separate entries with separate handler functions. Use separate defs if help text differs.
- **Sanity checks** (`ui_help_check_vout_vref()` etc.): keep as-is, independent of this migration.
