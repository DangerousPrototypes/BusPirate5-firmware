# Unified Buffer Builder: Eliminating the Dual-Path Pin Rendering — Agent Guidance

## Purpose of this document

You are refactoring `ui_pin_render.c` so that all pin rendering (names, labels, voltages) **always writes into a caller-provided buffer** — never calls `printf` directly. This eliminates the pervasive `if (buf) { snprintf... } else { printf... }` branching that currently plagues every function. Both Core0 (the `v`/`V` command) and Core1 (the statusbar) call the same unified builder, each passing their own buffer. Each caller then sends that buffer to the terminal through its own TX mechanism.

Read the companion document `monitor_numeric_snapshot_prompt.md` for the planned monitor refactor. Your changes should be **compatible with** that direction — avoid introducing new dependencies on the current monitor API that would need to be undone later.

## Why the dual-path exists today

The RP2040 has two cores with separate TX mechanisms:

| | Core0 (commands) | Core1 (USB service loop) |
|---|---|---|
| **TX path** | `printf()` → `_putchar()` → `tx_fifo_put()` (SPSC ring buffer, 1024 bytes, one byte at a time) | `snprintf()` → `tx_sb_buf[1024]` (flat buffer), then `tx_sb_start(len)` triggers the state machine |
| **Constraint** | `tx_fifo_put()` asserts `BP_ASSERT_CORE0()` — only Core0 can use printf | `tx_sb_start()` asserts `BP_ASSERT_CORE1()` — only Core1 can mark the statusbar buffer ready |
| **When it runs** | On-demand when user types `v` or `V` | Periodically in `core1_infinite_loop()` after `monitor()` detects changes |

