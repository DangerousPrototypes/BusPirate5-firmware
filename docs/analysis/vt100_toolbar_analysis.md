# VT100 & Toolbar System — Architecture Analysis

## Executive Summary

The current VT100/terminal system grew organically. It has a solid foundational concept (color API, buffered statusbar TX, scroll regions) but suffers from code duplication, scattered raw escape sequences, no centralized toolbar management, and a race condition window during status bar updates. This document maps the current state and proposes a migration path.

---

## 1. Current Architecture Map

### Layer Diagram

```
┌──────────────────────────────────────────────────────────┐
│  Application Code                                        │
│  freq.c  psu.c  ui_info.c  logic_bar.c  ui_cmdln.c      │
│       ↓ (raw \033[, \e[ scattered everywhere)            │
├──────────────────────────────────────────────────────────┤
│  ui_term.c — Color API                                   │
│  ui_term_color_*() → returns static string               │
│  ui_term_color_text/background() → printf directly       │
│  ui_term_color_text_background_buf() → snprintf to buf   │
│  ui_term_cursor_hide/show()                              │
│  (NO cursor positioning, NO erase, NO scroll regions)    │
├──────────────────────────────────────────────────────────┤
│  ui_statusbar.c — 4-line fixed bar at terminal bottom    │
│  Builds VT100 into tx_sb_buf[1024] via snprintf          │
│  Core1 only, uses intercore messaging                    │
├──────────────────────────────────────────────────────────┤
│  logic_bar.c — 10-line LA waveform display               │
│  Direct printf of raw \e[ sequences                      │
│  Own scroll region, own cursor save/restore               │
├──────────────────────────────────────────────────────────┤
│  usb_tx.c — TX State Machine                             │
│  tx_fifo (regular output) — Core0 produces, Core1 drains │
│  tx_sb_buf (statusbar) — Core1 builds & drains           │
│  State: IDLE → STATUSBAR_DELAY → STATUSBAR_TX            │
└──────────────────────────────────────────────────────────┘
```

### Data Flow for Status Bar

```
Core0: ui_statusbar_update_blocking()
  → sends BP_ICM_UPDATE_STATUS_BAR to Core1 (synchronous)

Core1: ui_statusbar_update_from_core1(flags)
  → snprintf VT100 escape sequences into tx_sb_buf[1024]
  → tx_sb_start(len)

Core1: tx_fifo_service() state machine
  → IDLE: drain tx_fifo (regular printf output)
  → STATUSBAR_DELAY: wait one cycle for tx_fifo to empty
  → STATUSBAR_TX: send tx_sb_buf in 64-byte chunks to USB/UART
```

### Data Flow for Logic Bar

```
Core0: logic_bar_draw_frame() / logic_bar_redraw()
  → draw_prepare(): pause statusbar, hide cursor
  → printf raw \e[ sequences directly into tx_fifo
  → draw_release(): unpause statusbar, show cursor

No buffering, no state machine, no interleaving protection.
```

---

## 2. Problem Inventory

### 2.1 Scattered Raw Escape Sequences

**17 files** contain raw VT100 escape codes. Three notations are mixed:

| Style | Files using it |
|-------|---------------|
| `\033[` (octal) | ui_term.c, ui_statusbar.c, ui_info.c, psu.c |
| `\e[` (GCC extension) | logic_bar.c, freq.c |
| `\x1b[` (hex) | linenoise (3rd party only) |

`ui_term.c` provides cursor hide/show and colors, but has **zero helpers** for:
- Cursor positioning (`\033[row;colH`)
- Erase line (`\033[K`) / erase N chars (`\033[nX`)
- Scroll region setting (`\033[top;bottomr`)
- Save/restore cursor (`\0337` / `\0338`)
- Screen flash (`\033[?5h` / `\033[?5l`)
- Move cursor N positions (`\033[nC`, `\033[nB`, `\033[nD`)
- Disable line wrap (`\033[7l`)

Every file that needs these operations rolls its own.

### 2.2 Duplicated Pin Display Code

**Two complete implementations** of the same pin info display exist:

| Function | File | Output method | Used when |
|----------|------|---------------|-----------|
| `ui_info_print_pin_names()` | ui_info.c | Direct `printf` | `v`/`V` command, no statusbar |
| `ui_statusbar_names()` | ui_statusbar.c | `snprintf` to buf | Statusbar active |
| `ui_info_print_pin_labels()` | ui_info.c | Direct `printf` | `v`/`V` command |
| `ui_statusbar_labels()` | ui_statusbar.c | `snprintf` to buf | Statusbar active |
| `ui_info_print_pin_voltage()` | ui_info.c | Direct `printf` | `v`/`V` command |
| `ui_statusbar_value()` | ui_statusbar.c | `snprintf` to buf | Statusbar active |

