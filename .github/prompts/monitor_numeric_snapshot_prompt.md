# Monitor Refactor: Numeric Snapshot Architecture — Implementation Prompt

## Purpose of this document

This document describes a refactor of the **system monitor** (`system_monitor.c/.h`). A
prior refactor already decoupled the VT100 statusbar and `v`/`V` command from the
monitor's ASCII formatting APIs. The VT100 path now reads raw ADC values directly and
does its own change tracking with consumer-side shadow buffers. This document specifies
how to complete the transition: simplify the monitor to be a thin numeric-only ADC cache
and migrate the remaining consumer (LCD) to the same pattern.

## What changed in the statusbar/v-command refactor

### Before (old architecture)

The monitor converted ADC → ASCII on every cycle, stored pre-formatted strings, and
exposed per-character accessors. Both VT100 and LCD consumers called these accessors.
`ui_pin_render.c` contained `if (buf)` / `else` forks to serve printf (core0) and
buffer (core1) paths through the same functions.

### After (current state)

`ui_pin_render.c` was rewritten as a **buffer-only builder** controlled by
`pin_render_flags_t` flags. Key changes:

1. **No printf path.** All rendering writes to a caller-provided buffer via `snprintf()`.
   Core0 (`v` command via `ui_info.c`) passes a stack-local buffer and pushes it
   through `tx_fifo_write()`. Core1 (statusbar) receives a buffer slice from the
   toolbar state machine (`toolbar_core1_service()`) and returns the bytes written;
   the state machine handles `tx_tb_start()` and drain.

2. **Reads raw ADC directly.** Voltages come from `*hw_pin_voltage_ordered[i]`
   (millivolts). Current comes from `hw_adc_raw[HW_ADC_CURRENT_SENSE]` with inline
   conversion `(raw >> 1) * (500000 / 2048)`. No monitor ASCII APIs are called.

3. **Consumer-side shadow buffers.** `ui_pin_render.c` maintains its own static shadows:
   ```c
   static uint16_t shadow_voltage_mv[HW_PINS];
   static uint32_t shadow_current_raw;
   ```
   When the `PIN_RENDER_CHANGE_TRACK` flag is set (core1 statusbar only), the builder
   compares current values against these shadows and emits a bare `\t` for unchanged
   cells. Only the core1 statusbar uses this flag — no lock needed.

4. **No `monitor_get_*` calls from VT100 rendering.** `ui_pin_render.c` does not
   `#include "system_monitor.h"`. `ui_statusbar.c` still includes it but only for
   `monitor_force_update()` (called from `statusbar_update_core1_cb()` when
   `UI_UPDATE_INFOBAR` triggers a full repaint).

## Current monitor state — what still exists

### Files and their monitor API usage

| File | Monitor APIs still called | Role |
|---|---|---|
| `src/system_monitor.c` | (defines all) | ADC → ASCII conversion, per-digit change masks |
| `src/system_monitor.h` | (declares all) | Full API surface: 10 functions |
| `src/pirate.c` (core1 loop) | `monitor()`, `monitor_voltage_changed()`, `monitor_current_changed()`, `monitor_reset()` | Orchestrator: calls monitor, builds `update_flags`, dispatches to consumers |
| `src/ui/ui_lcd.c` | `monitor_get_voltage_char()`, `monitor_get_current_char()` | LCD per-digit rendering — the **only remaining ASCII API consumer** |
| `src/ui/ui_statusbar.c` | `monitor_force_update()` | Sledgehammer full-repaint trigger on `UI_UPDATE_INFOBAR` |
| `src/ui/ui_pin_render.c` | **None** | Reads `hw_pin_voltage_ordered[]` and `hw_adc_raw[]` directly |
| `src/ui/ui_info.c` (`v` command) | **None** | Calls `amux_sweep()` then `ui_pin_render_*()` with stack buffer |

### Current data flow

