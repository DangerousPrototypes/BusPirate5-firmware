# GUI Toolkit & Reusable Framework — Analysis

## 1. Current State of `eeprom_i2c_gui.c`

**1249 lines.** A working fullscreen GUI for I2C EEPROM operations: menu bar, Tab-cycling config bar, embedded hex editor, file picker integration, inline progress/message rendering. It works, but nearly everything is hardcoded to the I2C EEPROM use case.

### What's in the file (by line count, approximate)

| Section | Lines | Purpose |
|---|---|---|
| Includes, enums, typedefs | 1–115 | Action IDs, focus enum, `eeprom_gui_t` struct, static state |
| I/O wrappers | 115–145 | 6 thin functions over `rx_fifo`/`tx_fifo` |
| GUI UI ops (eeprom_ui_ops) | 145–240 | Progress bar, message, error, warning renderers |
| Confirm destructive popup | 240–290 | Hardcoded red popup box |
| Size formatting | 290–310 | `format_size()`, `action_needs_file()`, `config_ready()` |
| Focus navigation | 310–340 | `focus_next()`/`focus_prev()` with skip logic |
| Screen refresh | 340–480 | **140 lines** — the config bar, separator, hints, hex editor |
| Menu definitions & builders | 480–600 | Action/device/file/options menu structs + builders |
| File picker integration | 600–620 | Thin wrapper around `ui_file_pick()` |
| I2C address popup | 620–780 | **160 lines** — hand-drawn popup with char-by-char input |
| Execute operation | 780–945 | The eeprom_ui_ops-driven execution engine |
| Action dispatch | 945–990 | `gui_dispatch()` — maps menu action IDs to state changes |
| Key handler | 990–1115 | **125 lines** — switch-in-switch spaghetti for field navigation |
| Main entry point + loop | 1115–1249 | Init, arena, hex editor, main loop, cleanup |

### Anti-Patterns Identified

**1. Giant switch-in-switch key handler (125 lines)**
`gui_process_key()` has `switch(key)` → `switch(gui.focused_field)` at every key. Each field (action, device, file, addr, verify, execute, hex) has its own UP/DOWN/ENTER behavior written inline. Adding a field means touching 4+ switch cases.

**2. Hardcoded config bar layout (140 lines)**
`gui_refresh_screen()` hand-draws each `[field v]` bracket widget with per-field VT100 escape sequences. The column positions are implicit (drawn left-to-right with spaces). Adding a field means editing a 140-line function.

**3. Hardcoded I2C address popup (160 lines)**
A bespoke char-by-char input popup with hand-drawn borders, field positioning, and hex parsing. This is exactly what `ui_cmd_menu_pick_number()` already does — but that's a wizard popup, not an alt-screen overlay popup. The duplication is because there's no generic "popup text input" widget.

**4. Hardcoded confirm popup (50 lines)**
`gui_confirm_destructive()` draws an entire red box. Similar to `ui_cmd_menu_confirm()` but rendered as an overlay instead of a wizard step.

**5. Menu builders partially dynamic, partially static**
`action_items[]` is const but `dev_items[]`, `opt_items_buf[]`, `file_menu_item[]` are mutable statics rebuilt every loop iteration. The separator-insertion logic for device categories is 30 lines of manual array manipulation.

**6. Duplicated I/O wrappers**
Every fullscreen app writes the same 6 functions: `read_blocking(char*)`, `read_try(char*)`, `read_key()`, `write(fd, buf, count)`, `write_str(s)`, `write_buf(buf, len)`. They're all trivially identical across `eeprom_i2c_gui.c`, `menu_demo.c`, `ui_file_picker.c`, `ui_cmd_menu.c`.

**7. Duplicated alt-screen lifecycle**
8 lines of boilerplate (`toolbar_draw_prepare` → alt-screen → clear → app → restore) duplicated 7 times across the codebase. `game_engine.c` already extracted this for games but it's not used by non-game apps.