The data, formatting, and even escape sequences (`\033[8X%d.%s\t`) are identical — only the output method differs.

### 2.3 No Centralized Toolbar Management

Each toolbar independently:
- Calculates its own position: `rows - (height + statusbar*4)` 
- Sets its own scroll region: `\033[1;Nr`
- Pauses/unpauses toolbars: `terminal_toolbar_pause = true/false`
- Hides/shows cursor: `terminal_hide_cursor = true/false`
- Saves/restores cursor position

There's no registry, no arbitration. If two toolbars were active simultaneously, they'd fight over scroll regions.

The logic bar **hardcodes** the statusbar height as `* 4`:
```c
return system_config.terminal_ansi_rows - (height + (system_config.terminal_ansi_statusbar * 4));
```

### 2.4 Status Bar "Blowout" Race

The `STATUSBAR_DELAY` state waits **one cycle** for `tx_fifo` to empty before sending the statusbar buffer. But:

1. Core0 can `printf` between the DELAY check and the actual TX
2. The 64-byte chunked transmission means multi-chunk statusbar updates can be interleaved with Core0 printf output arriving between chunks
3. `draw_prepare()` sets `statusbar_pause`, then does a `busy_wait_ms(1)` — a 1ms window is not a guarantee

The fundamental issue: **there is no mutex or lock between Core0 printf and Core1 statusbar TX**. The SPSC queue is lock-free, but the VT100 cursor-position sequences in tx_sb_buf can get interleaved with regular output, causing screen corruption.

### 2.5 Color Function Boilerplate

Every `ui_term_color_*()` function is an identical switch statement:
```c
char* ui_term_color_THING(void) {
    switch (system_config.terminal_ansi_color) {
        case UI_TERM_256:    return UI_TERM_256_COLOR_CONCAT_TEXT(BP_COLOR_256_THING);
        case UI_TERM_FULL_COLOR: return UI_TERM_FULL_COLOR_CONCAT_TEXT(BP_COLOR_THING);
        case UI_TERM_NO_COLOR:
        default: return "";
    }
}
```

This pattern is repeated **9 times**. It's a lookup table trying to be functions.

### 2.6 Terminal Sizing is Static

`terminal_ansi_rows` and `terminal_ansi_columns` are set once at detection. If the user resizes their terminal, nothing updates. The statusbar and logic bar positions become wrong.

---

## 3. Proposed Architecture

### 3.1 Central VT100 Escape Code API

Add cursor/erase/scroll primitives to `ui_term.c`. All return `char*` or write to buffer, respecting the color mode. These replace every raw escape sequence in the codebase:

```
// Proposed new API surface in ui_term.h

// Cursor movement
uint32_t ui_term_cursor_position(char* buf, size_t len, uint16_t row, uint16_t col);
uint32_t ui_term_cursor_move(char* buf, size_t len, int16_t rows, int16_t cols);
uint32_t ui_term_cursor_save(char* buf, size_t len);
uint32_t ui_term_cursor_restore(char* buf, size_t len);
uint32_t ui_term_cursor_column(char* buf, size_t len, uint16_t col);

// Erase
uint32_t ui_term_erase_line(char* buf, size_t len);           // \033[K
uint32_t ui_term_erase_chars(char* buf, size_t len, uint16_t n); // \033[nX
uint32_t ui_term_clear_screen(char* buf, size_t len);         // \033[2J

// Scroll region
uint32_t ui_term_scroll_region_set(char* buf, size_t len, uint16_t top, uint16_t bottom);
uint32_t ui_term_scroll_region_reset(char* buf, size_t len);

// Line wrap
uint32_t ui_term_line_wrap(char* buf, size_t len, bool enable);

// Screen flash (reverse video)
uint32_t ui_term_screen_flash(char* buf, size_t len, bool enable);

// Window title
uint32_t ui_term_set_title(char* buf, size_t len, const char* title);

// Printf variants for convenience (direct output)
void ui_term_cursor_position_printf(uint16_t row, uint16_t col);
void ui_term_erase_line_printf(void);
// ... etc
```

Every function returns 0 (writes nothing) when `terminal_ansi_color == UI_TERM_NO_COLOR`. This automatically disables VT100 for dumb terminals.