```
hw_adc_raw[] / hw_pin_voltage_ordered[]
        │
        ▼
   amux_sweep()              ← reads ADC hardware
        │
        ├─────────────────────────────────────────────────┐
        ▼                                                 ▼
   monitor()                                    ui_pin_render_values()
   ├─ converts raw → ASCII chars                ├─ reads raw values directly
   ├─ diffs per-digit into update masks         ├─ diffs against shadow_voltage_mv[]
   ├─ stores voltages_value[pin][4]             └─ formats only changed pins
   └─ stores current_value[6]                        │
        │                                            │
   core1 loop                                        │
   ├─ queries monitor_voltage_changed()              │
   ├─ queries monitor_current_changed()              │
   ├─ checks system_config.pin_changed               │
   ├─ checks system_config.info_bar_changed          │
   └─ combines into update_flags (UI_UPDATE_*)       │
        │                                            │
        ├──────────────────┐                         │
        ▼                  ▼                         │
   ui_lcd_update()    toolbar_core1_begin_update()
   (per-digit via         │  → toolbar_core1_service()
    monitor_get_*_char()) │  → statusbar_update_core1_cb()
                          └─ calls ui_pin_render_*() ◄──┘
```

**Key observation:** `monitor()` now does redundant work. It converts every ADC sample
to ASCII and tracks per-digit changes, but the only consumer of that work is the LCD.
The VT100 path does its own numeric diffing against `hw_pin_voltage_ordered[]`. Both
paths call `amux_sweep()` (monitor calls it; the `v` command also calls it
independently on core0).

### Remaining problems

1. **Redundant ASCII conversion.** `monitor()` converts all voltages to ASCII characters
   every cycle. Only `ui_lcd.c` reads these ASCII strings. The VT100 path ignores them
   entirely, yet the conversion still runs.

2. **Bug in current sense comparison (still present).** `bool current_sense =
   psu_status.enabled` (0 or 1) is compared against `current_sense_temp` (milliamp
   uint32_t). The comparison is almost always true, so snprintf + digit-diff runs every
   cycle. The VT100 path avoids this by comparing `hw_adc_raw` directly, but the LCD
   path still consumes the buggy output.

3. **Parallel change-tracking systems.** Voltage change detection now exists in two
   places:
   - `system_monitor.c`: `voltages_update_mask[3]` / `current_update_mask` (for LCD)
   - `ui_pin_render.c`: `shadow_voltage_mv[]` / `shadow_current_raw` (for VT100)

   Both do essentially the same job. The monitor's version has the confusing 3-element
   array (index 0 = ones digit, index 1 = unused decimal point, index 2 = tenths).

4. **`update_flags` built from monitor queries, but VT100 doesn't need them for
   voltage/current.** The core1 loop calls `monitor_voltage_changed()` and
   `monitor_current_changed()` to set `UI_UPDATE_VOLTAGES` and `UI_UPDATE_CURRENT` in
   `update_flags`. These flags gate whether `statusbar_update_core1_cb()` renders
   the voltage/current rows at all. But `ui_pin_render_values()` has its own change
   detection — if called with `PIN_RENDER_CHANGE_TRACK`, it returns 0 when nothing
   changed. The `update_flags` gating is redundant for VT100 and can cause missed
   updates (e.g. if `ui_pin_render.c`'s shadow detects a change that monitor's mask
   missed, or vice versa).

5. **`monitor_force_update()` in statusbar is a cross-system side-effect.**
   `statusbar_update_core1_cb()` calls `monitor_force_update()` when
   `UI_UPDATE_INFOBAR` is set. This sets all monitor masks to `0xffffffff`, which
   forces the LCD to repaint everything too. The intent is to force `ui_pin_render.c`
   to redraw all cells, but `ui_pin_render.c` doesn't read monitor masks — it has its
   own shadows. The actual effect is an unnecessary LCD full-repaint.

6. **Dirty flags still scattered in `system_config`.** `system_config.pin_changed` and
   `system_config.info_bar_changed` are set by core0 commands and cleared by
   `monitor_reset()` on core1. Both `ui_pin_render.c` and `ui_lcd.c` read
   `system_config.pin_changed` directly for label/function changes.