**8. File-static singleton state**
`static eeprom_gui_t gui;` means one GUI at a time, and all functions implicitly reference it. This is acceptable for the single-threaded Core 0 model, but prevents composition (e.g., a GUI launching a sub-GUI).

---

## 2. Existing Reusable Components

Already working and tested:

| Component | File | Lines | Pattern |
|---|---|---|---|
| **vt100_menu** | `src/lib/vt100_menu/` | 800 | Callback I/O, zero alloc, const menu defs |
| **vt100_keys** | `src/lib/vt100_keys/` | 300 | Callback I/O, pushback, pure decode |
| **ui_file_picker** | `src/ui/ui_file_picker.c` | 645 | Standalone/embedded dual mode |
| **ui_cmd_menu** | `src/ui/ui_cmd_menu.c` | 450 | Sequential wizard over vt100_menu |
| **hx (hex editor)** | `src/lib/hx/` | 2000+ | Embeddable with granular API |
| **game_engine** | `src/commands/global/game_engine.c` | 135 | Screen lifecycle + input + timing |
| **eeprom_ui_ops_t** | `src/commands/eeprom/eeprom_base.h` | 15 | Progress/message callback vtable |

---

## 3. Design Goals for the Toolkit

1. **Widget library** — Small, composable, I/O-agnostic widgets (dropdown, spinner, checkbox, text input, progress bar, confirm popup, file picker) that can be placed at any screen position.

2. **Config-bar framework** — A data-driven config bar that replaces the hand-coded `gui_refresh_screen()` and `gui_process_key()` switch nests. Fields defined as const structs; the framework handles Tab-cycling, Up/Down value changes, Enter activation, and rendering.

3. **Fullscreen app scaffold** — Lifecycle management (alt-screen, toolbar interlock, arena allocation, hex editor embedding, menu bar) extracted from the 8-line boilerplate + 130-line main loop.

4. **Memory-operation GUI template** — Reusable "configure + execute + view results" app for any command that reads/writes/dumps/verifies a memory device (I2C EEPROM, SPI EEPROM, 1-Wire EEPROM, SPI Flash, potentially NAND).

---

## 4. Proposed Architecture

### 4.1 Layer Diagram

```
┌─────────────────────────────────────────────────────┐
│          Application (eeprom_i2c_gui, flash_gui)    │  ← ~200 lines each
├─────────────────────────────────────────────────────┤
│      ui_mem_gui  — memory-op GUI template           │  ← NEW (~400 lines)
├─────────────────────────────────────────────────────┤
│      ui_config_bar — data-driven config bar         │  ← NEW (~300 lines)
├──────────────┬──────────────┬───────────────────────┤
│ ui_popup     │ ui_spinner   │ ui_checkbox           │  ← NEW widgets
├──────────────┴──────────────┴───────────────────────┤
│      ui_app  — fullscreen app scaffold              │  ← NEW (~150 lines)
├─────────────────────────────────────────────────────┤
│  vt100_menu  │ vt100_keys │ ui_file_picker │ hx    │  ← EXISTING
└─────────────────────────────────────────────────────┘
```

### 4.2 `ui_app` — Fullscreen App Scaffold

Extracts the duplicated lifecycle into a clean open/close API:

```c
/* ui_app.h */
typedef struct {
    uint8_t rows, cols;
    vt100_key_state_t keys;
    /* Common I/O function pointers, pre-wired to rx/tx FIFOs */
    int (*read_key)(void);
    int (*write_out)(int fd, const void *buf, int count);
    void (*write_str)(const char *s);
    void (*write_buf)(const void *buf, int len);
} ui_app_t;

void ui_app_open(ui_app_t *app);   /* toolbar_draw_prepare, alt-screen, drain, init keys */
void ui_app_close(ui_app_t *app);  /* leave alt-screen, restore toolbar */
void ui_app_drain(void);           /* drain rx FIFO */
```

Every fullscreen app allocates a `ui_app_t` on the stack, calls `ui_app_open()`, runs its loop, calls `ui_app_close()`. No more copy-paste lifecycle boilerplate.