Two variants per function:
- `_buf()` — writes to buffer (for statusbar/toolbar buffered rendering)  
- `_printf()` — direct output (for immediate commands)

### 3.2 Table-Driven Color System

Replace the 9 identical switch functions with a single lookup:

```
typedef struct {
    const char* full_color;    // "\033[38;2;R;G;Bm"
    const char* color_256;     // "\033[38;5;Nm"
} ui_term_color_entry_t;

typedef enum {
    UI_COLOR_RESET = 0,
    UI_COLOR_PROMPT,
    UI_COLOR_INFO,
    UI_COLOR_NOTICE,
    UI_COLOR_WARNING,
    UI_COLOR_ERROR,
    UI_COLOR_NUM_FLOAT,
    UI_COLOR_GREY,
    UI_COLOR_PACMAN,
    UI_COLOR_COUNT
} ui_color_id_t;

// Single function replaces 9:
char* ui_term_color(ui_color_id_t id);

// Keep the old names as inline wrappers during migration:
static inline char* ui_term_color_prompt(void) { return ui_term_color(UI_COLOR_PROMPT); }
```

### 3.3 Toolbar Registry

Central toolbar manager that owns scroll region calculation and lifecycle:

```
#define TOOLBAR_MAX_COUNT 4

typedef struct {
    const char* name;           // "statusbar", "logic_analyzer", "i2c_monitor"...
    uint16_t height;            // lines this toolbar occupies
    bool enabled;               // currently active
    bool visible;               // currently drawn
    
    // Callbacks
    void (*draw)(char* buf, size_t len, uint16_t start_row, uint16_t width);
    void (*update)(char* buf, size_t len, uint16_t start_row, uint16_t width, uint32_t flags);
    void (*destroy)(void);
} toolbar_t;

// Registry API
bool     toolbar_register(toolbar_t* tb);
void     toolbar_unregister(toolbar_t* tb);
void     toolbar_enable(const char* name);
void     toolbar_disable(const char* name);
uint16_t toolbar_total_height(void);          // sum of all enabled toolbar heights
uint16_t toolbar_get_scroll_top(void);        // always 1
uint16_t toolbar_get_scroll_bottom(void);     // rows - total_height
void     toolbar_recalculate_layout(void);    // recalc positions, set scroll region
void     toolbar_redraw_all(void);
```

**Layout rule**: Toolbars stack from the bottom. The statusbar is always last (bottommost). New toolbars go above it.

```
┌─────────────────────────────┐ ← row 1
│ Scrollable command area     │
│                             │
│                             │
├─────────────────────────────┤ ← scroll_bottom = rows - total_toolbar_height
│ [toolbar: logic_analyzer]   │ 10 lines
│ [toolbar: i2c_monitor]      │  3 lines  (future)
│ [toolbar: statusbar]        │  4 lines
└─────────────────────────────┘ ← row = terminal_ansi_rows
```

The scroll region is set **once** by the registry: `\033[1;(scroll_bottom)r`

### 3.4 Unified Pin Display

Merge `ui_info.c` and `ui_statusbar.c` pin rendering into one set of functions that can output to either a buffer or printf:

```
typedef struct {
    char* buf;        // NULL = use printf
    size_t buf_len;
    uint32_t offset;  // current write position in buf
} ui_output_t;

// Unified functions
uint32_t ui_pin_render_names(ui_output_t* out);
uint32_t ui_pin_render_labels(ui_output_t* out);
uint32_t ui_pin_render_values(ui_output_t* out);
```

When `out->buf == NULL`, they `printf`. When a buffer is provided, they `snprintf` into it. One implementation, two output modes.

### 3.5 Safer Status Bar TX

The existing `STATUSBAR_DELAY` state is a start, but needs strengthening:

**Option A (minimal change)**: Add a "quiet window" counter. Instead of 1 cycle, require N consecutive cycles (e.g., 3) with an empty `tx_fifo` before transmitting the statusbar. This reduces the race window significantly.

**Option B (better)**: Add a `tx_fifo_statusbar_lock` flag. When Core1 is about to send the statusbar, set the flag. Core0's `tx_fifo_put()` spins/drops while the flag is set. The flag is cleared after statusbar TX completes. This is effectively a lightweight spinlock scoped only to the statusbar update window.

**Option C (best, more work)**: Build the statusbar content into a separate SPSC queue (or double-buffer). The TX state machine sends from whichever buffer has complete content. Core1 builds the next frame while the previous is being transmitted. No interleaving possible because each buffer is complete before being selected for TX.

