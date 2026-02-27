# VT100 & Toolbar System — Post-Refactor Status

**Updated: 2026-02-24** — After Unified Buffer Builder refactor

---

## 1. Architecture Overview

### Dual-Core TX Model

The RP2040 has two cores with separate TX mechanisms:

| | Core0 (commands) | Core1 (USB service loop) |
|---|---|---|
| **TX path** | `tx_fifo_write(buf, len)` → SPSC ring buffer (1024 bytes) → Core1 drains to USB | `snprintf` → `tx_sb_buf[1024]` → `tx_sb_start(len)` → state machine drains to USB |
| **Constraint** | `tx_fifo_write()` / `tx_fifo_put()` assert `BP_ASSERT_CORE0()` | `tx_sb_start()` asserts `BP_ASSERT_CORE1()` |
| **When it runs** | On-demand when user types `v`, `V`, `i`, etc. | Periodically in `core1_infinite_loop()` after `monitor()` detects changes |

### Unified Buffer Builder (`ui_pin_render.c`)

All pin rendering (names, labels, voltages) always writes into a caller-provided buffer via `snprintf` — there is no `printf` path and no `if (buf)` branching. Behaviour is controlled through `pin_render_flags_t` flags:

```c
typedef enum {
    PIN_RENDER_CHANGE_TRACK  = (1u << 0),  // skip unchanged cells (emit bare \t)
    PIN_RENDER_NEWLINE       = (1u << 1),  // append \r\n at end of row
    PIN_RENDER_CLEAR_CELLS   = (1u << 2),  // prepend \033[8X before each cell
} pin_render_flags_t;
```

| Flag | Statusbar (Core1) | `v`/`V` command (Core0) |
|---|---|---|
| `CHANGE_TRACK` | ✅ Yes — skip unchanged cells | ❌ No — always repaint |
| `NEWLINE` | ❌ No — cursor positioned externally | ✅ Yes — needs `\r\n` |
| `CLEAR_CELLS` | ✅ Yes | ✅ Yes |

The builder reads raw data directly — no dependency on `system_monitor.h`:
- Voltages: `*hw_pin_voltage_ordered[i]` (millivolts)
- Current: `hw_adc_raw[HW_ADC_CURRENT_SENSE]` (raw ADC → computed milliamps)
- Freq/PWM: `freq_display_hz()`

Change tracking uses a static shadow buffer (`shadow_voltage_mv[]`, `shadow_current_raw`) — only written when `CHANGE_TRACK` is set, which is Core1-only. No lock needed.

---

## 2. What Was Done

### Phase 1: VT100 Primitive API ✅ Complete

**Naming convention:** bare names use `printf` internally (the common path); `_buf()` variants write into a caller-provided buffer (less common, used by statusbar/pin-render).

Printf-based helpers in `ui_term.c`:

| Function | Status |
|----------|--------|
| `ui_term_erase_line()` | ✅ |
| `ui_term_screen_flash(bool)` | ✅ |
| `ui_term_cursor_position(row,col)` | ✅ |
| `ui_term_cursor_save()` | ✅ |
| `ui_term_cursor_restore()` | ✅ |
| `ui_term_scroll_region(top,bot)` | ✅ |
| `ui_term_line_wrap_disable()` | ✅ |
| `ui_term_cursor_move_down(n)` | ✅ |
| `ui_term_cursor_move_right(n)` | ✅ |
| `ui_term_cursor_move_left(n)` | ✅ |

Buffer variants (`_buf`) in `ui_term.c` — each returns `uint32_t` bytes written:

| Function | Status |
|----------|--------|
| `ui_term_cursor_position_buf(buf, len, row, col)` | ✅ |
| `ui_term_cursor_save_buf(buf, len)` | ✅ |
| `ui_term_cursor_restore_buf(buf, len)` | ✅ |
| `ui_term_cursor_hide_buf(buf, len)` | ✅ |
| `ui_term_cursor_show_buf(buf, len)` | ✅ |
| `ui_term_erase_line_buf(buf, len)` | ✅ |
| `ui_term_erase_chars_buf(buf, len, n)` | ✅ |
| `ui_term_scroll_region_buf(buf, len, top, bot)` | ✅ |

### Phase 1: Table-Driven Colors ✅ Complete

Nine identical `ui_term_color_*()` switch functions replaced by a single `ui_term_color(ui_color_id_t id)` lookup with old-name wrappers preserved.

### Phase 2: Toolbar Registry ✅ Complete

Files: `src/ui/ui_toolbar.c`, `src/ui/ui_toolbar.h`. Provides `toolbar_register/unregister`, `toolbar_total_height`, `toolbar_scroll_bottom`, `toolbar_get_start_row`, `toolbar_apply_scroll_region`, `toolbar_redraw_all`.

### Phase 3: Statusbar + Logic Bar Migration ✅ Complete

Both register as `toolbar_t` and use the toolbar registry for positioning. Logic bar still has raw escape sequences (see §3).

### Phase 4: Unified Pin Rendering → Unified Buffer Builder ✅ Complete

`ui_pin_render.c` and `ui_pin_render.h` provide three buffer-only functions:
- `ui_pin_render_names(buf, len, flags)`
- `ui_pin_render_labels(buf, len, flags)`
- `ui_pin_render_values(buf, len, flags)`

Key properties:
- **Zero `printf`/`vprintf` calls** — everything is `snprintf` into caller's buffer
- **Zero `if (buf)` branches** — buffer is always non-NULL
- **Zero `monitor_get_voltage_ptr` / `monitor_get_current_ptr` calls** — reads raw ADC data directly
- **No dependency on `system_monitor.h`**

`ui_info.c` is a 3-function wrapper: builds into `char tmp[512]`, sends via `tx_fifo_write()`.