### 4.3 `ui_popup` — Generic Popup Widgets

Replaces the 3 hand-drawn popup implementations (confirm, I2C address, filename) with parameterized widgets:

```c
/* ui_popup.h */
typedef struct {
    int (*write_out)(int fd, const void *buf, int count);
    uint8_t cols, rows;
} ui_popup_io_t;

/* Confirm dialog: centered box with message + y/n prompt.
 * Returns true if confirmed. */
bool ui_popup_confirm(const ui_popup_io_t *io,
                      const char *title,
                      const char *message,
                      uint8_t style);   /* UI_POPUP_WARN, UI_POPUP_DANGER */

/* Text input popup: centered box with prompt + editable field.
 * Returns true if submitted (Enter), false if cancelled (Esc). */
bool ui_popup_text_input(const ui_popup_io_t *io,
                         const char *title,
                         const char *prompt,
                         char *buf, uint8_t buf_size,
                         uint8_t input_flags);  /* UI_INPUT_HEX, UI_INPUT_ALNUM, etc. */

/* Number input popup: wraps ui_popup_text_input with parsing.
 * Accepts hex (0x prefix) or decimal. */
bool ui_popup_number(const ui_popup_io_t *io,
                     const char *title,
                     uint32_t default_val,
                     uint32_t *result);
```

Color palettes:
- `UI_POPUP_WARN` = yellow on red (destructive confirm)  
- `UI_POPUP_INFO` = white on blue (input, neutral)

### 4.4 `ui_config_bar` — Data-Driven Config Bar

The core of the refactoring. Replaces the 140-line `gui_refresh_screen()` config bar and 125-line `gui_process_key()` switch-in-switch.

```c
/* ui_config_bar.h */

/* Field types */
typedef enum {
    UI_FIELD_SPINNER,    /* Up/Down cycles through string options */
    UI_FIELD_CHECKBOX,   /* Toggle with Space/Enter */
    UI_FIELD_TEXT,       /* Enter opens popup input */
    UI_FIELD_NUMBER,     /* Enter opens number popup, Up/Down ±1 */
    UI_FIELD_FILE,       /* Enter opens file picker */
    UI_FIELD_BUTTON,     /* Enter triggers callback */
} ui_field_type_t;

/* A single config-bar field */
typedef struct {
    const char       *label;       /* display label (NULL = use value only) */
    ui_field_type_t   type;
    uint8_t           width;       /* display width in columns */

    /* Type-specific config */
    union {
        struct {                           /* SPINNER */
            const char *const *options;    /* string array */
            uint8_t             count;
            bool                wrap;      /* wrap around at ends? */
        } spinner;
        struct {                           /* CHECKBOX */
            const char *text;              /* e.g. "Vfy" */
        } checkbox;
        struct {                           /* NUMBER */
            uint32_t min, max;
            const char *fmt;               /* e.g. "0x%02X" */
        } number;
        struct {                           /* FILE */
            const char *ext_filter;        /* e.g. "bin", or NULL */
        } file;
        struct {                           /* BUTTON */
            const char *text;              /* e.g. "Execute" */
            bool (*ready)(void *ctx);      /* grey out when false */
        } button;
    };

    /* Value accessors — the framework calls these, no internal state */
    int      (*get_int)(void *ctx);        /* spinner index, checkbox bool, number value */
    void     (*set_int)(void *ctx, int v);
    const char* (*get_str)(void *ctx);     /* file name, custom display */
    void     (*set_str)(void *ctx, const char *v);
    void     (*on_activate)(void *ctx);    /* button press, Enter on non-editable */

    /* Visibility predicate — NULL = always visible */
    bool     (*visible)(void *ctx);
} ui_field_def_t;

/* Config bar state */
typedef struct {
    const ui_field_def_t *fields;
    uint8_t               field_count;
    uint8_t               focused;    /* current field index */
    void                 *ctx;        /* app context passed to all callbacks */
    uint8_t               bar_row;    /* terminal row to draw on */
    uint8_t               cols;       /* terminal width */
} ui_config_bar_t;

void ui_config_bar_init(ui_config_bar_t *bar, const ui_field_def_t *fields,
                        uint8_t count, void *ctx, uint8_t row, uint8_t cols);
void ui_config_bar_draw(ui_config_bar_t *bar, int (*write_out)(int,const void*,int));
bool ui_config_bar_handle_key(ui_config_bar_t *bar, int key);
     /* Returns true if key was consumed */
void ui_config_bar_focus_next(ui_config_bar_t *bar);
void ui_config_bar_focus_prev(ui_config_bar_t *bar);
```