The previous refactor created `ui_pin_render.c` with `out_str()` / `out_fmt()` helper functions that branch on `buf != NULL`. Every cell in every row has an `if (buf)` / `else` fork. This works but doubles the code paths and makes bugs hard to trace (we've already fixed ~6 bugs caused by path divergence).

## The unified design

### Core idea

**Every rendering function takes `(char* buf, size_t buf_len, uint32_t flags)` and always writes into `buf` via `snprintf`.** There is no printf path. The `out_str()` / `out_fmt()` / `vprintf()` helpers are deleted.

The caller is responsible for:
1. Providing a buffer
2. Sending that buffer to the terminal after the builder returns

### Flags (switches for per-use customization)

```c
typedef enum {
    PIN_RENDER_CHANGE_TRACK  = (1u << 0),  // only write cells that changed; emit \t for unchanged
    PIN_RENDER_NEWLINE       = (1u << 1),  // append \r\n at end of each row
    PIN_RENDER_CLEAR_CELLS   = (1u << 2),  // prepend \033[8X before each cell (erase 8 columns)
} pin_render_flags_t;
```

| Flag | Statusbar (Core1) | `v`/`V` command (Core0) |
|---|---|---|
| `CHANGE_TRACK` | ✅ Yes — skip unchanged cells to minimize TX bytes | ❌ No — always repaint everything |
| `NEWLINE` | ❌ No — cursor positioning done externally | ✅ Yes — needs `\r\n` to advance lines |
| `CLEAR_CELLS` | ✅ Yes — cell width can change (e.g. freq value) | ✅ Yes — same reason |

### Function signatures

```c
// Render pin names row:  "1.IO0\t2.IO1\t..."
uint32_t ui_pin_render_names(char* buf, size_t buf_len, pin_render_flags_t flags);

// Render pin labels row:  "SDA\tSCK\t..."  (includes current mA for VOUT pin)
uint32_t ui_pin_render_labels(char* buf, size_t buf_len, pin_render_flags_t flags);

// Render pin values row:  "3.3V\t0.0V\t10.0kHz\t..."
uint32_t ui_pin_render_values(char* buf, size_t buf_len, pin_render_flags_t flags);
```

All return bytes written. If `CHANGE_TRACK` is set and nothing changed, `ui_pin_render_values` returns 0.

### How each caller uses it

**Core0 — `v` command (single measurement):**
```c
void ui_info_print_pin_names(void) {
    char tmp[256];
    uint32_t len;
    pin_render_flags_t flags = PIN_RENDER_NEWLINE | PIN_RENDER_CLEAR_CELLS;
    
    len = ui_pin_render_names(tmp, sizeof(tmp), flags);
    tx_buf_write(tmp, len);  // push through tx_fifo (see below)
}
```

**Core0 — `V` command (continuous refresh):**
Same as above but called in a loop. No `CHANGE_TRACK` — always full repaint because the `v` command doesn't maintain a shadow/diff state.

**Core1 — statusbar:**
```c
void ui_statusbar_update_from_core1(uint32_t update_flags) {
    uint32_t len = 0;
    pin_render_flags_t sb_flags = PIN_RENDER_CHANGE_TRACK | PIN_RENDER_CLEAR_CELLS;
    
    // ... cursor save, position to start_row, etc. (written directly into tx_sb_buf) ...
    
    if (update_flags & UI_UPDATE_NAMES) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\033[%d;0H", start_row + 1);
        len += ui_pin_render_names(&tx_sb_buf[len], buffLen - len, sb_flags);
    }
    if (update_flags & UI_UPDATE_LABELS) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\033[%d;0H", start_row + 2);
        len += ui_pin_render_labels(&tx_sb_buf[len], buffLen - len, sb_flags);
    }
    if (update_flags & UI_UPDATE_VOLTAGES) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\033[%d;0H", start_row + 3);
        len += ui_pin_render_values(&tx_sb_buf[len], buffLen - len, sb_flags);
    }
    
    // ... cursor restore, etc. ...
    tx_sb_start(len);
}
```

### Core0 buffer-send helper

Core0 currently sends output byte-by-byte through `tx_fifo_put()` (via `_putchar`). To send a pre-built buffer, add a small helper:

```c
// usb_tx.c — new function
void tx_fifo_write(const char* buf, uint32_t len) {
    BP_ASSERT_CORE0();
    for (uint32_t i = 0; i < len; i++) {
        spsc_queue_add_blocking(&tx_fifo, (uint8_t)buf[i]);
    }
}
```

This is functionally equivalent to `printf("%s", buf)` but avoids re-parsing format strings. The thin `ui_info_print_pin_*()` wrappers in `ui_info.c` would use this.

### Data sources — what the builder reads

This is an important detail. Today the two paths read different data:

| Data | Current printf path (Core0) | Current buffer path (Core1) |
|---|---|---|
| **Voltages** | `hw_pin_voltage_ordered[i]` directly (raw ADC, fresh from `amux_sweep()`) | `monitor_get_voltage_ptr(i, &c)` (pre-formatted ASCII from `system_monitor.c`) |
| **Current mA** | Computed from `hw_adc_raw[HW_ADC_CURRENT_SENSE]` with `(raw >> 1) * (500000/2048)` | `monitor_get_current_ptr(&c)` (pre-formatted ASCII) |
| **Freq/PWM** | `freq_display_hz()` directly | Same — `freq_display_hz()` |

**For the unified builder, always read from raw data sources:**
- Voltages: `*hw_pin_voltage_ordered[i]` — millivolts, format as `X.YV`
- Current: `hw_adc_raw[HW_ADC_CURRENT_SENSE]` — compute milliamps, format as `NNN.NmA`  
- Freq/PWM: `freq_display_hz()` — already shared

This means the builder no longer calls `monitor_get_voltage_ptr()` or `monitor_get_current_ptr()`. The monitor's pre-formatted ASCII strings become unused by VT100 rendering (the LCD can keep using them for now, as described in the companion prompt).

**Freshness:** The caller must ensure `amux_sweep()` has been called before invoking the builder. Core0 calls it explicitly. Core1 gets it from `monitor()` which calls `amux_sweep()` internally. This is already the case today.

### Change tracking without the monitor

When `PIN_RENDER_CHANGE_TRACK` is set, the builder needs to know "did this pin's voltage change since last paint?" Currently this is delegated to `monitor_get_voltage_ptr()` which gates on `voltages_update_mask`.

**New approach — builder-owned shadow buffer:**

```c
// static inside ui_pin_render.c
static uint16_t shadow_voltage_mv[HW_PINS];
static uint32_t shadow_current_raw;

// In ui_pin_render_values(), when CHANGE_TRACK is set:
uint16_t mv = *hw_pin_voltage_ordered[i];
if (flags & PIN_RENDER_CHANGE_TRACK) {
    if (mv == shadow_voltage_mv[i] && !pin_changed) {
        len += snprintf(buf + len, buf_len - len, "\t");  // skip — no change
        continue;
    }
    shadow_voltage_mv[i] = mv;
}
// format and write the cell
len += snprintf(buf + len, buf_len - len, ...);
```

**Important concurrency note:** This shadow buffer is only written by Core1 (the statusbar is the only caller with `CHANGE_TRACK`). Core0 never sets `CHANGE_TRACK`, so it never reads or writes the shadow. No lock needed — but document this constraint clearly. If someday both cores need change tracking, each would need its own shadow (pass a pointer to the shadow struct, or use two separate instances).

> **Alternative:** If the monitor_numeric_snapshot refactor happens first, the snapshot's `generation` counter could replace the shadow entirely — the builder compares `snapshot->pin_voltage_mv[i]` against its own shadow. Same principle, cleaner data source.

## Files to modify

### `src/ui/ui_pin_render.h` — interface changes

- Add `pin_render_flags_t` enum
- Update function signatures to take `flags` parameter
- Remove `NULL`-buffer documentation (buffer is now always required)

### `src/ui/ui_pin_render.c` — the main refactor

1. **Delete `out_str()` and `out_fmt()`** — everything is now `snprintf` into `buf`
2. **Delete all `if (buf) / else` branches** — there is only one path
3. **Add shadow buffers** for change tracking (`static uint16_t shadow_voltage_mv[HW_PINS]`, etc.)
4. **Read raw data directly:**
   - Voltages from `*hw_pin_voltage_ordered[i]`
   - Current from `hw_adc_raw[HW_ADC_CURRENT_SENSE]`
   - Freq from `freq_display_hz()`
5. **Gate on flags:**
   - `PIN_RENDER_CHANGE_TRACK`: compare against shadow, emit `\t` for unchanged cells
   - `PIN_RENDER_NEWLINE`: append `\r\n` at end of row
   - `PIN_RENDER_CLEAR_CELLS`: prepend `\033[8X` before each cell
6. **All color escapes go through `snprintf`** using `ui_term_color_*_buf()` variants (or inline the escape strings — they're just `const char*` returns from `ui_term_color_num_float()`, `ui_term_color_reset()`, etc.)

### `src/ui/ui_info.c` — thin wrappers for Core0

Update the three wrappers to:
1. Declare a local `char tmp[256]` (or larger if needed — measure the actual max output)
2. Call the unified builder with appropriate flags
3. Send the buffer via `tx_fifo_write(tmp, len)` (new helper) or iterate with `tx_fifo_put`

```c
void ui_info_print_pin_names(void) {
    char tmp[256];
    uint32_t len = ui_pin_render_names(tmp, sizeof(tmp),
                                        PIN_RENDER_NEWLINE | PIN_RENDER_CLEAR_CELLS);
    tx_fifo_write(tmp, len);
}

void ui_info_print_pin_labels(void) {
    char tmp[256];
    uint32_t len = ui_pin_render_labels(tmp, sizeof(tmp),
                                         PIN_RENDER_NEWLINE | PIN_RENDER_CLEAR_CELLS);
    tx_fifo_write(tmp, len);
}

void ui_info_print_pin_voltage(bool refresh) {
    char tmp[256];
    pin_render_flags_t flags = PIN_RENDER_CLEAR_CELLS;
    if (!refresh) flags |= PIN_RENDER_NEWLINE;
    uint32_t len = ui_pin_render_values(tmp, sizeof(tmp), flags);
    tx_fifo_write(tmp, len);
}
```

### `src/ui/ui_statusbar.c` — minimal changes

- Remove the special `UI_UPDATE_CURRENT` block that calls `monitor_get_current_ptr()` directly — current is now handled inside `ui_pin_render_labels()` and `ui_pin_render_values()` uniformly
- Pass `PIN_RENDER_CHANGE_TRACK | PIN_RENDER_CLEAR_CELLS` (no `PIN_RENDER_NEWLINE`) to the builder functions
- Everything else stays the same — cursor positioning, save/restore, `tx_sb_start()` are statusbar-specific framing

### `src/usb_tx.c` / `src/usb_tx.h` — new helper

Add `tx_fifo_write(const char* buf, uint32_t len)` — a simple loop that calls `spsc_queue_add_blocking` for each byte. Asserts `BP_ASSERT_CORE0()`.

### `src/system_monitor.c` / `src/system_monitor.h` — no changes needed now

The builder stops calling `monitor_get_voltage_ptr()` and `monitor_get_current_ptr()`. These functions remain for the LCD's use (`ui_lcd.c`) but are no longer called from VT100 rendering code. The `monitor()` function still runs on Core1 and still calls `amux_sweep()` — that's what keeps `hw_pin_voltage_ordered[]` fresh for the statusbar builder.

## Concurrency model

```
Core0                                          Core1
─────                                          ─────
user types 'v'                                 core1_infinite_loop()
  │                                              │
  ├─ amux_sweep()                                ├─ monitor()
  │    (reads ADC hw)                            │    ├─ amux_sweep()
  │                                              │    └─ (updates voltages_update_mask — unused by VT100 now)
  ├─ ui_pin_render_values(                       │
  │     tmp, sizeof(tmp),                        ├─ ui_pin_render_values(
  │     PIN_RENDER_NEWLINE|CLEAR_CELLS)          │     &tx_sb_buf[len], buffLen-len,
  │                                              │     PIN_RENDER_CHANGE_TRACK|CLEAR_CELLS)
  ├─ tx_fifo_write(tmp, len)                     │
  │    (bytes → tx_fifo SPSC → Core1 drains)     ├─ tx_sb_start(len)
  │                                              │    (tx_sb_buf → USB in STATUSBAR_TX state)
  ▼                                              ▼
```

**Both can be active simultaneously.** The `v` command and statusbar write into separate buffers (`tmp` on Core0's stack vs `tx_sb_buf` global for Core1). They read the same underlying ADC data (`hw_pin_voltage_ordered[]`), which is safe because `amux_sweep()` is a hardware read + cache — no write races on the cached values since each core calls its own sweep.

**The shadow buffer** (`shadow_voltage_mv[]` etc.) is only touched when `PIN_RENDER_CHANGE_TRACK` is set, and only the statusbar uses that flag. If this constraint changes in the future, the shadow should be passed as a pointer parameter rather than kept as a static.

## What NOT to do

1. **Don't keep any `printf` / `vprintf` calls in `ui_pin_render.c`.** Everything goes into the buffer.
2. **Don't add `if (buf)` branches.** The `buf` is always non-NULL now.
3. **Don't call `monitor_get_voltage_ptr()` or `monitor_get_current_ptr()`** from the builder. Read raw data and format it yourself.
4. **Don't worry about the LCD.** `ui_lcd.c` has its own rendering path using `monitor_get_voltage_char()` and is not part of this refactor.
5. **Don't change `v_adc.c` directly** — it calls through `ui_info_print_pin_*()` wrappers which are the adaptation layer. The wrappers change, the command doesn't.
6. **Don't put cursor-positioning escapes** (like `\033[row;colH`) inside the builder. Those are the caller's responsibility. The builder only renders cell content with tabs as column separators.

## Buffer sizing

Current `tx_sb_buf` is 1024 bytes. Worst-case per row (8 pins):
- Color escape: ~20 bytes (`\033[38;2;R;G;Bm\033[48;2;R;G;Bm`)
- Cell clear: 4 bytes (`\033[8X`)
- Cell content: ~8 bytes (`3.3V\t` or `123.4mA\t`)
- Color reset: ~4 bytes (`\033[0m`)
- Per cell total: ~36 bytes × 8 pins = ~288 bytes per row

Three rows × 288 = ~864 bytes. Plus cursor save/restore/positioning from the statusbar framing ≈ 50 bytes. Total ≈ 914 bytes — fits in 1024 with room to spare.

For the Core0 `tmp` buffer, a single row at ~288 bytes means `char tmp[320]` is safe. Using `char tmp[512]` gives comfortable headroom.

## Validation checklist

After implementing, verify:
- [ ] `ui_pin_render.c` contains zero `printf` / `vprintf` calls
- [ ] `ui_pin_render.c` contains zero `if (buf)` conditionals
- [ ] `v` command shows correct voltages on all pins (single and continuous)
- [ ] `v` command shows correct current mA when PSU is on
- [ ] `v` command shows `-` for current when PSU is off
- [ ] Statusbar updates voltages when they change
- [ ] Statusbar skips unchanged cells (verify with terminal logging or debug output)
- [ ] Statusbar and `v` command work simultaneously (statusbar enabled + `V` continuous)
- [ ] Freq and PWM values display correctly with proper unit labels
- [ ] GND pins show `GND` text
- [ ] No build warnings from `ui_pin_render.c`
- [ ] Flash size hasn't grown significantly (should shrink slightly — removed printf overhead)