### 3.6 Potential Dummy Toolbars (for testing the registry)

While building the enable/disable system, these are useful development/testing toolbars:

1. **Pin Watcher Toolbar** (2-3 lines) — Shows real-time GPIO states as HIGH/LOW with direction arrows. Simpler than the logic analyzer; just reads current pin state.

2. **Command History Toolbar** (2-3 lines) — Shows last 3 commands executed. Useful for Bus Pirate users running macro sequences.

3. **Protocol Decoder Toolbar** (3-4 lines) — When in I2C/SPI/UART mode, shows decoded last-N transactions (address, data, ACK/NAK). Stub version just shows "No transactions".

4. **System Stats Toolbar** (1-2 lines) — Core temperatures, free memory, uptime, USB connection state.

---

## 4. File Impact Matrix

| File | Current Role | Change Needed |
|------|-------------|---------------|
| `ui/ui_term.c` | Color API only | Add cursor/erase/scroll/wrap primitives; table-driven colors |
| `ui/ui_term.h` | Color API header | New enums, new function declarations |
| `ui/ui_statusbar.c` | 4-line fixed bar | Refactor to toolbar_t interface; use ui_term primitives |
| `ui/ui_statusbar.h` | Statusbar header | Minimal change; add toolbar_t registration |
| `toolbars/logic_bar.c` | 10-line LA bar | Refactor to toolbar_t interface; replace all raw `\e[` |
| `toolbars/logic_bar.h` | LA header | Update to toolbar_t API |
| `ui/ui_info.c` | Pin display (v cmd) | Merge with ui_pin_render.c shared implementation |
| `ui/ui_flags.h` | Update flag enum | Possibly extend for toolbar-generic flags |
| `usb_tx.c` | TX state machine | Strengthen STATUSBAR_DELAY; consider double-buffer |
| `commands/global/freq.c` | Freq display | Replace `\e[0K` with ui_term_erase_line_printf() |
| `pirate/psu.c` | PSU error flash | Replace raw screen flash with ui_term_screen_flash() |
| `system_config.h` | Terminal state fields | Add toolbar registry state (or separate struct) |
| **NEW: `ui/ui_toolbar.c`** | — | Central toolbar registry + layout manager |
| **NEW: `ui/ui_toolbar.h`** | — | Toolbar types and API |
| **NEW: `ui/ui_pin_render.c`** | — | Unified pin name/label/voltage rendering |

---

## 5. Migration Order

### Phase 1: VT100 Primitive API (no behavioral change)
1. Add cursor/erase/scroll functions to `ui_term.c` / `ui_term.h`
2. Replace raw escape codes file-by-file, testing each
3. Standardize on `\033[` notation everywhere
4. Table-driven color refactor

### Phase 2: Toolbar Registry (infrastructure)
1. Create `ui_toolbar.c` / `ui_toolbar.h` with registry, layout calc
2. Refactor `ui_statusbar.c` to register as a `toolbar_t`
3. Implement dummy toolbars for testing enable/disable
4. Update `ui_statusbar_init/deinit` to use `toolbar_recalculate_layout()`

### Phase 3: Logic Bar Migration
1. Refactor `logic_bar.c` to register as a `toolbar_t`
2. Use toolbar registry for position calculation instead of `draw_get_position_index()`
3. Verify both statusbar + logic bar work simultaneously

### Phase 4: Unified Pin Rendering
1. Create `ui_pin_render.c` with output-target abstraction
2. Migrate `ui_info.c` to use it (printf mode)
3. Migrate `ui_statusbar.c` to use it (buffer mode)
4. Delete duplicate code

### Phase 5: TX Hardening
1. Implement stronger STATUSBAR_DELAY (multi-cycle or lock)
2. Test under heavy output load (e.g., protocol decoder flood)

---

## 6. Key Constraints

- **No ncurses** — bare VT100 only; this runs on a Pico RP2040 with no OS
- **Dual-core**: Core0 = command processing + printf; Core1 = USB/UART TX + statusbar rendering
- **Lock-free**: SPSC queues are the inter-core primitive; avoid mutexes where possible
- **Memory**: tx_sb_buf is 1024 bytes; must accommodate all toolbar content combined
- **Flash**: Color string literals are stored in flash; table-driven approach should use `const` to stay in flash
- **Backwards compat**: `v`/`V` command must continue to work with statusbar disabled
- **256-color support**: LGPL3 library is optional (`#ifdef ANSI_COLOR_256`); all new code must respect this