**How this eliminates the switch nests:**

The current 125-line `gui_process_key` with per-field UP/DOWN/ENTER handling becomes:

```c
static void gui_process_key(int key) {
    if (key == VT100_KEY_CTRL_Q) { gui.running = false; return; }
    if (key == VT100_KEY_F10)    { gui.menu_pending = true; return; }
    /* Config bar consumes Tab, arrows, Enter, Space for its fields */
    if (ui_config_bar_handle_key(&gui.config_bar, key)) return;
    /* Remaining keys go to hex editor or app-specific handlers */
    if (gui.hex_ed) { /* forward to hx */ }
}
```

The framework handles:
- **Tab** → cycle to next visible field (skipping hidden ones)
- **Left/Right** → move focus
- **Up/Down** → spinner cycle / number ±1 / checkbox toggle
- **Enter** → activate (button callback, file picker launch, number popup)
- **Space** → checkbox toggle

**How this eliminates the config bar drawing:**

The current 140-line `gui_refresh_screen` config bar section becomes:

```c
ui_config_bar_draw(&gui.config_bar, gui_write_out);
```

The framework renders `[value v]` brackets for spinners, `[xVfy]` for checkboxes, `[0x50]` for numbers, `[Execute]` for buttons, with green/highlighted/dimmed states based on focus and readiness. Each field's `get_int`/`get_str` callback fetches the current display value.

### 4.5 `ui_mem_gui` — Memory Operation GUI Template

The top-level reusable framework for any "configure + execute + view results" memory command:

```c
/* ui_mem_gui.h */

/* Menu definition for the memory GUI.
 * The caller provides these; the framework builds the menu bar. */
typedef struct {
    const char          *title;          /* e.g. "I2C EEPROM", "SPI Flash" */

    /* Fields for the config bar */
    const ui_field_def_t *fields;
    uint8_t               field_count;

    /* Extra menus beyond the standard Options menu (may be NULL) */
    const vt100_menu_def_t *extra_menus;
    uint8_t                 extra_menu_count;

    /* Execute callback — the app's actual operation runner */
    void (*execute)(void *ctx);

    /* Optional: bus-specific pre-checks (e.g. clock stretching) */
    bool (*pre_check)(void *ctx, const char **err_msg);

    /* Hex editor embedding */
    bool  enable_hex_editor;

    /* App context */
    void *ctx;
} ui_mem_gui_config_t;

/* Run the memory GUI. Blocks until user quits.
 * Returns true if any operation was executed. */
bool ui_mem_gui_run(const ui_mem_gui_config_t *config);
```

**What the template provides (shared across all memory GUIs):**
- Alt-screen lifecycle (`ui_app_open`/`ui_app_close`)
- Menu bar with Action + Device + File + Options (standard menus) + extra menus
- Config bar rendering and key handling (`ui_config_bar`)
- Hex editor embedding (optional, via `hx_embed_*`)
- Progress/message/error row rendering (the `gui_op_*` pattern)
- Confirm popup for destructive actions (`ui_popup_confirm`)
- Separator line with context hints
- Content area management (hints when not configured, results after execution)

**What the application provides (bus-specific, ~200 lines):**