## Refactor plan

### Goal

Replace `system_monitor.c` with a thin numeric ADC cache. Move the LCD to the same
consumer-side shadow pattern already used by `ui_pin_render.c`. Consolidate dirty flags.

### Phase 1: Simplify monitor to numeric-only

Replace `system_monitor.c` with a minimal implementation:

```c
// ---- system_monitor.h (new) ----

#include <stdint.h>
#include <stdbool.h>
#include "pirate.h"  // for HW_PINS

/**
 * @brief Read-only snapshot of current measurements.
 *
 * Updated by monitor_update().  Consumers compare against their own
 * shadow copies to detect changes — the monitor does NOT track dirty
 * state per-consumer.
 */
typedef struct {
    uint16_t voltage_mv[HW_PINS];  ///< Per-pin voltage in millivolts
    uint32_t current_raw;          ///< Raw ADC count for current sense
    uint32_t current_ua;           ///< Current in microamps (derived)
} monitor_snapshot_t;

/// Read ADC (amux_sweep), update snapshot.  Returns true if any value changed.
bool monitor_update(void);

/// Get pointer to current snapshot (valid until next monitor_update).
const monitor_snapshot_t* monitor_get_snapshot(void);

/// Initialise snapshot to zeros.
void monitor_init(void);
```

Implementation:

```c
// ---- system_monitor.c (new) ----

#include "system_monitor.h"
#include "pirate/amux.h"
#include "display/scope.h"

static monitor_snapshot_t snapshot;

void monitor_init(void) {
    memset(&snapshot, 0, sizeof(snapshot));
}

const monitor_snapshot_t* monitor_get_snapshot(void) {
    return &snapshot;
}

bool monitor_update(void) {
    if (scope_running) return false;

    amux_sweep();

    bool changed = false;
    for (uint8_t i = 0; i < HW_PINS; i++) {
        uint16_t mv = (uint16_t)(*hw_pin_voltage_ordered[i]);
        if (mv != snapshot.voltage_mv[i]) {
            snapshot.voltage_mv[i] = mv;
            changed = true;
        }
    }

    uint32_t raw = hw_adc_raw[HW_ADC_CURRENT_SENSE];
    if (raw != snapshot.current_raw) {
        snapshot.current_raw = raw;
        snapshot.current_ua = ((raw >> 1) * ((500 * 1000) / 2048));
        changed = true;
    }

    return changed;
}
```

**What gets removed:**
- `voltages_value[HW_PINS-1][4]` (ASCII string storage)
- `voltages_update_mask[3]` (per-digit bitmasks)
- `current_value[6]` (ASCII string)
- `current_update_mask` (per-digit bitmask)
- `monitor_get_voltage_char()`, `monitor_get_voltage_ptr()`
- `monitor_get_current_char()`, `monitor_get_current_ptr()`
- `monitor_voltage_changed()`, `monitor_current_changed()`
- `monitor_clear_voltage()`, `monitor_clear_current()`
- `monitor_reset()`, `monitor_force_update()`

### Phase 2: Migrate LCD to consumer-side shadows

`ui_lcd.c` currently calls `monitor_get_voltage_char(pin, digit, &c)` to get one ASCII
character at a time and uses the return value (true = changed) to decide whether to
repaint that digit position.

Migrate to the same pattern `ui_pin_render.c` already uses:

```c
// In ui_lcd.c — new static shadows
static uint16_t lcd_shadow_voltage_mv[HW_PINS];
static uint32_t lcd_shadow_current_raw;

// In the voltage rendering loop:
const monitor_snapshot_t* snap = monitor_get_snapshot();
for (int i = 0; i < HW_PINS - 1; i++) {
    uint16_t mv = snap->voltage_mv[i];
    uint16_t old = lcd_shadow_voltage_mv[i];
    if (mv == old && !(update_flags & UI_UPDATE_FORCE)) continue;

    // Per-digit diffing for minimal SPI writes
    uint8_t new_ones  = (mv / 1000);
    uint8_t old_ones  = (old / 1000);
    if (new_ones != old_ones || (update_flags & UI_UPDATE_FORCE)) {
        char c[2] = { new_ones + '0', 0 };
        lcd_write_labels(left_margin, top_margin, font, color, c, 0);
    }

    uint8_t new_tenth = (mv % 1000) / 100;
    uint8_t old_tenth = (old % 1000) / 100;
    if (new_tenth != old_tenth || (update_flags & UI_UPDATE_FORCE)) {
        char c[2] = { new_tenth + '0', 0 };
        lcd_write_labels(left_margin_skip_two, top_margin, font, color, c, 0);
    }

    lcd_shadow_voltage_mv[i] = mv;
}
```

Similarly for current:

```c
uint32_t raw = snap->current_raw;
if (raw != lcd_shadow_current_raw || (update_flags & UI_UPDATE_FORCE)) {
    uint32_t ua = snap->current_ua;
    // per-digit rendering with local diffing...
    lcd_shadow_current_raw = raw;
}
```

**Key difference from VT100:** The LCD consumer does per-digit diffing of its own
shadows — same numeric comparison, but it decides at the digit level whether to issue
an SPI write. This is the LCD's concern, not the monitor's.

### Phase 3: Consolidate dirty flags

Move `pin_changed` and `info_bar_changed` out of `system_config` and into a dedicated
dirty-flag struct with setter functions:

```c
// ---- monitor_dirty.h (or add to system_monitor.h) ----

typedef struct {
    uint32_t pin_config;    ///< Bitmask: which pins' label/function changed
    bool     info_bar;      ///< PSU/pullup/scope status changed
} monitor_dirty_t;

/// Called by commands (core0) to mark a pin's config as changed.
void monitor_mark_pin_changed(uint8_t pin);

/// Called by commands (core0) to mark the info bar as changed.
void monitor_mark_info_changed(void);

/// Called by core1 after dispatching updates.  Returns the flags and clears them.
monitor_dirty_t monitor_consume_dirty(void);
```

Implementation uses atomic-safe word writes (naturally aligned uint32_t on Cortex-M0+):

```c
static volatile monitor_dirty_t dirty;

void monitor_mark_pin_changed(uint8_t pin) {
    dirty.pin_config |= (1u << pin);
}

void monitor_mark_info_changed(void) {
    dirty.info_bar = true;
}

monitor_dirty_t monitor_consume_dirty(void) {
    monitor_dirty_t d = dirty;
    dirty.pin_config = 0;
    dirty.info_bar = false;
    return d;
}
```

**Callers to update** (currently write `system_config.pin_changed` / `.info_bar_changed`):
- `src/system_config.c` → `system_config_set_pin_func()` sets `pin_changed |= (1 << pin)`
- `src/commands/global/w_psu.c` → sets `pin_changed = 0xff` and `info_bar_changed = true`
- `src/commands/global/p_pullups.c` → sets `info_bar_changed = true`
- `src/display/scope.c` → sets `info_bar_changed = 1` in multiple places

### Phase 4: Simplify core1 loop

The orchestration in `pirate.c` `core1_infinite_loop()` simplifies because
`monitor_update()` returns a single bool and dirty flags are consumed atomically:

```c
// Current core1 loop (to be replaced):
//   monitor();
//   uint32_t update_flags = 0;
//   if (lcd_update_force) { ... }
//   if (system_config.pin_changed)       update_flags |= UI_UPDATE_LABELS | UI_UPDATE_VOLTAGES;
//   if (monitor_voltage_changed())       update_flags |= UI_UPDATE_VOLTAGES;
//   if (psu_status.enabled && monitor_current_changed()) update_flags |= UI_UPDATE_CURRENT;
//   if (system_config.info_bar_changed)  update_flags |= UI_UPDATE_INFOBAR;
//   ... dispatch ...
//   monitor_reset();

// New:
bool adc_changed = monitor_update();
monitor_dirty_t cfg = monitor_consume_dirty();
uint32_t update_flags = 0;

if (lcd_update_force) {
    lcd_update_force = false;
    update_flags |= UI_UPDATE_FORCE | UI_UPDATE_ALL;
}
if (cfg.pin_config)  update_flags |= UI_UPDATE_LABELS | UI_UPDATE_VOLTAGES;
if (adc_changed)     update_flags |= UI_UPDATE_VOLTAGES | UI_UPDATE_CURRENT;
if (cfg.info_bar)    update_flags |= UI_UPDATE_INFOBAR;

// Dispatch to LCD (uses snapshot + own shadows for per-digit work)
if (!system_config.lcd_screensaver_active) {
    displays[system_config.display].display_lcd_update(update_flags);
}

// Dispatch to VT100 toolbars (statusbar uses ui_pin_render with PIN_RENDER_CHANGE_TRACK)
if (system_config.terminal_ansi_color &&
    toolbar_count_registered() &&
    !system_config.terminal_toolbar_pause) {
    toolbar_core1_begin_update(update_flags);
}
```

**Note:** `update_flags` now sets `UI_UPDATE_VOLTAGES | UI_UPDATE_CURRENT` together
when `adc_changed` is true. The VT100 path's own `PIN_RENDER_CHANGE_TRACK` shadows
filter out cells that didn't actually change, so over-signaling is harmless. The LCD
path does its own per-digit comparison, so it also filters naturally.

### Phase 5: Remove `monitor_force_update()` from statusbar

`statusbar_update_core1_cb()` in `ui_statusbar.c` currently calls
`monitor_force_update()` when `UI_UPDATE_INFOBAR` is set. This was needed when the
statusbar used `monitor_get_*` APIs for change gating. Now that `ui_pin_render.c` has
its own shadows, the statusbar should instead reset its own shadows to force a full
repaint:

```c
// In ui_pin_render.c — add a reset function:
void ui_pin_render_reset_shadows(void) {
    memset(shadow_voltage_mv, 0xFF, sizeof(shadow_voltage_mv));  // force mismatch
    shadow_current_raw = UINT32_MAX;
}
```

Then in `statusbar_update_core1_cb()` (in `ui_statusbar.c`):

```c
if (update_flags & UI_UPDATE_INFOBAR) {
    ui_pin_render_reset_shadows();  // force full repaint of pin rows
    // ... render info bar ...
}
```

This replaces the `monitor_force_update()` call and eliminates the cross-system
side effect on LCD.

## Summary of API changes

### Removed (dead after migration)

| Function | Was used by | Replacement |
|---|---|---|
| `monitor()` | core1 loop | `monitor_update()` |
| `monitor_get_voltage_char()` | `ui_lcd.c` | Snapshot + LCD-local shadow per-digit diff |
| `monitor_get_voltage_ptr()` | (nobody since statusbar refactor) | Removed |
| `monitor_get_current_char()` | `ui_lcd.c` | Snapshot + LCD-local shadow per-digit diff |
| `monitor_get_current_ptr()` | (nobody since statusbar refactor) | Removed |
| `monitor_voltage_changed()` | core1 loop | `monitor_update()` return value |
| `monitor_current_changed()` | core1 loop | `monitor_update()` return value |
| `monitor_reset()` | core1 loop | No longer needed (no monitor-side dirty state) |
| `monitor_force_update()` | `ui_statusbar.c` | `ui_pin_render_reset_shadows()` |
| `monitor_clear_voltage()` | `monitor_init()` | `monitor_init()` zeroes snapshot |
| `monitor_clear_current()` | `monitor_init()` | `monitor_init()` zeroes snapshot |

### Added