`ui_statusbar.c` passes `PIN_RENDER_CHANGE_TRACK | PIN_RENDER_CLEAR_CELLS` and builds directly into `tx_sb_buf`.

### New: `tx_fifo_write()` helper

Added to `usb_tx.c/.h` — pushes a pre-built buffer through the SPSC queue byte-by-byte. Asserts `BP_ASSERT_CORE0()`. Used by `ui_info.c` wrappers so Core0 never calls `printf` for pin rendering.

---

## 3. Remaining Raw Escape Sequences

| File | Count | Types | Notes |
|------|-------|-------|-------|
| `src/toolbars/logic_bar.c` | **0** | — | ✅ Fully migrated to bare-name wrappers |
| `src/ui/ui_statusbar.c` | **0** | — | ✅ Fully migrated to `_buf()` calls |
| `src/ui/ui_pin_render.c` | **0** | — | ✅ Fully migrated to `ui_term_erase_chars_buf()` |

**All toolbar/rendering files are now free of raw VT100 escape sequences.**

---

## 4. Issues & Concerns

### 4.1 ~~No `_buf()` Variants~~ ✅ Resolved

Eight `_buf()` variants added to `ui_term.c`/`ui_term.h`. All raw escape sequences in `ui_statusbar.c` and `ui_pin_render.c` have been migrated.

### 4.2 Toolbar `.draw` Callbacks are NULL

Both `statusbar_toolbar.draw` and `logic_bar_toolbar.draw` are `NULL`. `toolbar_redraw_all()` is a no-op. Fine for now but prevents full-screen redraw on terminal resize.

### 4.3 ~~Logic Bar Detach Scroll Region Fragility~~ ✅ Resolved

`logic_bar_detach()` now calls `toolbar_unregister()` first, then uses `toolbar_scroll_bottom()` to get the correct restored value. `logic_bar_hide()` / `logic_bar_show()` properly unregister/re-register the toolbar.

### 4.4 `ui_info_print_toolbar()` — Dead Declaration

`ui_info.h` declares `ui_info_print_toolbar()` but no definition exists.

### 4.5 `voltages_value` Bounds Safety

`voltages_value` has `HW_PINS - 1` entries (9). `ui_pin_render_values()` iterates all `HW_PINS` (10). The 10th (GND) is caught before the `default` case, but any future pin type at that index could OOB.

### 4.6 Status Bar TX Race (Unchanged)

The `STATUSBAR_DELAY` state still waits only one cycle. No new protections added.

---

## 5. File Change Summary

| File | Change |
|------|--------|
| `src/ui/ui_term.c` | 11 VT100 printf helpers (bare names) + 8 `_buf()` variants; table-driven color lookup |
| `src/ui/ui_term.h` | `ui_color_id_t` enum; printf + `_buf()` function declarations |
| `src/ui/ui_toolbar.c` | **NEW** — Toolbar registry + layout manager |
| `src/ui/ui_toolbar.h` | **NEW** — `toolbar_t` struct and registry API |
| `src/ui/ui_pin_render.c` | **NEW** — Buffer-only unified pin rendering with `pin_render_flags_t` flags, shadow-based change tracking, reads raw ADC directly; **zero raw escape sequences** |
| `src/ui/ui_pin_render.h` | **NEW** — `pin_render_flags_t` enum + 3-arg function signatures |
| `src/ui/ui_info.c` | 3 thin wrappers: build into `char tmp[512]`, send via `tx_fifo_write()` |
| `src/ui/ui_statusbar.c` | Registers as `toolbar_t`; delegates pin rows to `ui_pin_render_*(buf, len, flags)` with `CHANGE_TRACK \| CLEAR_CELLS`; **zero raw escape sequences** |
| `src/usb_tx.c` / `src/usb_tx.h` | Added `tx_fifo_write(const char* buf, uint32_t len)` — Core0 buffer-send helper |
| `src/toolbars/logic_bar.c` | Registers as `toolbar_t`; uses `toolbar_get_start_row()` exclusively; `draw_get_position_index()` removed; detach ordering fixed; **zero raw escape sequences** |

---

## 6. Remaining Migration Work

### VT100 Naming Convention Sweep ✅ Complete
1. ~~Rename all existing `_printf` functions to bare names~~ — Done (11 functions, ~12 call sites)
2. Convention: bare name = `printf` internally; `_buf()` = writes into caller buffer

### VT100 `_buf()` Variants ✅ Complete
1. ~~Add `_buf()` variants of all VT100 primitives to `ui_term.c`~~ — Done (8 functions)
2. ~~Migrate `ui_statusbar.c` raw sequences to `_buf()` variants~~ — Done (10 raw escapes → 0)
3. ~~Migrate `ui_pin_render.c` erase-chars to `ui_term_erase_chars_buf()`~~ — Done (8 raw escapes → 0)

### Logic Bar Full Migration ✅ Complete
1. ~~Replace ~21 raw escape sequences in `logic_bar.c` with bare-name wrappers~~ — Done (21 raw escapes → 0)
2. ~~Remove `draw_get_position_index()`~~ — Done, uses `toolbar_get_start_row()` exclusively
3. ~~Fix detach scroll-region fragility~~ — Done, unregister-first ordering; `hide`/`show` properly unregister/re-register

### TX Hardening
1. Strengthen `STATUSBAR_DELAY` (multi-cycle quiet window or lock)
2. Test under heavy output load

### Cleanup
1. Remove dead `ui_info_print_toolbar()` declaration from `ui_info.h`
2. Wire up `toolbar_t.draw` callbacks so `toolbar_redraw_all()` works
3. Add bounds check for `voltages_value` access in `system_monitor.c`