```c
/* Example: eeprom_i2c_gui.c after refactoring */

/* App state — much smaller, no rendering concerns */
typedef struct {
    int action, device_idx;
    char file_name[13];
    uint8_t i2c_addr;
    bool verify_flag;
    /* ... */
} i2c_eeprom_app_t;

/* Field definitions — pure data, no code */
static const ui_field_def_t i2c_fields[] = {
    {
        .label = NULL,
        .type = UI_FIELD_SPINNER,
        .width = 8,
        .spinner = { action_names, ACTION_COUNT, true },
        .get_int = get_action_idx,
        .set_int = set_action_idx,
    },
    {
        .label = NULL,
        .type = UI_FIELD_SPINNER,
        .width = 10,
        .spinner = { NULL, 0, true },  /* populated at runtime from device table */
        .get_int = get_device_idx,
        .set_int = set_device_idx,
    },
    {
        .type = UI_FIELD_FILE,
        .width = 14,
        .file = { "bin" },
        .get_str = get_file_name,
        .set_str = set_file_name,
        .visible = file_field_visible,  /* only when action needs a file */
    },
    {
        .type = UI_FIELD_NUMBER,
        .width = 6,
        .number = { 0, 0x7F, "0x%02X" },
        .get_int = get_i2c_addr,
        .set_int = set_i2c_addr,
    },
    {
        .type = UI_FIELD_CHECKBOX,
        .checkbox = { "Vfy" },
        .get_int = get_verify,
        .set_int = set_verify,
    },
    {
        .type = UI_FIELD_BUTTON,
        .button = { "Execute", config_ready },
        .on_activate = do_execute,
    },
};

/* The entire GUI entry point */
bool eeprom_i2c_gui(const struct eeprom_device_t *devices, uint8_t count,
                     struct eeprom_info *args) {
    i2c_eeprom_app_t app = { .i2c_addr = 0x50, .action = -1, .device_idx = -1 };

    ui_mem_gui_config_t config = {
        .title = "I2C EEPROM",
        .fields = i2c_fields,
        .field_count = ARRAY_SIZE(i2c_fields),
        .execute = i2c_execute,
        .pre_check = i2c_pre_check,
        .enable_hex_editor = true,
        .ctx = &app,
    };

    return ui_mem_gui_run(&config);
}
```

From 1249 lines → ~200 lines. The other ~1050 lines live in the shared framework.

### 4.6 How SPI EEPROM GUI Would Work

```c
/* eeprom_spi_gui.c — ~180 lines */

static const ui_field_def_t spi_fields[] = {
    { .type = UI_FIELD_SPINNER, /* action: dump/erase/write/read/verify/test/protect */ },
    { .type = UI_FIELD_SPINNER, /* device: 25X010..25X512 */ },
    { .type = UI_FIELD_FILE,    /* file: *.bin */ },
    { .type = UI_FIELD_CHECKBOX, .checkbox = { "Vfy" } },
    { .type = UI_FIELD_BUTTON,  .button = { "Execute", config_ready } },
    /* No I2C address field — SPI doesn't need one */
    /* Could add: write-protect status indicator */
};

bool eeprom_spi_gui(...) {
    ui_mem_gui_config_t config = {
        .title = "SPI EEPROM",
        .fields = spi_fields,
        .field_count = ARRAY_SIZE(spi_fields),
        .execute = spi_execute,
        .pre_check = spi_check_write_protect,
        .enable_hex_editor = true,
        .ctx = &app,
    };
    return ui_mem_gui_run(&config);
}
```

### 4.7 How SPI Flash GUI Would Work

```c
/* flash_gui.c — ~200 lines */

static const ui_field_def_t flash_fields[] = {
    { .type = UI_FIELD_SPINNER, /* action: probe/dump/erase/write/read/verify/test */ },
    /* No device spinner — Flash uses SFDP auto-detection (probe) */
    { .type = UI_FIELD_FILE,    /* file: *.bin */ },
    { .type = UI_FIELD_NUMBER,  .number = { 0, 0xFFFFFFFF, "0x%08X" }, /* start addr */ },
    { .type = UI_FIELD_NUMBER,  .number = { 0, 0xFFFFFFFF, "%u" },     /* byte count */ },
    { .type = UI_FIELD_CHECKBOX, .checkbox = { "Vfy" } },
    { .type = UI_FIELD_CHECKBOX, .checkbox = { "Erase" } },  /* erase-before-write */
    { .type = UI_FIELD_BUTTON,  .button = { "Execute", config_ready } },
};
```