| Function | Purpose |
|---|---|
| `monitor_update()` | Read ADC, update numeric snapshot, return true if changed |
| `monitor_get_snapshot()` | Return pointer to current read-only snapshot |
| `monitor_mark_pin_changed()` | Core0 commands signal pin config change |
| `monitor_mark_info_changed()` | Core0 commands signal info bar change |
| `monitor_consume_dirty()` | Core1 atomically reads + clears config dirty flags |
| `ui_pin_render_reset_shadows()` | Force VT100 full repaint (replaces `monitor_force_update()`) |

### `system_config` fields removed

| Field | Replacement |
|---|---|
| `system_config.pin_changed` | `monitor_dirty_t.pin_config` via `monitor_mark_pin_changed()` |
| `system_config.info_bar_changed` | `monitor_dirty_t.info_bar` via `monitor_mark_info_changed()` |

## Implementation order and dependencies

1. **Phase 1 + 2 together** — Replace `system_monitor.c` and update `ui_lcd.c`. These
   are tightly coupled: removing the old ASCII APIs requires the LCD to stop calling them.
   `ui_pin_render.c` needs no changes (it already reads raw values).

2. **Phase 5** — Remove `monitor_force_update()` from `ui_statusbar.c`, add
   `ui_pin_render_reset_shadows()`. This can be done immediately after Phase 1 since
   `monitor_force_update()` will no longer exist.

3. **Phase 4** — Simplify the core1 loop in `pirate.c`. Straightforward once Phases 1
   and 5 are done.

4. **Phase 3** — Migrate dirty flags out of `system_config`. This touches more files
   (PSU, pullups, scope, system_config.c) and can be done as a separate commit. The
   system works correctly with dirty flags still in `system_config` — the migration is
   a cleanliness improvement, not a correctness fix.

## Files to modify

| File | Changes |
|---|---|
| `src/system_monitor.h` | Replace entire contents with snapshot API |
| `src/system_monitor.c` | Replace entire contents with ~30-line numeric implementation |
| `src/ui/ui_lcd.c` | Add local shadows, replace `monitor_get_*_char()` calls with snapshot reads |
| `src/ui/ui_statusbar.c` | Replace `monitor_force_update()` with `ui_pin_render_reset_shadows()`, remove `#include "system_monitor.h"` |
| `src/ui/ui_pin_render.c` | Add `ui_pin_render_reset_shadows()` function |
| `src/ui/ui_pin_render.h` | Declare `ui_pin_render_reset_shadows()` |
| `src/pirate.c` | Simplify core1 loop: `monitor_update()` + `monitor_consume_dirty()` replaces 5 separate queries |
| `src/system_config.h` | Remove `pin_changed` and `info_bar_changed` fields (Phase 3) |
| `src/system_config.c` | Replace `pin_changed \|=` with `monitor_mark_pin_changed()` (Phase 3) |
| `src/commands/global/w_psu.c` | Replace direct dirty-flag writes with setter calls (Phase 3) |
| `src/commands/global/p_pullups.c` | Replace direct dirty-flag writes with setter calls (Phase 3) |
| `src/display/scope.c` | Replace direct dirty-flag writes with setter calls (Phase 3) |

## Constraints

- **No changes to `ui_pin_render.c`'s rendering logic.** It already works correctly
  with raw ADC values and consumer-side shadows. Only add `ui_pin_render_reset_shadows()`.
- **LCD per-digit optimization must be preserved.** The LCD draws individual font
  glyphs via SPI. Each `lcd_write_labels()` call sets a bounding box and pushes pixels.
  Skipping unchanged digits avoids ~2KB of SPI traffic per digit. The consumer-side
  shadow pattern preserves this — it just moves the comparison from ASCII to numeric.
- **Both consumers run on core1 in the same loop iteration.** After `monitor_update()`,
  the snapshot is stable. LCD reads from the snapshot; VT100 reads from
  `hw_pin_voltage_ordered[]` (which `amux_sweep()` inside `monitor_update()` already
  refreshed). Both approaches see the same underlying data.
- **The `v` command runs on core0.** It calls `amux_sweep()` independently and never
  touches the monitor or its snapshot. No changes needed to `ui_info.c` or `v_adc.c`.
