+++
weight = 90404
title = 'bp_cmd Linenoise Integration'
+++

# bp_cmd Linenoise Integration

> Developer reference for hint generation and tab-completion bridging between `bp_cmd` and the linenoise line-editing library.

---

## Architecture

Three files collaborate to provide interactive hints and completions:

| File | Role |
|------|------|
| `src/lib/bp_args/bp_cmd.h` | Declares `bp_cmd_hint()` and `bp_cmd_completion()` |
| `src/lib/bp_args/bp_cmd.c` | Implements hint/completion logic |
| `src/lib/bp_args/bp_cmd_linenoise.c` | Bridges to linenoise callbacks |

`bp_cmd.c` contains the pure logic (no linenoise dependency). `bp_cmd_linenoise.c` adapts that logic into the callback signatures linenoise expects and handles definition collection from the command/mode tables.

---

## Hint Generation — `bp_cmd_hint()`

```c
const char *bp_cmd_hint(const char *buf, size_t len,
                        const bp_command_def_t *const *defs, size_t count);
```

- Called on every keystroke via linenoise callback
- Walks registered defs to find matching command
- Ghost text suggestions: action verbs, flag names, `<arg_hint>` placeholders
- Returns static buffer, or `NULL` if no hint

---

## Tab-Completion — `bp_cmd_completion()`

```c
typedef void (*bp_cmd_add_completion_fn)(const char *text, void *userdata);

void bp_cmd_completion(const char *buf, size_t len,
                       const bp_command_def_t *const *defs, size_t count,
                       bp_cmd_add_completion_fn add_completion, void *userdata);
```

- Called on Tab press
- Completes: command names, action verbs, flag names (both `-short` and `--long`)
- Sub-definition aware: `m uart -<Tab>` completes UART flags

---

## `collect_defs()` — Building the Definition Array

From `src/lib/bp_args/bp_cmd_linenoise.c`:

```c
#define MAX_COLLECTED_DEFS 48

static size_t collect_defs(const bp_command_def_t *defs[]) {
    size_t n = 0;

    /* Global commands */
    for (uint32_t i = 0; i < commands_count && n < MAX_COLLECTED_DEFS; i++) {
        if (commands[i].def) {
            defs[n++] = commands[i].def;
        }
    }

    /* Current mode commands */
    if (modes[system_config.mode].mode_commands_count) {
        uint32_t mc = *modes[system_config.mode].mode_commands_count;
        const struct _mode_command_struct *mc_arr = modes[system_config.mode].mode_commands;
        for (uint32_t i = 0; i < mc && n < MAX_COLLECTED_DEFS; i++) {
            if (mc_arr[i].def) {
                defs[n++] = mc_arr[i].def;
            }
        }
    }

    return n;
}
```

- Builds flat array from global `commands[]` + current mode's `mode_commands[]`
- Entries with `.def = NULL` (legacy commands) silently skipped
- Max 48 defs — safe tradeoff vs `malloc` on RP2040

---

## Hints Callback

The linenoise hints callback collects definitions, delegates to `bp_cmd_hint()`, and sets display attributes:

```c
#define BP_CMD_HINT_COLOR  95
#define BP_CMD_HINT_BOLD    0

static char *bp_cmd_ln_hints(const char *buf, int *color, int *bold) {
    if (!buf || buf[0] == '\0') return NULL;

    const bp_command_def_t *defs[MAX_COLLECTED_DEFS];
    size_t count = collect_defs(defs);
    if (count == 0) return NULL;

    size_t len = strlen(buf);
    const char *hint = bp_cmd_hint(buf, len, defs, count);
    if (!hint) return NULL;

    *color = BP_CMD_HINT_COLOR;
    *bold  = BP_CMD_HINT_BOLD;
    return (char *)hint;
}
```

---

## Completion Callback

An adapter function translates `bp_cmd_completion()`'s generic callback into linenoise's `linenoiseAddCompletion()`:

```c
static void add_completion_adapter(const char *text, void *userdata) {
    linenoiseCompletions *lc = (linenoiseCompletions *)userdata;
    linenoiseAddCompletion(lc, text);
}

static void bp_cmd_ln_completion(const char *buf, linenoiseCompletions *lc) {
    if (!buf || buf[0] == '\0') return;

    const bp_command_def_t *defs[MAX_COLLECTED_DEFS];
    size_t count = collect_defs(defs);
    if (count == 0) return;

    size_t len = strlen(buf);
    bp_cmd_completion(buf, len, defs, count, add_completion_adapter, lc);
}
```

---

## Initialization

Both callbacks are registered during startup:

```c
void bp_cmd_linenoise_init(void) {
    linenoiseSetHintsCallback(bp_cmd_ln_hints);
    linenoiseSetCompletionCallback(bp_cmd_ln_completion);
}
```

---

## Sub-Definition Awareness

When a command uses `bp_action_delegate_t` with `def_for_verb()`, the hint/completion system resolves the verb's own `bp_command_def_t` for contextual flag hinting. Example: `m uart -b` hints UART baud rate flags because `mode_def_for_verb(HWUART)` returns `&uart_setup_def`.

---

## Hint Appearance

| Setting | Value | Description |
|---------|-------|-------------|
| `BP_CMD_HINT_COLOR` | 95 | ANSI color (95 = bright magenta) |
| `BP_CMD_HINT_BOLD` | 0 | Bold attribute (0 = normal) |

---

## Related Documentation

- [bp_cmd_data_types.md](bp_cmd_data_types.md) — `bp_action_delegate_t` details
- [bp_cmd_parsing_api.md](bp_cmd_parsing_api.md) — API reference
- [new_command_guide.md](new_command_guide.md) — Command registration
- Source: `src/lib/bp_args/bp_cmd_linenoise.c`, `src/lib/bp_args/bp_cmd.h`