---

## 5. Design Patterns Applied

### 5.1 Strategy Pattern (GoF)
The `execute`, `pre_check` function pointers in `ui_mem_gui_config_t` are the Strategy pattern. The framework defines the algorithm skeleton (configure → confirm → execute → display result), and the application provides the variant steps.

### 5.2 Template Method Pattern (GoF)
`ui_mem_gui_run()` is a Template Method: it defines the main loop invariant (init → render → input → dispatch → cleanup) while allowing variation through the config struct's callbacks and field definitions. This is the C equivalent of an abstract base class with virtual methods.

### 5.3 Command Pattern (GoF)
Each `ui_field_def_t` encapsulates an interaction command with its own `get`/`set`/`activate` operations and visibility predicate. The config bar iterates over the field array without knowing what each field does — it just calls the interface.

### 5.4 Observer Pattern (lightweight)
The `visible()` predicate on fields acts as a lightweight observer: when the action changes, the file field's visibility check re-evaluates on the next draw cycle without explicit notification wiring.

### 5.5 Immediate-Mode Rendering
Like the existing `gui_refresh_screen()`, the config bar redraws fully each frame. No retained widget tree, no dirty tracking. This is appropriate for the low-frequency interaction model (humans type ~10 keys/sec, full redraw takes <1ms over USB CDC).

### 5.6 Passive View (MVP variant)
The field definitions are pure data (the "model + view config"). The callbacks access the app state directly (no separate model layer). The framework is the presenter that mediates between key events and field state. This is a pragmatic simplification of MVP for embedded C.

---

## 6. Implementation Phases

### Phase A: Foundation Layer (~150 lines)
- Extract `ui_app_open()`/`ui_app_close()` from the duplicated lifecycle boilerplate
- Extract I/O wrapper functions (read_blocking, read_try, read_key, write_out, write_str, write_buf)
- Files: `src/ui/ui_app.h`, `src/ui/ui_app.c`
- Migrate: `menu_demo.c`, `game_engine.c` as validation targets (small, low-risk)

### Phase B: Popup Widgets (~200 lines)
- Extract `ui_popup_confirm()` from `gui_confirm_destructive()` and `ui_cmd_menu_confirm()`
- Extract `ui_popup_text_input()` from `gui_input_i2c_addr()` and `fp_popup_filename()`
- Extract `ui_popup_number()` wrapping text input with `strtoul` parsing
- Files: `src/ui/ui_popup.h`, `src/ui/ui_popup.c`

### Phase C: Config Bar Framework (~300 lines)
- Implement `ui_field_def_t`, `ui_config_bar_t` structs
- Implement `ui_config_bar_draw()` — renders `[value v]` brackets with focus highlighting
- Implement `ui_config_bar_handle_key()` — Tab/Arrow/Enter/Space dispatch by field type
- Integrate popup widgets for NUMBER and FILE field types
- Files: `src/ui/ui_config_bar.h`, `src/ui/ui_config_bar.c`

### Phase D: Memory GUI Template (~400 lines)
- Implement `ui_mem_gui_config_t` and `ui_mem_gui_run()`
- Menu bar integration (Action + Device + File + Options built from field defs)
- Hex editor embedding (optional, via `hx_embed_*`)
- Progress/message/error content area with `eeprom_ui_ops_t` adapter
- Separator line with dynamic context hints
- Files: `src/ui/ui_mem_gui.h`, `src/ui/ui_mem_gui.c`

### Phase E: Migrate I2C EEPROM GUI (~200 lines result)
- Rewrite `eeprom_i2c_gui.c` to use `ui_mem_gui_run()` with I2C-specific field defs
- Verify identical behavior
- Files: `src/commands/eeprom/eeprom_i2c_gui.c` (rewrite)

### Phase F: New GUIs (optional, future)
- `eeprom_spi_gui.c` — SPI EEPROM GUI using same framework
- `eeprom_1wire_gui.c` — 1-Wire EEPROM GUI
- `flash_gui.c` — SPI Flash GUI
- Each ~150-200 lines

---

## 7. Menu Definition via Const Structs

The current `eeprom_i2c_gui.c` has a mix of const and mutable menu definitions:

| Menu | Current | Issues |
|---|---|---|
| Action | `const vt100_menu_item_t[]` | Fine as-is |
| Device | `static vt100_menu_item_t[18]` rebuilt every loop | Wasteful; category separators hardcoded |
| File | `static vt100_menu_item_t[1]` rebuilt every loop | Just launches picker; doesn't need a menu |
| Options | `static vt100_menu_item_t[6]` rebuilt every loop | Checkmarks/disable states need dynamic build |

**Proposed approach for the framework:**

The config-bar fields replace the need for Action/Device/File as dropdown menus. The menu bar becomes simpler:

```
[Action v] [Device v] [File v] [0x50] [xVfy] [Execute]   ← config bar (row 2)
[Options]                                                  ← menu bar (row 1)
```

- **Action, Device** → spinner fields (Up/Down cycles, no dropdown needed)
- **File** → file field (Enter opens picker)
- **Options** → stays as a menu (verify toggle, address input, execute, quit)
- **Device submenu with separators** → the spinner could show a dropdown on Enter for large device lists, built from a structured device table:

```c
typedef struct {
    const char *category_name;  /* "Small (≤2K)", "Medium (≤64K)", "Large (>64K)" */
    const struct eeprom_device_t *devices;
    uint8_t count;
} ui_device_group_t;
```

Or simpler: the spinner field definition could include separator indices:

```c
.spinner = {
    .options = device_names,
    .count = device_count,
    .separators = (uint8_t[]){5, 10},  /* insert separator before index 5 and 10 */
    .separator_count = 2,
}
```

---

## 8. Risk Assessment

| Risk | Impact | Mitigation |
|---|---|---|
| Over-abstraction for embedded context | Code size bloat, harder to debug | Keep widgets pure-C with static dispatch, no vtables beyond what's needed |
| Flash size increase | RP2040 has 16MB, currently at 6.6% | Shared code offsets per-app duplication; net should be neutral or positive |
| RAM increase | Already at 94.6% of 256KB | Config bar state is stack-allocated (~50 bytes); no heap use |
| Breaking working I2C GUI | User-visible regression | Phase E is a rewrite with identical behavior; test before/after |
| Scope creep | Framework becomes a project unto itself | Strict phase gates; each phase is usable standalone |

---

## 9. Summary

The current `eeprom_i2c_gui.c` is a **working prototype** that proved the concept of a fullscreen config-bar GUI with embedded hex editor. It succeeded in validating the UX. But it's a monolith: 1249 lines of interleaved rendering, input handling, popup drawing, and business logic that can't be reused.

The proposed toolkit extracts **5 reusable layers** (`ui_app`, `ui_popup`, `ui_config_bar`, `ui_mem_gui`, plus the existing `vt100_menu`/`vt100_keys`/`ui_file_picker`) that would:

1. **Reduce the I2C EEPROM GUI from ~1250 to ~200 lines** (84% reduction)
2. **Enable SPI EEPROM, 1-Wire EEPROM, and SPI Flash GUIs in ~150-200 lines each** (vs copying 1250 lines)
3. **Eliminate ~30 switch cases** in the key handler via `ui_config_bar_handle_key()`
4. **Eliminate ~140 lines of manual VT100 rendering** via `ui_config_bar_draw()`
5. **Extract 3 popup implementations** into 1 reusable `ui_popup` module
6. **Unify alt-screen lifecycle** across 7+ fullscreen apps
